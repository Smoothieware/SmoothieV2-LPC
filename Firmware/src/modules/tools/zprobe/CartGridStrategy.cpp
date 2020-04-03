/*

    Summary
    -------
    Probes grid_size points in X and Y (total probes grid_size * grid_size) and stores the relative offsets from the 0,0 Z height
    When enabled every move will calculate the Z offset based on interpolating the height offset within the grids nearest 4 points.

    Configuration
    -------------
    The strategy must be enabled in the config as well as zprobe.

      leveling-strategy.rectangular-grid.enable         true

    The size of the grid can be set with...
      leveling-strategy.rectangular-grid.size        7
    or
      leveling-strategy.rectangular-grid.grid_x_size        7
      leveling-strategy.rectangular-grid.grid_y_size        7
    this is the X and Y size of the grid, it must be an odd number, the default is 7 which is 49 probe points

    If both "size" and "grid_x_size" and "grid_x_size defined "grid_x_size" and "grid_x_size" will be used.
    If "grid_x_size" and "grid_x_size" omitted then "size" will be used.
    If "size" omitted default value will be used.

    I and J params used for grid size. If both omitted values from config will be used. If only one provided (I or J) then it will be used for both x_size and y-size.

    The width and length of the rectangle that is probed is set with...

      leveling-strategy.rectangular-grid.x_size       100
      leveling-strategy.rectangular-grid.y_size       90

   Optionally probe offsets from the nozzle or tool head can be defined with...

      leveling-strategy.rectangular-grid.probe_offsets  0,0,0  # probe offsetrs x,y,z

      they may also be set with M565 X0 Y0 Z0

    If the saved grid is to be loaded on boot then this must be set in the config...

      leveling-strategy.rectangular-grid.save        true

      Then when M500 is issued it will save M375 which will cause the grid to be loaded on boot. The default is to not autoload the grid on boot

    Optionally an initial_height can be set that tell the intial probe where to stop the fast decent before it probes, this should be around 5-10mm above the bed
      leveling-strategy.rectangular-grid.initial_height  10

    If two corners rectangular mode activated using "leveling-strategy.rectangular-grid.only_by_two_corners true" then G29/31/32 will not work without providing XYAB parameters
        XY - start point, AB rectangle size from starting point
        "Two corners"" not absolutely correct name for this mode, because it use only one corner and rectangle size.

    Display mode of current grid can be changed to human redable mode (table with coordinates) by using
       leveling-strategy.rectangular-grid.human_readable  true

    Usage
    -----
    G29 test probes a rectangle which defaults to the width and height, can be overidden with Xnnn and Ynnn

    G32 probes the grid and turns the compensation on, this will remain in effect until reset or M561/M370
        optional parameters {{Xn}} {{Yn}} sets the size for this rectangular probe, which gets saved with M375

    M370 clears the grid and turns off compensation
    M374 Save grid to /sd/cartesian.grid
    M374.1 delete /sd/cartesian.grid
    M375 Load the grid from /sd/cartesian.grid and enable compensation
    M375.1 display the current grid
    M561 clears the grid and turns off compensation
    M565 defines the probe offsets from the nozzle or tool head


    M500 saves the probe points
    M503 displays the current settings
*/

#include "CartGridStrategy.h"
#include "ConfigReader.h"
#include "Robot.h"
#include "main.h"
#include "GCode.h"
#include "Conveyor.h"
#include "ZProbe.h"
#include "Dispatcher.h"
#include "StepperMotor.h"
#include "BaseSolution.h"
#include "StringUtils.h"
#include "OutputStream.h"

#include <string>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <fastmath.h>

#define grid_x_size_key "grid_x_size"
#define grid_y_size_key "grid_y_size"
#define tolerance_key "tolerance"
#define save_key "save"
#define probe_offsets_key "probe_offsets"
#define initial_height_key "initial_height"
#define x_size_key "x_size"
#define y_size_key "y_size"
#define do_home_key "do_home"
#define only_by_two_corners_key "only_by_two_corners"
#define human_readable_key "human_readable"
#define height_limit_key "height_limit"
#define dampening_start_key "dampening_start"

