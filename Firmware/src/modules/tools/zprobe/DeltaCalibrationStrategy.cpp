#include "DeltaCalibrationStrategy.h"

#include "ConfigReader.h"
#include "Robot.h"
#include "main.h"
#include "GCode.h"
#include "Conveyor.h"
#include "ZProbe.h"
#include "Dispatcher.h"
#include "StepperMotor.h"
#include "BaseSolution.h"
#include "OutputStream.h"

#include <cmath>
#include <tuple>
#include <algorithm>

#define radius_key "radius"
#define initial_height_key "initial_height"
#define tolerance_key "tolerance"

bool DeltaCalibrationStrategy::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("delta calibration strategy", m)) {
        printf("configure-delta-calibration: no delta calibration strategy section found\n");
        return false;
    }

    // default is probably wrong
    this->probe_radius = cr.get_float(m, radius_key, 100.0F);

    // the initial height above the bed we stop the initial move down after home to find the bed
    // this should be a height that is enough that the probe will not hit the bed
    this->initial_height = cr.get_float(m, initial_height_key, 20);

    // the error we allow for probe (target to resolve to)
    this->tolerance = cr.get_float(m, tolerance_key, 0.03F);

    return true;
}

bool DeltaCalibrationStrategy::handle_gcode(GCode& gcode, OutputStream& os)
{
    // G code processing
    if( gcode.get_code() == 32 ) { // auto calibration for delta, Z bed mapping for cartesian
        // first wait for an empty queue i.e. no moves left
        Conveyor::getInstance()->wait_for_idle();

        // turn off any compensation transform as it will be invalidated anyway by this
        Robot::getInstance()->compensationTransform = nullptr;

        if(!gcode.has_arg('R')) {
            if(!calibrate_delta_endstops(gcode, os)) {
                os.printf("Calibration failed to complete, check the initial probe height and/or initial_height settings\n");
                return true;
            }
        }
        if(!gcode.has_arg('E')) {
            if(!calibrate_delta_radius(gcode, os)) {
                os.printf("Calibration failed to complete, check the initial probe height and/or initial_height settings\n");
                return true;
            }
        }
        os.printf("Calibration complete, save settings with M500\n");
        return true;

    } else if (gcode.get_code() == 29) {
        // probe the 7 points
        if(!probe_delta_points(gcode, os)) {
            os.printf("Calibration failed to complete, check the initial probe height and/or initial_height settings\n");
        }
        return true;
    }
    return false;
}

// calculate the X and Y positions for the three towers given the radius from the center
static std::tuple<float, float, float, float, float, float> getCoordinates(float radius)
{
    float px = 0.866F * radius; // ~sin(60)
    float py = 0.5F * radius; // cos(60)
    float t1x = -px, t1y = -py; // X Tower
    float t2x = px, t2y = -py; // Y Tower
    float t3x = 0.0F, t3y = radius; // Z Tower
    return std::make_tuple(t1x, t1y, t2x, t2y, t3x, t3y);
}


// Probes the 7 points on a delta can be used for off board calibration
bool DeltaCalibrationStrategy::probe_delta_points(GCode& gcode, OutputStream& os)
{
    float bedht;
    if(!findBed(bedht)) return false;

    os.printf("initial Bed ht is Z%f\n", bedht);

    // check probe ht
    float mm;
    if(!zprobe->doProbeAt(mm, 0, 0)) return false;
    float dz = zprobe->getProbeHeight() - mm;
    os.printf("center probe: %1.4f\n", dz);

    // get probe points
    float t1x, t1y, t2x, t2y, t3x, t3y;
    std::tie(t1x, t1y, t2x, t2y, t3x, t3y) = getCoordinates(this->probe_radius);

    // gather probe points
    float pp[][2] {{t1x, t1y}, {t2x, t2y}, {t3x, t3y}, {0, 0}, { -t1x, -t1y}, { -t2x, -t2y}, { -t3x, -t3y}};

    float max_delta = 0;
    float last_z;
    bool last_z_set = false;
    float start_z = Robot::getInstance()->actuators[2]->get_current_position();

    for(auto& i : pp) {
        if(!zprobe->doProbeAt(mm, i[0], i[1])) return false;
        float z = mm;
        if(gcode.get_subcode() == 0) {
            // prints the delta Z moved at the XY coordinates given
            os.printf("X:%1.4f Y:%1.4f Z:%1.4f\n", i[0], i[1], z);

        } else if(gcode.get_subcode() == 1) {
            // format that can be pasted here http://escher3d.com/pages/wizards/wizarddelta.php
            os.printf("X%1.4f Y%1.4f Z%1.4f\n", i[0], i[1], start_z - z); // actual Z of bed at probe point
        }

        if(!last_z_set) {
            last_z = z;
            last_z_set = true;
        } else {
            max_delta = std::max(max_delta, fabsf(z - last_z));
        }
    }

    os.printf("max delta: %f\n", max_delta);

    return true;
}

