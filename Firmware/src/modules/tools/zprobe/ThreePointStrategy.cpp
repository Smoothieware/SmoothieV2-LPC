/*
    Author: Jim Morris (wolfmanjm@gmail.com)
    License: GPL3 or better see <http://www.gnu.org/licenses/>

    Summary
    -------
    Probes three user specified points on the bed and determines the plane of the bed relative to the probe.
    as the head moves in X and Y it will adjust Z to keep the head tram with the bed.

    Configuration
    -------------
    The strategy must be enabled in the cofnig as well as zprobe.

    leveling-strategy.three-point-leveling.enable         true

    Three probe points must be defined, these are best if they are the three points of an equilateral triangle, as far apart as possible.
    They can be defined in the config file as:-

    leveling-strategy.three-point-leveling.point1         100.0,0.0   # the first probe point (x,y)
    leveling-strategy.three-point-leveling.point2         200.0,200.0 # the second probe point (x,y)
    leveling-strategy.three-point-leveling.point3         0.0,200.0   # the third probe point (x,y)

    or they may be defined (and saved with M500) using M557 P0 X30 Y40.5  where P is 0,1,2

    probe offsets from the nozzle or tool head can be defined with

    leveling-strategy.three-point-leveling.probe_offsets  0,0,0  # probe offsetrs x,y,z

    they may also be set with M565 X0 Y0 Z0

    To force homing in X and Y before G32 does the probe the following can be set in config, this is the default

    leveling-strategy.three-point-leveling.home_first    true   # disable by setting to false

    The probe tolerance can be set using the config line

    leveling-strategy.three-point-leveling.tolerance   0.03    # the probe tolerance in mm, default is 0.03mm


    Usage
    -----
    G29 probes the three probe points and reports the Z at each point, if a plane is active it will be used to level the probe.
    G32 probes the three probe points and defines the bed plane, this will remain in effect until reset or M561
    G31 reports the status

    M557 defines the probe points
    M561 clears the plane and the bed leveling is disabled until G32 is run again
    M565 defines the probe offsets from the nozzle or tool head

    M500 saves the probe points and the probe offsets
    M503 displays the current settings
*/

#include "ThreePointStrategy.h"
#include "ConfigReader.h"
#include "Robot.h"
#include "main.h"
#include "GCode.h"
#include "Conveyor.h"
#include "ZProbe.h"
#include "Plane3D.h"
#include "Dispatcher.h"
#include "OutputStream.h"

#include <string>
#include <algorithm>
#include <cstdlib>
#include <cmath>

#define probe_point_1_key "point1"
#define probe_point_2_key "point2"
#define probe_point_3_key "point3"
#define probe_offsets_key "probe_offsets"
#define home_key "home_first"
#define tolerance_key "tolerance"
#define save_plane_key "save_plane"

ThreePointStrategy::ThreePointStrategy(ZProbe *zprb) : ZProbeStrategy(zprb)
{
    for (int i = 0; i < 3; ++i) {
        probe_points[i] = std::make_tuple(0.0F, 0.0F);
    }
    plane = nullptr;
}

ThreePointStrategy::~ThreePointStrategy()
{
    delete plane;
}