#define GRIDFILE "/sd/cartesian.grid"

CartGridStrategy::CartGridStrategy(ZProbe *zprb) : ZProbeStrategy(zprb)
{
    grid = nullptr;
}

CartGridStrategy::~CartGridStrategy()
{
    if(grid != nullptr) free(grid);
}

bool CartGridStrategy::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("cartesian grid leveling strategy", m)) {
        printf("configure-cart-grid: no cart grid leveling strategy section found\n");
        return false;
    }

    this->current_grid_x_size = this->configured_grid_x_size = cr.get_float(m, grid_x_size_key, 7);
    this->current_grid_y_size = this->configured_grid_y_size = cr.get_float(m, grid_y_size_key, 7);
    tolerance = cr.get_float(m, tolerance_key, 0.03F);
    save = cr.get_bool(m, save_key, false);
    do_home = cr.get_bool(m, do_home_key, true);
    only_by_two_corners = cr.get_bool(m, only_by_two_corners_key, false);
    human_readable = cr.get_bool(m, human_readable_key, false);

    this->height_limit = cr.get_float(m, height_limit_key, 0);
    this->dampening_start = cr.get_float(m, dampening_start_key, 0);

    if(this->dampening_start > 0.001F && this->height_limit > this->dampening_start ) {
        this->damping_interval = height_limit - dampening_start;
    } else {
        this->damping_interval = 0;
    }

    this->x_start = 0.0F;
    this->y_start = 0.0F;
    this->x_size = cr.get_float(m, x_size_key, 0.0F);
    this->y_size = cr.get_float(m, y_size_key, 0.0F);
    if (this->x_size == 0.0F || this->y_size == 0.0F) {
        printf("configure-cart-grid: Invalid config, x_size and y_size must be defined\n");
        return false;
    }

    // the initial height above the bed we stop the intial move down after home to find the bed
    // this should be a height that is enough that the probe will not hit the bed
    this->initial_height = cr.get_float(m, initial_height_key, 10);

    // Probe offsets xxx,yyy,zzz
    {
        std::string po = cr.get_string(m, probe_offsets_key, "0,0,0");
        std::vector<float> v = stringutils::parse_number_list(po.c_str());
        if(v.size() >= 3) {
            this->probe_offsets = std::make_tuple(v[0], v[1], v[2]);
        }
    }

    // allocate
    grid = (float *)malloc(configured_grid_x_size * configured_grid_y_size * sizeof(float));

    if(grid == nullptr) {
        printf("configure-cart-grid: Not enough memory\n");
        return false;
    }

    reset_bed_level();

    // register mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    // M Code handlers
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 370, std::bind(&CartGridStrategy::handle_mcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 374, std::bind(&CartGridStrategy::handle_mcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 375, std::bind(&CartGridStrategy::handle_mcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 500, std::bind(&CartGridStrategy::handle_mcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 561, std::bind(&CartGridStrategy::handle_mcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 565, std::bind(&CartGridStrategy::handle_mcode, this, _1, _2));

    return true;
}

void CartGridStrategy::save_grid(OutputStream& os)
{
    if(only_by_two_corners){
        os.printf("error:Unable to save grid in only_by_two_corners mode\n");
        return;
    }

    if(grid[0] < -1E5F) {
        os.printf("error:No grid to save\n");
        return;
    }

    if((current_grid_x_size != configured_grid_x_size) || (current_grid_y_size != configured_grid_y_size)) {
        os.printf("error:Unable to save grid with size different from configured\n");
        return;
    }

    FILE *fp = fopen(GRIDFILE, "w");
    if(fp == NULL) {
        os.printf("error:Failed to open grid file %s\n", GRIDFILE);
        return;
    }
    uint8_t tmp_configured_grid_size = configured_grid_x_size;
    if(fwrite(&tmp_configured_grid_size, sizeof(uint8_t), 1, fp) != 1) {
        os.printf("error:Failed to write grid x size\n");
        fclose(fp);
        return;
    }

    tmp_configured_grid_size = configured_grid_y_size;
    if(configured_grid_y_size != configured_grid_x_size){
        if(fwrite(&tmp_configured_grid_size, sizeof(uint8_t), 1, fp) != 1) {
            os.printf("error:Failed to write grid y size\n");
            fclose(fp);
            return;
        }
    }

    if(fwrite(&x_size, sizeof(float), 1, fp) != 1)  {
        os.printf("error:Failed to write x_size\n");
        fclose(fp);
        return;
    }

    if(fwrite(&y_size, sizeof(float), 1, fp) != 1)  {
        os.printf("error:Failed to write y_size\n");
        fclose(fp);
        return;
    }

    for (int y = 0; y < configured_grid_y_size; y++) {
        for (int x = 0; x < configured_grid_x_size; x++) {
            if(fwrite(&grid[x + (configured_grid_x_size * y)], sizeof(float), 1, fp) != 1) {
                os.printf("error:Failed to write grid\n");
                fclose(fp);
                return;
            }
        }
    }
    os.printf("grid saved to %s\n", GRIDFILE);
    fclose(fp);
}

bool CartGridStrategy::load_grid(OutputStream& os)
{
    if(only_by_two_corners){
        os.printf("error:Unable to load grid in only_by_two_corners mode\n");
        return false;
    }

    FILE *fp = fopen(GRIDFILE, "r");
    if(fp == NULL) {
        os.printf("error:Failed to open grid %s\n", GRIDFILE);
        return false;
    }

    uint8_t load_grid_x_size, load_grid_y_size;
    float x, y;

    if(fread(&load_grid_x_size, sizeof(uint8_t), 1, fp) != 1) {
        os.printf("error:Failed to read grid size\n");
        fclose(fp);
        return false;
    }

    if(load_grid_x_size != configured_grid_x_size) {
        os.printf("error:grid size x is different read %d - config %d\n", load_grid_x_size, configured_grid_x_size);
        fclose(fp);
        return false;
    }

    load_grid_y_size = load_grid_x_size;

    if(configured_grid_x_size != configured_grid_y_size){
        if(fread(&load_grid_y_size, sizeof(uint8_t), 1, fp) != 1) {
            os.printf("error:Failed to read grid size\n");
            fclose(fp);
            return false;
        }

        if(load_grid_y_size != configured_grid_y_size) {
            os.printf("error:grid size y is different read %d - config %d\n", load_grid_y_size, configured_grid_x_size);
            fclose(fp);
            return false;
        }
    }

    if(fread(&x, sizeof(float), 1, fp) != 1) {
        os.printf("error:Failed to read grid x size\n");
        fclose(fp);
        return false;
    }

    if(fread(&y, sizeof(float), 1, fp) != 1) {
        os.printf("error:Failed to read grid y size\n");
        fclose(fp);
        return false;
    }

    if(x != x_size || y != y_size) {
        os.printf("error:bed dimensions changed read (%f, %f) - config (%f,%f)\n", x, y, x_size, y_size);
        fclose(fp);
        return false;
    }

    for (int yy = 0; yy < configured_grid_y_size; yy++) {
        for (int xx = 0; xx < configured_grid_x_size; xx++) {
            if(fread(&grid[xx + (configured_grid_x_size * yy)], sizeof(float), 1, fp) != 1) {
                os.printf("error:Failed to read grid\n");
                fclose(fp);
                return false;
            }
        }
    }
    os.printf("grid loaded, grid: (%f, %f), size: %d x %d\n", x_size, y_size, load_grid_x_size, load_grid_y_size);
    fclose(fp);
    return true;
}

bool CartGridStrategy::probe_grid(int n, int m, float _x_start, float _y_start, float _x_size, float _y_size, OutputStream& os)
{
    if((n < 5)||(m < 5)) {
        os.printf("Need at least a 5x5 grid to probe\n");
        return true;
    }


    if(!findBed()) return false;

    float x_step = _x_size / n;
    float y_step = _y_size / m;
    for (int c = 0; c < m; ++c) {
        std::string scanline;
        float y = _y_start + y_step * c;
        for (int r = 0; r < n; ++r) {
            float x = _x_start + x_step * r;
            float z = 0.0F;
            float mm;
            if(!zprobe->doProbeAt(mm, x, y)) return false;
            z = zprobe->getProbeHeight() - mm;
            char buf[16];
            size_t s= snprintf(buf, sizeof(buf), "%1.4f ", z);
            scanline.append(buf, s);
        }
        os.printf("%s\n", scanline.c_str());
    }
    return true;
}

bool CartGridStrategy::handle_gcode(GCode& gcode, OutputStream& os)
{
    if (gcode.get_code() == 29) { // do a probe to test flatness
        // first wait for an empty queue i.e. no moves left
        Conveyor::getInstance()->wait_for_idle();

        int n = gcode.has_arg('I') ? gcode.get_arg('I') : configured_grid_x_size;
        int m = gcode.has_arg('J') ? gcode.get_arg('J') : configured_grid_y_size;

        float _x_size = this->x_size, _y_size = this->y_size;
        float _x_start = this->x_start, _y_start = this->y_start;

        if(only_by_two_corners){
            if(gcode.has_arg('X') && gcode.has_arg('Y') && gcode.has_arg('A') && gcode.has_arg('B')){
                _x_start = gcode.get_arg('X'); // override default probe start point
                _y_start = gcode.get_arg('Y'); // override default probe start point
                _x_size = gcode.get_arg('A'); // override default probe width
                _y_size = gcode.get_arg('B'); // override default probe length
            } else {
                os.printf("In only_by_two_corners mode all XYAB parameters needed\n");
                return true;
            }
        } else {
            if(gcode.has_arg('X')) _x_size = gcode.get_arg('X'); // override default probe width
            if(gcode.has_arg('Y')) _y_size = gcode.get_arg('Y'); // override default probe length
        }

        probe_grid(n, m, _x_start, _y_start, _x_size, _y_size, os);

        return true;

    } else if( gcode.get_code() == 31 || gcode.get_code() == 32) { // do a grid probe
        // first wait for an empty queue i.e. no moves left
        Conveyor::getInstance()->wait_for_idle();

        if(!doProbe(gcode, os)) {
            os.printf("Probe failed to complete, check the initial probe height and/or initial_height settings\n");
        } else {
            os.printf("Probe completed\n");
        }
        return true;
    }

    return false;
}

bool CartGridStrategy::handle_mcode(GCode& gcode, OutputStream& os)
{
    if(gcode.get_code() == 370 || gcode.get_code() == 561) { // M370: Clear bed, M561: Set Identity Transform
        // delete the compensationTransform in robot
        setAdjustFunction(false);
        reset_bed_level();
        os.printf("grid cleared and disabled\n");
        return true;

    } else if(gcode.get_code() == 374) { // M374: Save grid, M374.1: delete saved grid
        if(gcode.get_subcode() == 1) {
            remove(GRIDFILE);
            os.printf("%s deleted\n", GRIDFILE);
        } else {
            save_grid(os);
        }

        return true;

    } else if(gcode.get_code() == 375) { // M375: load grid, M375.1 display grid
        if(gcode.get_subcode() == 1) {
            print_bed_level(os);
        } else {
            if(load_grid(os)) setAdjustFunction(true);
        }
        return true;

    } else if(gcode.get_code() == 565) { // M565: Set Z probe offsets
        float x = 0, y = 0, z = 0;
        if(gcode.has_arg('X')) x = gcode.get_arg('X');
        if(gcode.has_arg('Y')) y = gcode.get_arg('Y');
        if(gcode.has_arg('Z')) z = gcode.get_arg('Z');
        probe_offsets = std::make_tuple(x, y, z);
        return true;

    } else if(gcode.get_code() == 500) { // M500 save, M500.3 display only
        float x, y, z;
        std::tie(x, y, z) = probe_offsets;
        os.printf(";Probe offsets:\nM565 X%1.5f Y%1.5f Z%1.5f\n", x, y, z);
        if(save) {
            if(grid != nullptr && grid[0] > -1E5F) os.printf(";Load saved grid\nM375\n");
            else if(gcode.get_subcode() == 3) os.printf(";WARNING No grid to save\n");
        }
        return true;
    }

    return false;
}

#define X_PROBE_OFFSET_FROM_EXTRUDER std::get<0>(probe_offsets)
#define Y_PROBE_OFFSET_FROM_EXTRUDER std::get<1>(probe_offsets)
#define Z_PROBE_OFFSET_FROM_EXTRUDER std::get<2>(probe_offsets)

void CartGridStrategy::setAdjustFunction(bool on)
{
    if(on) {
        // set the compensationTransform in robot
        using std::placeholders::_1;
        using std::placeholders::_2;
        Robot::getInstance()->compensationTransform = std::bind(&CartGridStrategy::doCompensation, this, _1, _2); // [this](float *target, bool inverse) { doCompensation(target, inverse); };
    } else {
        // clear it
        Robot::getInstance()->compensationTransform = nullptr;
    }
}

bool CartGridStrategy::findBed()
{
    if (do_home) zprobe->home();
    float z = initial_height;
    zprobe->move_z(z, zprobe->getFastFeedrate()); // move Z only to initial_height
    zprobe->move_xy(x_start - X_PROBE_OFFSET_FROM_EXTRUDER, y_start - Y_PROBE_OFFSET_FROM_EXTRUDER, zprobe->getFastFeedrate()); // move at initial_height to x_start, y_start

    // find bed at 0,0 run at slow rate so as to not hit bed hard
    float mm;
    if(!zprobe->run_probe_return(mm, zprobe->getSlowFeedrate())) return false;

    // leave head probe_height above bed
    float dz = zprobe->getProbeHeight() - mm;
    zprobe->move_z(dz, zprobe->getFastFeedrate(), true); // relative move

    return true;
}

bool CartGridStrategy::doProbe(GCode& gcode, OutputStream& os)
{
    os.printf("Rectangular Grid Probe...\n");

    if(only_by_two_corners){
        if(gcode.has_arg('X') && gcode.has_arg('Y') && gcode.has_arg('A') && gcode.has_arg('B')){
            this->x_start = gcode.get_arg('X'); // override default probe start point, will get saved
            this->y_start = gcode.get_arg('Y'); // override default probe start point, will get saved
            this->x_size = gcode.get_arg('A'); // override default probe width, will get saved
            this->y_size = gcode.get_arg('B'); // override default probe length, will get saved
        } else {
            os.printf("In only_by_two_corners mode all XYAB parameters needed\n");
            return false;
        }
    } else {
        if(gcode.has_arg('X')) this->x_size = gcode.get_arg('X'); // override default probe width, will get saved
        if(gcode.has_arg('Y')) this->y_size = gcode.get_arg('Y'); // override default probe length, will get saved
    }

    setAdjustFunction(false);
    reset_bed_level();

    if(gcode.has_arg('I')) current_grid_x_size = gcode.get_arg('I'); // override default grid x size
    if(gcode.has_arg('J')) current_grid_y_size = gcode.get_arg('J'); // override default grid y size

    if((this->current_grid_x_size * this->current_grid_y_size)  > (this->configured_grid_x_size * this->configured_grid_y_size)){
        os.printf("Grid size (%d x %d = %d) bigger than configured (%d x %d = %d). Change configuration.\n",
                            this->current_grid_x_size, this->current_grid_y_size, this->current_grid_x_size*this->current_grid_x_size,
                            this->configured_grid_x_size, this->configured_grid_y_size, this->configured_grid_x_size*this->configured_grid_y_size);
        return false;
    }

    // find bed, and leave probe probe_height above bed
    if(!findBed()) {
        os.printf("Finding bed failed, check the initial height setting\n");
        return false;
    }

    os.printf("Probe start ht is %f mm, rectangular bed width %fmm, height %fmm, grid size is %dx%d\n", zprobe->getProbeHeight(), x_size, y_size, current_grid_x_size, current_grid_y_size);

    // do first probe for 0,0
    float mm;
    if(!zprobe->doProbeAt(mm, this->x_start - X_PROBE_OFFSET_FROM_EXTRUDER, this->y_start - Y_PROBE_OFFSET_FROM_EXTRUDER)) return false;
    float z_reference = zprobe->getProbeHeight() - mm; // this should be zero
    os.printf("probe at 0,0 is %f mm\n", z_reference);

    // probe all the points of the grid
    for (int yCount = 0; yCount < this->current_grid_y_size; yCount++) {
        float yProbe = this->y_start + (this->y_size / (this->current_grid_y_size - 1)) * yCount;
        int xStart, xStop, xInc;
        if (yCount % 2) {
            xStart = this->current_grid_x_size - 1;
            xStop = -1;
            xInc = -1;
        } else {
            xStart = 0;
            xStop = this->current_grid_x_size;
            xInc = 1;
        }

        for (int xCount = xStart; xCount != xStop; xCount += xInc) {
            float xProbe = this->x_start + (this->x_size / (this->current_grid_x_size - 1)) * xCount;

            if(!zprobe->doProbeAt(mm, xProbe - X_PROBE_OFFSET_FROM_EXTRUDER, yProbe - Y_PROBE_OFFSET_FROM_EXTRUDER)) return false;
            float measured_z = zprobe->getProbeHeight() - mm - z_reference; // this is the delta z from bed at 0,0
            os.printf("DEBUG: X%1.4f, Y%1.4f, Z%1.4f\n", xProbe, yProbe, measured_z);
            grid[xCount + (this->current_grid_x_size * yCount)] = measured_z;
        }
    }

    print_bed_level(os);

    setAdjustFunction(true);

    return true;
}

void CartGridStrategy::doCompensation(float *target, bool inverse)
{
    // Adjust print surface height by linear interpolation over the bed_level array.
    // offset scale: 1 for default (use offset as is)
    float scale = 1.0;
    if (this->damping_interval > 0.001F) {
        // if the height is below our compensation limit:
        if(target[Z_AXIS] <= this->height_limit) {
            // scale the offset as necessary:
            if(target[Z_AXIS] >= this->dampening_start) {
                scale = (1.0 - ((target[Z_AXIS] - this->dampening_start) / this->damping_interval));
            } // else leave scale at 1.0;
        } else {
            return; // if Z is higher than max, no compensation
        }
    }

    // find min/maxes, and handle the case where size is negative (assuming this is possible? Legacy code supported this)
    float min_x = std::min(this->x_start, this->x_start + this->x_size);
    float max_x = std::max(this->x_start, this->x_start + this->x_size);
    float min_y = std::min(this->y_start, this->y_start + this->y_size);
    float max_y = std::max(this->y_start, this->y_start + this->y_size);

    // clamp the input to the bounds of the compensation grid
    // if a point is beyond the bounds of the grid, it will get the offset of the closest grid point
    float x_target = std::min(std::max(target[X_AXIS], min_x), max_x);
    float y_target = std::min(std::max(target[Y_AXIS], min_y), max_y);

    float grid_x = std::max(0.001F, (x_target - this->x_start) / (this->x_size / (this->current_grid_x_size - 1)));
    float grid_y = std::max(0.001F, (y_target - this->y_start) / (this->y_size / (this->current_grid_y_size - 1)));
    int floor_x = floorf(grid_x);
    int floor_y = floorf(grid_y);
    float ratio_x = grid_x - floor_x;
    float ratio_y = grid_y - floor_y;
    float z1 = grid[(floor_x) + ((floor_y) * this->current_grid_x_size)];
    float z2 = grid[(floor_x) + ((floor_y + 1) * this->current_grid_x_size)];
    float z3 = grid[(floor_x + 1) + ((floor_y) * this->current_grid_x_size)];
    float z4 = grid[(floor_x + 1) + ((floor_y + 1) * this->current_grid_x_size)];
    float left = (1 - ratio_y) * z1 + ratio_y * z2;
    float right = (1 - ratio_y) * z3 + ratio_y * z4;
    float offset = (1 - ratio_x) * left + ratio_x * right;

    if (inverse) {
        target[Z_AXIS] -= offset * scale;
    } else {
        target[Z_AXIS] += offset * scale;
    }

    /*THEKERNEL->streams->printf("//DEBUG: TARGET: %f, %f, %f\n", target[0], target[1], target[2]);
     THEKERNEL->streams->printf("//DEBUG: grid_x= %f\n", grid_x);
     THEKERNEL->streams->printf("//DEBUG: grid_y= %f\n", grid_y);
     THEKERNEL->streams->printf("//DEBUG: floor_x= %d\n", floor_x);
     THEKERNEL->streams->printf("//DEBUG: floor_y= %d\n", floor_y);
     THEKERNEL->streams->printf("//DEBUG: ratio_x= %f\n", ratio_x);
     THEKERNEL->streams->printf("//DEBUG: ratio_y= %f\n", ratio_y);
     THEKERNEL->streams->printf("//DEBUG: z1= %f\n", z1);
     THEKERNEL->streams->printf("//DEBUG: z2= %f\n", z2);
     THEKERNEL->streams->printf("//DEBUG: z3= %f\n", z3);
     THEKERNEL->streams->printf("//DEBUG: z4= %f\n", z4);
     THEKERNEL->streams->printf("//DEBUG: left= %f\n", left);
     THEKERNEL->streams->printf("//DEBUG: right= %f\n", right);
     THEKERNEL->streams->printf("//DEBUG: offset= %f\n", offset);
     THEKERNEL->streams->printf("//DEBUG: scale= %f\n", scale);
     */
}


// Print calibration results for plotting or manual frame adjustment.
void CartGridStrategy::print_bed_level(OutputStream& os)
{
    if(!human_readable){
        for (int y = 0; y < current_grid_y_size; y++) {
            for (int x = 0; x < current_grid_x_size; x++) {
                os.printf("%1.4f ", grid[x + (current_grid_x_size * y)]);
            }
            os.printf("\n");
        }
    } else {

        int xStart = (x_size>0) ? 0 : (current_grid_x_size - 1);
        int xStop = (x_size>0) ? current_grid_x_size : -1;
        int xInc = (x_size>0) ? 1: -1;

        int yStart = (y_size<0) ? 0 : (current_grid_y_size - 1);
        int yStop = (y_size<0) ? current_grid_y_size : -1;
        int yInc = (y_size<0) ? 1: -1;

        for (int y = yStart; y != yStop; y += yInc) {
            os.printf("%10.4f|", y * (y_size / (current_grid_y_size - 1)));
            for (int x = xStart; x != xStop; x += xInc) {
                os.printf("%10.4f ",  grid[x + (current_grid_x_size * y)]);
            }
            os.printf("\n");
        }
        os.printf("           ");
        for (int x = xStart; x != xStop; x += xInc) {
            os.printf("-----+-----");
        }
        os.printf("\n");
        os.printf("           ");
        for (int x = xStart; x != xStop; x += xInc) {
            os.printf("%1.4f ",  x * (x_size / (current_grid_x_size - 1)));
        }
            os.printf("\n");

    }

}

// Reset calibration results to zero.
void CartGridStrategy::reset_bed_level()
{
    for (int y = 0; y < current_grid_y_size; y++) {
        for (int x = 0; x < current_grid_x_size; x++) {
            // set to very big negative number to indicate not set
            // then to be safe check against < -1E5F
            grid[x + (current_grid_x_size * y)] = -1e6F;
        }
    }
}