bool DeltaCalibrationStrategy::findBed(float& ht)
{
    // home
    zprobe->home();

    // move to an initial position fast so as to not take all day, we move to initial_height, which is set in config, default 20mm
    // This needs to be high enough to take the probe position under the head into account
    zprobe->move_z(initial_height, zprobe->getFastFeedrate());

    // find bed, run at slow rate so as to not hit bed hard
    float mm;
    if(!zprobe->run_probe(mm, zprobe->getSlowFeedrate())) return false;

    // leave the probe zprobe->getProbeHeight() above bed
    float dz = zprobe->getProbeHeight();
    zprobe->move_z(dz, zprobe->getFastFeedrate(), true); // relative move up

    ht = Robot::getInstance()->get_axis_position(Z_AXIS); // this is where we need to go to after homing to be 5mm above bed
    return true;
}

/* Run a calibration routine for a delta
    1. Home
    2. probe for z bed
    3. probe initial tower positions
    4. set initial trims such that trims will be minimal negative values
    5. home, probe three towers again
    6. calculate trim offset and apply to all trims
    7. repeat 5, 6 until it converges on a solution
*/

bool DeltaCalibrationStrategy::calibrate_delta_endstops(GCode& gcode, OutputStream& os)
{
    float target = this->tolerance;
    if(gcode.has_arg('I')) target = gcode.get_arg('I'); // override default target
    if(gcode.has_arg('J')) this->probe_radius = gcode.get_arg('J'); // override default probe radius

    bool keep = false;
    if(gcode.has_arg('K')) keep = true; // keep current settings

    os.printf("Calibrating Endstops: target %fmm, radius %fmm\n", target, this->probe_radius);

    // get probe points
    float t1x, t1y, t2x, t2y, t3x, t3y;
    std::tie(t1x, t1y, t2x, t2y, t3x, t3y) = getCoordinates(this->probe_radius);

    float trimx = 0.0F, trimy = 0.0F, trimz = 0.0F;
    if(!keep) {
        // zero trim values
        if(!set_trim(0, 0, 0, os)) return false;

    } else {
        // get current trim, and continue from that
        if (get_trim(trimx, trimy, trimz)) {
            os.printf("Current Trim X: %f, Y: %f, Z: %f\r\n", trimx, trimy, trimz);

        } else {
            os.printf("Could not get current trim, are endstops enabled?\n");
            return false;
        }
    }

    // find the bed, as we potentially have a temporary z probe we don't know how low under the nozzle it is
    // so we need to find the initial place that the probe triggers when it hits the bed
    float bedht;
    if(!findBed(bedht)) return false;

    os.printf("initial start ht is Z%f\n", bedht);

    // check probe ht
    float mm;
    if(!zprobe->doProbeAt(mm, 0, 0)) return false;
    float dz = zprobe->getProbeHeight() - mm;
    os.printf("center probe: %1.4f\n", dz);
    if(fabsf(dz) > target) {
        os.printf("Probe was not repeatable to %f mm, (%f)\n", target, dz);
        return false;
    }

    // get initial probes
    // probe the base of the X tower
    if(!zprobe->doProbeAt(mm, t1x, t1y)) return false;
    float t1z = mm;
    os.printf("T1-0 Z:%1.4f\n", t1z);

    // probe the base of the Y tower
    if(!zprobe->doProbeAt(mm, t2x, t2y)) return false;
    float t2z = mm;
    os.printf("T2-0 Z:%1.4f\n", t2z);

    // probe the base of the Z tower
    if(!zprobe->doProbeAt(mm, t3x, t3y)) return false;
    float t3z = mm;
    os.printf("T3-0 Z:%1.4f\n", t3z);

    float trimscale = 1.2522F; // empirically determined

    auto mmx = std::minmax({t1z, t2z, t3z});
    if((mmx.second - mmx.first) <= target) {
        os.printf("trim already set within required parameters: delta %f\n", mmx.second - mmx.first);
        return true;
    }

    // set trims to worst case so we always have a negative trim
    trimx += (mmx.first - t1z) * trimscale;
    trimy += (mmx.first - t2z) * trimscale;
    trimz += (mmx.first - t3z) * trimscale;

    for (int i = 1; i <= 10; ++i) {
        // set trim
        if(!set_trim(trimx, trimy, trimz, os)) return false;

        // home and move probe to start position just above the bed
        zprobe->home();
        zprobe->move_z(bedht, zprobe->getFastFeedrate());

        // probe the base of the X tower
        if(!zprobe->doProbeAt(mm, t1x, t1y)) return false;
        t1z = mm;
        os.printf("T1-%d Z:%1.4f\n", i, t1z);

        // probe the base of the Y tower
        if(!zprobe->doProbeAt(mm, t2x, t2y)) return false;
        t2z = mm;
        os.printf("T2-%d Z:%1.4f\n", i, t2z);

        // probe the base of the Z tower
        if(!zprobe->doProbeAt(mm, t3x, t3y)) return false;
        t3z = mm;
        os.printf("T3-%d Z:%1.4f\n", i, t3z);

        mmx = std::minmax({t1z, t2z, t3z});
        if((mmx.second - mmx.first) <= target) {
            os.printf("trim set to within required parameters: delta %f\n", mmx.second - mmx.first);
            break;
        }

        // set new trim values based on min difference
        trimx += (mmx.first - t1z) * trimscale;
        trimy += (mmx.first - t2z) * trimscale;
        trimz += (mmx.first - t3z) * trimscale;
    }

    if((mmx.second - mmx.first) > target) {
        os.printf("WARNING: trim did not resolve to within required parameters: delta %f\n", mmx.second - mmx.first);
    }

    return true;
}

