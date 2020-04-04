/*
 This code is derived from (and mostly copied from) Johann Rocholls code at https://github.com/jcrocholl/Marlin/blob/deltabot/Marlin/Marlin_main.cpp
 license is the same as his code.

    Summary
    -------
    Probes grid_size points in X and Y (total probes grid_size * grid_size) and stores the relative offsets from the 0,0 Z height
    When enabled every move will calculate the Z offset based on interpolating the height offset within the grids nearest 4 points.

    Configuration
    -------------
    The strategy must be enabled in the config as well as zprobe.

      leveling-strategy.delta-grid.enable         true

    The radius of the bed must be specified with...

      leveling-strategy.delta-grid.radius        50

      this needs to be at least as big as the maximum printing radius as moves outside of this will not be compensated for correctly

    The size of the grid can be set with...

      leveling-strategy.delta-grid.size        7

      this is the X and Y size of the grid, it must be an odd number, the default is 7 which is 49 probe points

   Optionally probe offsets from the nozzle or tool head can be defined with...

      leveling-strategy.delta-grid.probe_offsets  0,0,0  # probe offsetrs x,y,z

      they may also be set with M565 X0 Y0 Z0

    If the saved grid is to be loaded on boot then this must be set in the config...

      leveling-strategy.delta-grid.save        true

      Then when M500 is issued it will save M375 which will cause the grid to be loaded on boot. The default is to not autoload the grid on boot

    Optionally an initial_height can be set that tell the intial probe where to stop the fast decent before it probes, this should be around 5-10mm above the bed
      leveling-strategy.delta-grid.initial_height  10


    Usage
    -----
    G29 test probes in a spiral pattern within the radius producing a map of offsets, this can be imported into a graphing program to visualize the bed heights
        optional parameters {{In}} sets the number of points to the value n, {{Jn}} sets the radius for this probe.

    G31 probes the grid and turns the compensation on, this will remain in effect until reset or M561/M370
        optional parameters {{Jn}} sets the radius for this probe, which gets saved with M375

    M370 clears the grid and turns off compensation
    M374 Save grid to /sd/delta.grid
    M374.1 delete /sd/delta.grid
    M375 Load the grid from /sd/delta.grid and enable compensation
    M375.1 display the current grid
    M561 clears the grid and turns off compensation
    M565 defines the probe offsets from the nozzle or tool head


    M500 saves the probe points
    M503 displays the current settings
*/

#include "DeltaGridStrategy.h"
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

#define grid_radius_key "radius"
#define grid_size_key "size"
#define tolerance_key "tolerance"
#define save_key "save"
#define probe_offsets_key "probe_offsets"
#define initial_height_key "initial_height"
#define do_home_key "do_home"

#define GRIDFILE "/sd/delta.grid"

DeltaGridStrategy::DeltaGridStrategy(ZProbe *zprb) : ZProbeStrategy(zprb)
{
    grid = nullptr;
}

DeltaGridStrategy::~DeltaGridStrategy()
{
    if(grid != nullptr) _RAM2->dealloc(grid);
}