bool ThreePointStrategy::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("three point leveling strategy", m)) {
        printf("configure-zprobe: no three point leveling strategy section found\n");
        return false;
    }

    // format is xxx,yyy for the probe points
    std::string p1 = cr.get_string(m, probe_point_1_key, "");
    std::string p2 = cr.get_string(m, probe_point_2_key, "");
    std::string p3 = cr.get_string(m, probe_point_3_key, "");
    if(!p1.empty()) probe_points[0] = parseXY(p1.c_str());
    if(!p2.empty()) probe_points[1] = parseXY(p2.c_str());
    if(!p3.empty()) probe_points[2] = parseXY(p3.c_str());

    // Probe offsets xxx,yyy,zzz
    std::string po = cr.get_string(m, probe_offsets_key, "0,0,0");
    this->probe_offsets = parseXYZ(po.c_str());

    this->home = cr.get_bool(m, home_key, true);
    this->tolerance = cr.get_float(m, tolerance_key, 0.03F);
    this->save = cr.get_bool(m, save_plane_key, false);

    // register mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    // M Code handlers
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 557, std::bind(&ThreePointStrategy::handle_mcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 561, std::bind(&ThreePointStrategy::handle_mcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 565, std::bind(&ThreePointStrategy::handle_mcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 565, std::bind(&ThreePointStrategy::handle_mcode, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 500, std::bind(&ThreePointStrategy::handle_mcode, this, _1, _2));

    return true;
}

bool ThreePointStrategy::handle_gcode(GCode& gcode, OutputStream& os)
{
    // G code processing
    if(gcode.get_code() == 29) { // test probe points for level
        if(!test_probe_points(os)) {
            os.printf("Probe failed to complete, probe not triggered or other error\n");
        }
        return true;

    } else if( gcode.get_code() == 31 ) { // report status
        if(this->plane == nullptr) {
            os.printf("Bed leveling plane is not set\n");
        } else {
            os.printf("Bed leveling plane normal= %f, %f, %f\n", plane->getNormal()[0], plane->getNormal()[1], plane->getNormal()[2]);
        }
        os.printf("Probe is %s\n", zprobe->getProbeStatus() ? "Triggered" : "Not triggered");
        return true;

    } else if( gcode.get_code() == 32 ) { // three point probe
        // first wait for an empty queue i.e. no moves left
        Conveyor::getInstance()->wait_for_idle();

        // clear any existing plane and compensation
        delete this->plane;
        this->plane = nullptr;
        setAdjustFunction(false);

        if(!doProbing(os)) {
            os.printf("Probe failed to complete, probe not triggered or other error\n");
        } else {
            os.printf("Probe completed, bed plane defined\n");
        }
        return true;
    }

    return false;
}

bool ThreePointStrategy::handle_mcode(GCode& gcode, OutputStream& os)
{
    if(gcode.get_code() == 557) { // M557 - set probe points eg M557 P0 X30 Y40.5  where P is 0,1,2
        int idx = 0;
        float x = 0, y = 0;
        if(!gcode.has_arg('P') || !gcode.has_arg('X') || !gcode.has_arg('Y')) {
            os.printf("ERROR: Arguments P, X, Y are required\n");
            return true;
        }

        idx = gcode.get_arg('P');
        x = gcode.get_arg('X');
        y = gcode.get_arg('Y');
        if(idx >= 0 && idx <= 2) {
            probe_points[idx] = std::make_tuple(x, y);
        } else {
            os.printf("ERROR: only 3 probe points allowed P0-P2\n");
        }
        return true;

    } else if(gcode.get_code() == 561) { // M561: Set Identity Transform with no parameters, set the saved plane if A B C D are given
        delete this->plane;
        if(gcode.has_no_args()) {
            this->plane = nullptr;
            // delete the compensationTransform in robot
            setAdjustFunction(false);
            os.printf("saved plane cleared\n");

        } else {
            // smoothie specific way to restore a saved plane
            uint32_t a, b, c, d;
            a = b = c = d = 0;
            if(gcode.has_arg('A')) a = gcode.get_int_arg('A');
            if(gcode.has_arg('B')) b = gcode.get_int_arg('B');
            if(gcode.has_arg('C')) c = gcode.get_int_arg('C');
            if(gcode.has_arg('D')) d = gcode.get_int_arg('D');
            this->plane = new Plane3D(a, b, c, d);
            setAdjustFunction(true);
        }
        return true;

    } else if(gcode.get_code() == 565) { // M565: Set Z probe offsets
        float x = 0, y = 0, z = 0;
        if(gcode.has_arg('X')) x = gcode.get_arg('X');
        if(gcode.has_arg('Y')) y = gcode.get_arg('Y');
        if(gcode.has_arg('Z')) z = gcode.get_arg('Z');
        probe_offsets = std::make_tuple(x, y, z);
        return true;

    } else if(gcode.get_code() == 500) { // M500 save, M503 display
        float x, y, z;
        os.printf(";Probe points:\n");
        for (int i = 0; i < 3; ++i) {
            std::tie(x, y) = probe_points[i];
            os.printf("M557 P%d X%1.5f Y%1.5f\n", i, x, y);
        }
        os.printf(";Probe offsets:\n");
        std::tie(x, y, z) = probe_offsets;
        os.printf("M565 X%1.5f Y%1.5f Z%1.5f\n", x, y, z);

        // encode plane and save if set and M500 and enabled
        if(this->save && this->plane != nullptr) {
            if(gcode.get_code() == 500) {
                uint32_t a, b, c, d;
                this->plane->encode(a, b, c, d);
                os.printf(";Saved bed plane:\nM561 A%lu B%lu C%lu D%lu \n", a, b, c, d);
            } else {
                os.printf(";The bed plane will be saved on M500\n");
            }
        }
        return true;

    }
#if 0
    else if(gcode.get_code() == 9999) {
        // DEBUG run a test M9999 A B C X Y set Z to A B C and test for point at X Y
        Vector3 v[3];
        float x, y, z, a = 0, b = 0, c = 0;
        if(gcode.has_arg('A')) a = gcode.get_arg('A');
        if(gcode.has_arg('B')) b = gcode.get_arg('B');
        if(gcode.has_arg('C')) c = gcode.get_arg('C');
        std::tie(x, y) = probe_points[0]; v[0].set(x, y, a);
        std::tie(x, y) = probe_points[1]; v[1].set(x, y, b);
        std::tie(x, y) = probe_points[2]; v[2].set(x, y, c);
        delete this->plane;
        this->plane = new Plane3D(v[0], v[1], v[2]);
        os.printf("plane normal= %f, %f, %f\n", plane->getNormal()[0], plane->getNormal()[1], plane->getNormal()[2]);
        x = 0; y = 0;
        if(gcode.has_arg('X')) x = gcode.get_arg('X');
        if(gcode.has_arg('Y')) y = gcode.get_arg('Y');
        z = getZOffset(x, y);
        os.printf("z= %f\n", z);
        // tell robot to adjust z on each move
        setAdjustFunction(true);
        return true;
    }
#endif

    return false;
}

void ThreePointStrategy::homeXY()
{
    OutputStream nullos;
    Dispatcher::getInstance()->dispatch(nullos, 'G', 28, Dispatcher::getInstance()->is_grbl_mode() ? 2 : 0, 'X', 0.0F, 'Y', 0.0F, 0);
}

bool ThreePointStrategy::doProbing(OutputStream& os)
{
    float x, y;

    // optionally home XY axis first, but allow for manual homing
    if(this->home)
        homeXY();

    // move to the first probe point
    std::tie(x, y) = probe_points[0];
    // offset by the probe XY offset
    x -= std::get<X_AXIS>(this->probe_offsets);
    y -= std::get<Y_AXIS>(this->probe_offsets);
    zprobe->move_xy(x, y, zprobe->getFastFeedrate());

    // for now we use probe to find bed and not the Z min endstop
    // the first probe point becomes Z == 0 effectively so if we home Z or manually set z after this, it needs to be at the first probe point

    // TODO this needs to be configurable to use min z or probe

    // find bed via probe
    float mm;
    if(!zprobe->run_probe(mm, zprobe->getSlowFeedrate())) return false;

    // TODO if using probe then we probably need to set Z to 0 at first probe point, but take into account probe offset from head
    Robot::getInstance()->reset_axis_position(std::get<Z_AXIS>(this->probe_offsets), Z_AXIS);

    // move up to specified probe start position
    zprobe->move_z(zprobe->getProbeHeight(), zprobe->getSlowFeedrate()); // move to probe start position

    // probe the three points
    Vector3 v[3];
    for (int i = 0; i < 3; ++i) {
        float z;
        std::tie(x, y) = probe_points[i];
        // offset moves by the probe XY offset
        if(!zprobe->doProbeAt(z, x - std::get<X_AXIS>(this->probe_offsets), y - std::get<Y_AXIS>(this->probe_offsets))) return false;

        z = zprobe->getProbeHeight() - z; // relative distance between the probe points, lower is negative z
        os.printf("DEBUG: P%d:%1.4f\n", i, z);
        v[i] = Vector3(x, y, z);
    }

    // if first point is not within tolerance report it, it should ideally be 0
    if(fabsf(v[0][2]) > this->tolerance) {
        os.printf("WARNING: probe is not within tolerance: %f > %f\n", fabsf(v[0][2]), this->tolerance);
    }

    // define the plane
    delete this->plane;
    // check tolerance level here default 0.03mm
    auto mmx = std::minmax({v[0][2], v[1][2], v[2][2]});
    if((mmx.second - mmx.first) <= this->tolerance) {
        this->plane = nullptr; // plane is flat no need to do anything
        os.printf("DEBUG: flat plane\n");
        // clear the compensationTransform in robot
        setAdjustFunction(false);

    } else {
        this->plane = new Plane3D(v[0], v[1], v[2]);
        os.printf("DEBUG: plane normal= %f, %f, %f\n", plane->getNormal()[0], plane->getNormal()[1], plane->getNormal()[2]);
        setAdjustFunction(true);
    }

    return true;
}

// Probes the 3 points and reports heights
bool ThreePointStrategy::test_probe_points(OutputStream& os)
{
    // check the probe points have been defined
    float max_delta = 0;
    float last_z;
    bool last_z_set = false;
    for (int i = 0; i < 3; ++i) {
        float x, y;
        std::tie(x, y) = probe_points[i];

        float z;
        if(!zprobe->doProbeAt(z, x - std::get<X_AXIS>(this->probe_offsets), y - std::get<Y_AXIS>(this->probe_offsets))) return false;

        os.printf("X:%1.4f Y:%1.4f Z:%1.4f\n", x, y, z);

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

void ThreePointStrategy::setAdjustFunction(bool on)
{
    if(on) {
        // set the compensationTransform in robot
        Robot::getInstance()->compensationTransform = [this](float * target, bool inverse) { if(inverse) target[2] -= this->plane->getz(target[0], target[1]); else target[2] += this->plane->getz(target[0], target[1]); };
    } else {
        // clear it
        Robot::getInstance()->compensationTransform = nullptr;
    }
}

// find the Z offset for the point on the plane at x, y
float ThreePointStrategy::getZOffset(float x, float y)
{
    if(this->plane == nullptr) return NAN;
    return this->plane->getz(x, y);
}

// parse a "X,Y" string return x,y
std::tuple<float, float> ThreePointStrategy::parseXY(const char *str)
{
    float x = 0, y = 0;
    char *p;
    x = strtof(str, &p);
    if(p + 1 < str + strlen(str)) {
        y = strtof(p + 1, nullptr);
    }
    return std::make_tuple(x, y);
}

// parse a "X,Y,Z" string return x,y,z tuple
std::tuple<float, float, float> ThreePointStrategy::parseXYZ(const char *str)
{
    float x = 0, y = 0, z = 0;
    char *p;
    x = strtof(str, &p);
    if(p + 1 < str + strlen(str)) {
        y = strtof(p + 1, &p);
        if(p + 1 < str + strlen(str)) {
            z = strtof(p + 1, nullptr);
        }
    }
    return std::make_tuple(x, y, z);
}