/*
    probe edges to get outer positions, then probe center
    modify the delta radius until center and X converge
*/

bool DeltaCalibrationStrategy::calibrate_delta_radius(GCode& gcode, OutputStream& os)
{
    float target = this->tolerance;
    if(gcode.has_arg('I')) target = gcode.get_arg('I'); // override default target
    if(gcode.has_arg('J')) this->probe_radius = gcode.get_arg('J'); // override default probe radius

    os.printf("Calibrating delta radius: target %f, radius %f\n", target, this->probe_radius);

    // get probe points
    float t1x, t1y, t2x, t2y, t3x, t3y;
    std::tie(t1x, t1y, t2x, t2y, t3x, t3y) = getCoordinates(this->probe_radius);

    // find the bed, as we potentially have a temporary z probe we don't know how low under the nozzle it is
    // so we need to find the initial place that the probe triggers when it hits the bed
    float bedht;
    if(!findBed(bedht)) return false;
    os.printf("initial start ht is Z%f\n", bedht);

    // check probe ht
    float mm;
    if(!zprobe->doProbeAt(mm, 0, 0)) return false;
    float dz = zprobe->getProbeHeight() - mm;
    os.printf("center probe: %1.4f\n", dz);
    if(fabsf(dz) > target) {
        os.printf("Probe was not repeatable to %f mm, (%f)\n", target, dz);
        return false;
    }

    // probe center to get reference point at this Z height
    float dc;
    if(!zprobe->doProbeAt(dc, 0, 0)) return false;
    os.printf("CT Z:%1.3f\n", dc);
    float cmm = dc;

    // get current delta radius
    float delta_radius = 0.0F;
    BaseSolution::arm_options_t options;
    if(Robot::getInstance()->arm_solution->get_optional(options)) {
        delta_radius = options['R'];
    }
    if(delta_radius == 0.0F) {
        os.printf("This appears to not be a delta arm solution\n");
        return false;
    }
    options.clear();

    bool good = false;
    float drinc = 2.5F; // approx
    for (int i = 1; i <= 10; ++i) {
        // probe t1, t2, t3 and get average, but use coordinated moves, probing center won't change
        float dx, dy;
        if(!zprobe->doProbeAt(dx, t1x, t1y)) return false;
        os.printf("T1-%d Z:%1.3f\n", i, dx);
        if(!zprobe->doProbeAt(dy, t2x, t2y)) return false;
        os.printf("T2-%d Z:%1.3f\n", i, dy);
        if(!zprobe->doProbeAt(dz, t3x, t3y)) return false;
        os.printf("T3-%d Z:%1.3f\n", i, dz);

        // now look at the difference and reduce it by adjusting delta radius
        float m = (dx + dy + dz) / 3.0F;
        float d = cmm - m;
        os.printf("C-%d Z-ave:%1.4f delta: %1.3f\n", i, m, d);

        if(fabsf(d) <= target) {
            good = true;
            break; // resolution of success
        }

        // increase delta radius to adjust for low center
        // decrease delta radius to adjust for high center
        delta_radius += (d * drinc);

        // set the new delta radius
        options['R'] = delta_radius;
        Robot::getInstance()->arm_solution->set_optional(options);
        os.printf("Setting delta radius to: %1.4f\n", delta_radius);

        zprobe->home();
        zprobe->move_z(bedht, zprobe->getFastFeedrate());
    }

    if(!good) {
        os.printf("WARNING: delta radius did not resolve to within required parameters: %f\n", target);
    }

    return true;
}

bool DeltaCalibrationStrategy::set_trim(float x, float y, float z, OutputStream& os)
{
    float t[3] {x, y, z};

    Module *m = Module::lookup("endstops");
    if(m == nullptr) {
        os.printf("unable to set trim, are endstops enabled?\n");
        return false;
    }

    bool ok = m->request("set_trim", t);
    if (ok) {
        os.printf("set trim to X:%f Y:%f Z:%f\n", x, y, z);
    } else {
        os.printf("unable to set trim, are endstops enabled?\n");
    }

    return ok;
}

bool DeltaCalibrationStrategy::get_trim(float &x, float &y, float &z)
{
    Module *m = Module::lookup("endstops");
    if(m == nullptr) {
        return false;
    }

    float trim[3];
    bool ok = m->request("get-trim", trim);

    if (ok) {
        x = trim[0];
        y = trim[1];
        z = trim[2];
        return true;
    }

    return false;
}