bool DeltaGridStrategy::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("delta grid leveling strategy", m)) {
        printf("configure-delta-grid: no delta grid leveling strategy section found\n");
        return false;
    }

    grid_size = cr.get_float(m, grid_size_key, 7);
    tolerance = cr.get_float(m, tolerance_key, 0.03F);
    save = cr.get_bool(m, save_key, false);
    do_home = cr.get_bool(m, do_home_key, true);
    grid_radius = cr.get_float(m, grid_radius_key, 50.0F);

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

    // allocate memory in RAM2
    grid = (float *)_RAM2->alloc(grid_size * grid_size * sizeof(float));

    if(grid == nullptr) {
        printf("ERROR: config-deltagrid: Not enough memory for grid\n");
        return false;
    }

    reset_bed_level();


    // register mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    // M Code handlers
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 370, std::bind(&DeltaGridStrategy::handle_mcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 374, std::bind(&DeltaGridStrategy::handle_mcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 375, std::bind(&DeltaGridStrategy::handle_mcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 561, std::bind(&DeltaGridStrategy::handle_mcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 565, std::bind(&DeltaGridStrategy::handle_mcode, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 500, std::bind(&DeltaGridStrategy::handle_mcode, this, _1, _2));

    return true;
}

void DeltaGridStrategy::save_grid(OutputStream& os)
{
    if(grid[0] < -1e5F) {
        // very big negative number is unallocated
        os.printf("error:No grid to save\n");
        return;
    }

    FILE *fp = fopen(GRIDFILE, "w");
    if(fp == NULL) {
        os.printf("error:Failed to open grid file %s\n", GRIDFILE);
        return;
    }

    if(fwrite(&grid_size, sizeof(uint8_t), 1, fp) != 1) {
        os.printf("error:Failed to write grid size\n");
        fclose(fp);
        return;
    }

    if(fwrite(&grid_radius, sizeof(float), 1, fp) != 1) {
        os.printf("error:Failed to write grid radius\n");
        fclose(fp);
        return;
    }

    for (int y = 0; y < grid_size; y++) {
        for (int x = 0; x < grid_size; x++) {
            if(fwrite(&grid[x + (grid_size * y)], sizeof(float), 1, fp) != 1) {
                os.printf("error:Failed to write grid\n");
                fclose(fp);
                return;
            }
        }
    }
    os.printf("grid saved to %s\n", GRIDFILE);
    fclose(fp);
}

bool DeltaGridStrategy::load_grid(OutputStream& os)
{
    FILE *fp = fopen(GRIDFILE, "r");
    if(fp == NULL) {
        os.printf("error:Failed to open grid %s\n", GRIDFILE);
        return false;
    }

    uint8_t size;
    float radius;

    if(fread(&size, sizeof(uint8_t), 1, fp) != 1) {
        os.printf("error:Failed to read grid size\n");
        fclose(fp);
        return false;
    }

    if(size != grid_size) {
        os.printf("error:grid size is different read %d - config %d\n", size, grid_size);
        fclose(fp);
        return false;
    }

    if(fread(&radius, sizeof(float), 1, fp) != 1) {
        os.printf("error:Failed to read grid radius\n");
        fclose(fp);
        return false;
    }

    if(radius != grid_radius) {
        os.printf("warning:grid radius is different read %f - config %f, overriding config\n", radius, grid_radius);
        grid_radius = radius;
    }

    for (int y = 0; y < grid_size; y++) {
        for (int x = 0; x < grid_size; x++) {
            if(fread(&grid[x + (grid_size * y)], sizeof(float), 1, fp) != 1) {
                os.printf("error:Failed to read grid\n");
                fclose(fp);
                return false;
            }
        }
    }
    os.printf("grid loaded, radius: %f, size: %d\n", grid_radius, grid_size);
    fclose(fp);
    return true;
}

bool DeltaGridStrategy::probe_grid(int n, float radius, OutputStream& os)
{
    if(n < 5) {
        os.printf("Need at least a 5x5 grid to probe\n");
        return true;
    }

    float maxz = -1e6F, minz = 1e6F;
    float initial_z;
    if(!findBed(initial_z)) return false;

    float d = ((radius * 2) / (n - 1));

    for (int c = 0; c < n; ++c) {
        std::string scanline;
        float y = -radius + d * c;
        for (int r = 0; r < n; ++r) {
            float x = -radius + d * r;
            // Avoid probing the corners (outside the round or hexagon print surface) on a delta printer.
            float distance_from_center = sqrtf(x * x + y * y);
            float z = 0.0F;
            if (distance_from_center <= radius) {
                float mm;
                if(!zprobe->doProbeAt(mm, x, y)) return false;
                z = zprobe->getProbeHeight() - mm;
                if(z > maxz) maxz = z;
                if(z < minz) minz = z;
            }
            char buf[16];
            size_t s= snprintf(buf, sizeof(buf), "%8.4f ", z);
            scanline.append(buf, s);
        }
        os.printf("%s\n", scanline.c_str());
    }
    os.printf("max: %1.4f, min: %1.4f, delta: %1.4f\n", maxz, minz, maxz - minz);
    return true;
}

// taken from Oskars PR #713
bool DeltaGridStrategy::probe_spiral(int n, float radius, OutputStream& os)
{
    float a = radius / (2 * sqrtf(n * M_PI));
    float step_length = radius * radius / (2 * a * n);

    float initial_z;
    if(!findBed(initial_z)) return false;

    auto theta = [a](float length) {return sqrtf(2 * length / a); };

    float maxz = -1e6F, minz = 1e6F;
    for (int i = 0; i < n; i++) {
        float angle = theta(i * step_length);
        float r = angle * a;
        // polar to cartesian
        float x = r * cosf(angle);
        float y = r * sinf(angle);

        float mm;
        if (!zprobe->doProbeAt(mm, x, y)) return false;
        float z = zprobe->getProbeHeight() - mm;
        os.printf("PROBE: X%1.4f, Y%1.4f, Z%1.4f\n", x, y, z);
        if(z > maxz) maxz = z;
        if(z < minz) minz = z;
    }

    os.printf("max: %1.4f, min: %1.4f, delta: %1.4f\n", maxz, minz, maxz - minz);
    return true;
}

bool DeltaGridStrategy::handle_gcode(GCode& gcode, OutputStream& os)
{
    if (gcode.get_code() == 29) { // do a probe to test flatness
        // first wait for an empty queue i.e. no moves left
        Conveyor::getInstance()->wait_for_idle();

        int n = gcode.has_arg('I') ? gcode.get_arg('I') : 0;
        float radius = grid_radius;
        if(gcode.has_arg('J')) radius = gcode.get_arg('J'); // override default probe radius
        if(gcode.get_subcode() == 1) {
            if(n == 0) n = 50;
            probe_spiral(n, radius, os);
        } else {
            if(n == 0) n = 7;
            probe_grid(n, radius, os);
        }

        return true;

    } else if( gcode.get_code() == 31 ) { // do a grid probe
        // first wait for an empty queue i.e. no moves left
        Conveyor::getInstance()->wait_for_idle();

        if(!doProbe(gcode, os)) {
            os.printf("Probe failed to complete, check the initial probe height and/or initial_height settings\n");
        } else {
            os.printf("Probe completed. Use M374 to save it\n");
        }
        return true;
    }

    return false;
}

bool DeltaGridStrategy::handle_mcode(GCode & gcode, OutputStream & os)
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

    } else if(gcode.get_code() == 500) { // M500 save
        float x, y, z;
        std::tie(x, y, z) = probe_offsets;
        os.printf(";Probe offsets:\nM565 X%1.5f Y%1.5f Z%1.5f\n", x, y, z);
        if(save) {
            if(grid != nullptr) os.printf(";Load saved grid\nM375\n");
            else if(gcode.get_subcode() == 3) os.printf(";WARNING No grid to save\n");
        }
        return true;
    }

    return false;
}


// These are convenience defines to keep the code as close to the original as possible it also saves memory and flash
// set the rectangle in which to probe
#define LEFT_PROBE_BED_POSITION (-grid_radius)
#define RIGHT_PROBE_BED_POSITION (grid_radius)
#define BACK_PROBE_BED_POSITION (grid_radius)
#define FRONT_PROBE_BED_POSITION (-grid_radius)

// probe at the points of a lattice grid
#define AUTO_BED_LEVELING_GRID_X ((RIGHT_PROBE_BED_POSITION - LEFT_PROBE_BED_POSITION) / (grid_size - 1))
#define AUTO_BED_LEVELING_GRID_Y ((BACK_PROBE_BED_POSITION - FRONT_PROBE_BED_POSITION) / (grid_size - 1))

#define X_PROBE_OFFSET_FROM_EXTRUDER std::get<0>(probe_offsets)
#define Y_PROBE_OFFSET_FROM_EXTRUDER std::get<1>(probe_offsets)
#define Z_PROBE_OFFSET_FROM_EXTRUDER std::get<2>(probe_offsets)

void DeltaGridStrategy::setAdjustFunction(bool on)
{
    if(on) {
        // set the compensationTransform in robot
        using std::placeholders::_1;
        using std::placeholders::_2;
        Robot::getInstance()->compensationTransform = std::bind(&DeltaGridStrategy::doCompensation, this, _1, _2); // [this](float *target, bool inverse) { doCompensation(target, inverse); };
    } else {
        // clear it
        Robot::getInstance()->compensationTransform = nullptr;
    }
}

bool DeltaGridStrategy::findBed(float& ht)
{
    if (do_home) zprobe->home();
    // move to an initial position fast so as to not take all day
    float deltaz = initial_height;
    zprobe->move_z(deltaz, zprobe->getFastFeedrate());
    zprobe->move_xy(0, 0, zprobe->getFastFeedrate()); // move to 0,0

    // find bed at 0,0 run at slow rate so as to not hit bed hard
    float mm;
    if(!zprobe->run_probe_return(mm, zprobe->getSlowFeedrate())) return false;

    float dz = zprobe->getProbeHeight() - mm;
    zprobe->move_z(dz, zprobe->getFastFeedrate(), true); // relative move

    ht = mm + deltaz - zprobe->getProbeHeight(); // distance above bed
    return true;
}

bool DeltaGridStrategy::doProbe(GCode& gcode, OutputStream& os)
{
    os.printf("Delta Grid Probe...\n");
    setAdjustFunction(false);
    reset_bed_level();

    if(gcode.has_arg('J')) grid_radius = gcode.get_arg('J'); // override default probe radius, will get saved

    float radius = grid_radius;
    // find bed, and leave probe probe height above bed
    float initial_z;
    if(!findBed(initial_z)) {
        os.printf("Finding bed failed, check the max_travel and initial height settings\n");
        return false;
    }

    os.printf("Probe start ht is %f mm, probe radius is %f mm, grid size is %dx%d\n", initial_z, radius, grid_size, grid_size);

    // do first probe for 0,0
    float mm;
    if(!zprobe->doProbeAt(mm, -X_PROBE_OFFSET_FROM_EXTRUDER, -Y_PROBE_OFFSET_FROM_EXTRUDER)) return false;
    float z_reference = zprobe->getProbeHeight() - mm; // this should be zero
    os.printf("probe at 0,0 is %f mm\n", z_reference);

    // probe all the points in the grid within the given radius
    for (int yCount = 0; yCount < grid_size; yCount++) {
        float yProbe = FRONT_PROBE_BED_POSITION + AUTO_BED_LEVELING_GRID_Y * yCount;
        int xStart, xStop, xInc;
        if (yCount % 2) {
            xStart = 0;
            xStop = grid_size;
            xInc = 1;
        } else {
            xStart = grid_size - 1;
            xStop = -1;
            xInc = -1;
        }

        for (int xCount = xStart; xCount != xStop; xCount += xInc) {
            float xProbe = LEFT_PROBE_BED_POSITION + AUTO_BED_LEVELING_GRID_X * xCount;

            // Avoid probing the corners (outside the round or hexagon print surface) on a delta printer.
            float distance_from_center = sqrtf(xProbe * xProbe + yProbe * yProbe);
            if (distance_from_center > radius) continue;

            if(!zprobe->doProbeAt(mm, xProbe - X_PROBE_OFFSET_FROM_EXTRUDER, yProbe - Y_PROBE_OFFSET_FROM_EXTRUDER)) return false;
            float measured_z = zprobe->getProbeHeight() - mm - z_reference; // this is the delta z from bed at 0,0
            os.printf("DEBUG: X%1.4f, Y%1.4f, Z%1.4f\n", xProbe, yProbe, measured_z);
            grid[xCount + (grid_size * yCount)] = measured_z;
        }
    }

    extrapolate_unprobed_bed_level();
    print_bed_level(os);

    setAdjustFunction(true);

    return true;
}

void DeltaGridStrategy::extrapolate_one_point(int x, int y, int xdir, int ydir)
{
    // We use a very large negative number to see if it is allocated
    if (grid[x + (grid_size * y)] > -1e5F) {
        return;  // Don't overwrite good values.
    }
    float a = 2 * grid[(x + xdir) + (y * grid_size)] - grid[(x + xdir * 2) + (y * grid_size)]; // Left to right.
    float b = 2 * grid[x + ((y + ydir) * grid_size)] - grid[x + ((y + ydir * 2) * grid_size)]; // Front to back.
    float c = 2 * grid[(x + xdir) + ((y + ydir) * grid_size)] - grid[(x + xdir * 2) + ((y + ydir * 2) * grid_size)]; // Diagonal.
    float median = c;  // Median is robust (ignores outliers).
    if (a < b) {
        if (b < c) median = b;
        if (c < a) median = a;
    } else {  // b <= a
        if (c < b) median = b;
        if (a < c) median = a;
    }
    grid[x + (grid_size * y)] = median;
}

// Fill in the unprobed points (corners of circular print surface)
// using linear extrapolation, away from the center.
void DeltaGridStrategy::extrapolate_unprobed_bed_level()
{
    int half = (grid_size - 1) / 2;
    for (int y = 0; y <= half; y++) {
        for (int x = 0; x <= half; x++) {
            if (x + y < 3) continue;
            extrapolate_one_point(half - x, half - y, x > 1 ? +1 : 0, y > 1 ? +1 : 0);
            extrapolate_one_point(half + x, half - y, x > 1 ? -1 : 0, y > 1 ? +1 : 0);
            extrapolate_one_point(half - x, half + y, x > 1 ? +1 : 0, y > 1 ? -1 : 0);
            extrapolate_one_point(half + x, half + y, x > 1 ? -1 : 0, y > 1 ? -1 : 0);
        }
    }
}

void DeltaGridStrategy::doCompensation(float *target, bool inverse)
{
    // Adjust print surface height by linear interpolation over the bed_level array.
    int half = (grid_size - 1) / 2;
    float grid_x = std::max(0.001F - half, std::min(half - 0.001F, target[X_AXIS] / AUTO_BED_LEVELING_GRID_X));
    float grid_y = std::max(0.001F - half, std::min(half - 0.001F, target[Y_AXIS] / AUTO_BED_LEVELING_GRID_Y));
    int floor_x = floorf(grid_x);
    int floor_y = floorf(grid_y);
    float ratio_x = grid_x - floor_x;
    float ratio_y = grid_y - floor_y;
    float z1 = grid[(floor_x + half) + ((floor_y + half) * grid_size)];
    float z2 = grid[(floor_x + half) + ((floor_y + half + 1) * grid_size)];
    float z3 = grid[(floor_x + half + 1) + ((floor_y + half) * grid_size)];
    float z4 = grid[(floor_x + half + 1) + ((floor_y + half + 1) * grid_size)];
    float left = (1 - ratio_y) * z1 + ratio_y * z2;
    float right = (1 - ratio_y) * z3 + ratio_y * z4;
    float offset = (1 - ratio_x) * left + ratio_x * right;

    if(inverse)
        target[Z_AXIS] -= offset;
    else
        target[Z_AXIS] += offset;


    /*
        THEKERNEL->streams->printf("//DEBUG: TARGET: %f, %f, %f\n", target[0], target[1], target[2]);
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
    */
}


// Print calibration results for plotting or manual frame adjustment.
void DeltaGridStrategy::print_bed_level(OutputStream& os)
{
    for (int y = 0; y < grid_size; y++) {
        for (int x = 0; x < grid_size; x++) {
            os.printf("%7.4f ", grid[x + (grid_size * y)]);
        }
        os.printf("\n");
    }
}

// Reset calibration results to zero.
void DeltaGridStrategy::reset_bed_level()
{
    for (int y = 0; y < grid_size; y++) {
        for (int x = 0; x < grid_size; x++) {
            // set to very big negative number to indicate not set
            // then to be safe check against < -1E5F
            grid[x + (grid_size * y)] = -1e6F;
        }
    }
}
