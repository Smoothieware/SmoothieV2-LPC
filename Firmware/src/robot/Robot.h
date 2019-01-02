#pragma once

#include <functional>
#include <stack>
#include <vector>
#include <cstring>

#include "Module.h"
#include "ActuatorCoordinates.h"
#include "AxisDefns.h"

class GCode;
class BaseSolution;
class StepperMotor;
class ConfigReader;
class OutputStream;

// 9 WCS offsets
#define MAX_WCS 9UL

class Robot : public Module
{
public:
    using wcs_t = std::tuple<float, float, float>;
    Robot();
    // delete copy and move constructors and assign operators
    Robot(Robot const&) = delete;             // Copy construct
    Robot(Robot&&) = delete;                  // Move construct
    Robot& operator=(Robot const&) = delete;  // Copy assign
    Robot& operator=(Robot &&) = delete;      // Move assign

    bool configure(ConfigReader&);

    static Robot *getInstance() { return instance; }
    void on_halt(bool flg);

    void reset_axis_position(float position, int axis);
    void reset_axis_position(float x, float y, float z);
    void reset_actuator_position(const ActuatorCoordinates &ac);
    void reset_position_from_current_actuator_position();
    float get_seconds_per_minute() const { return seconds_per_minute; }
    float get_z_maxfeedrate() const { return this->max_speeds[Z_AXIS]; }
    float get_default_acceleration() const { return default_acceleration; }
    void setToolOffset(const float offset[N_PRIMARY_AXIS]);
    float get_feed_rate() const;
    float get_s_value() const { return s_value; }
    void set_s_value(float s) { s_value = s; }
    void  push_state();
    void  pop_state();
    void check_max_actuator_speeds();
    float to_millimeters( float value ) const { return this->inch_mode ? value * 25.4F : value; }
    float from_millimeters( float value) const { return this->inch_mode ? value / 25.4F : value;  }
    float get_axis_position(int axis) const { return(this->machine_position[axis]); }
    void get_axis_position(float position[], size_t n = 3) const { memcpy(position, this->machine_position, n * sizeof(float)); }
    wcs_t get_axis_position() const { return wcs_t(machine_position[X_AXIS], machine_position[Y_AXIS], machine_position[Z_AXIS]); }
    void get_current_machine_position(float *pos) const;
    void print_position(uint8_t subcode, std::string& buf, bool ignore_extruders = false) const;
    uint8_t get_current_wcs() const { return current_wcs; }
    std::vector<wcs_t> get_wcs_state() const;
    std::tuple<float, float, float, uint8_t> get_last_probe_position() const { return last_probe_position; }
    void set_last_probe_position(std::tuple<float, float, float, uint8_t> p) { last_probe_position = p; }
    bool delta_move(const float delta[], float rate_mm_s, uint8_t naxis);
    uint8_t register_actuator(StepperMotor*);
    uint8_t get_number_registered_motors() const {return n_motors; }
    void enable_all_motors(bool flg);
    void get_query_string(std::string&) const;
    void do_park();


    BaseSolution* arm_solution;                           // Selected Arm solution ( millimeters to step calculation )

    // gets accessed by Panel, Endstops, ZProbe, Extruder
    std::vector<StepperMotor*> actuators;

    // set by a leveling strategy to transform the target of a move according to the current plan
    std::function<void(float*, bool)> compensationTransform;
    // set by an active extruder, returns the amount to scale the E parameter by (to convert mm³ to mm)
    std::function<float(void)> get_e_scale_fnc;

    // Workspace coordinate systems
    wcs_t mcs2wcs(const wcs_t &pos) const;
    wcs_t mcs2wcs(const float *pos) const { return mcs2wcs(wcs_t(pos[X_AXIS], pos[Y_AXIS], pos[Z_AXIS])); }

    struct {
        bool inch_mode: 1;                                // true for inch mode, false for millimeter mode ( default )
        bool absolute_mode: 1;                            // true for absolute mode ( default ), false for relative mode
        bool e_absolute_mode: 1;                          // true for absolute mode for E ( default ), false for relative mode
        bool next_command_is_MCS: 1;                      // set by G53
        bool disable_segmentation: 1;                     // set to disable segmentation
        bool disable_arm_solution: 1;                     // set to disable the arm solution
        bool segment_z_moves: 1;
        bool save_g92: 1;                                 // save g92 on M500 if set
        bool save_wcs: 1;                                 // save wcs on M500 if set
        bool is_g123: 1;
        uint8_t plane_axis_0: 2;                          // Current plane ( XY, XZ, YZ )
        uint8_t plane_axis_1: 2;
        uint8_t plane_axis_2: 2;
        bool check_driver_errors: 1;
        bool halt_on_driver_alarm: 1;
    };

private:
    static Robot *instance;

    enum MOTION_MODE_T {
        NONE,
        SEEK, // G0
        LINEAR, // G1
        CW_ARC, // G2
        CCW_ARC // G3
    };

    bool handle_gcodes(GCode& gcode, OutputStream& os);
    bool handle_mcodes(GCode& gcode, OutputStream& os);
    bool handle_motion_command(GCode& gcode, OutputStream& os);
    bool handle_dwell(GCode& gcode, OutputStream& os);
    bool handle_G10(GCode&, OutputStream&);
    bool handle_G92(GCode&, OutputStream&);
    bool handle_M500(GCode&, OutputStream&);
    bool handle_M665(GCode&, OutputStream&);
    #ifdef BOARD_PRIMEALPHA
    bool handle_M909(GCode&, OutputStream&);
    bool handle_M911(GCode&, OutputStream&);
    bool handle_setregs_cmd( std::string& params, OutputStream& os );
    #endif

    bool append_milestone(const float target[], float rate_mm_s);
    bool append_line(GCode& gcode, const float target[], float rate_mm_s, float delta_e);
    bool append_arc(GCode& gcode, const float target[], const float offset[], float radius, bool is_clockwise );
    bool compute_arc(GCode& gcode, const float offset[], const float target[], enum MOTION_MODE_T motion_mode);
    void process_move(GCode& gcode, enum MOTION_MODE_T);
    bool is_halted() const { return halted; }

    float theta(float x, float y);
    void select_plane(uint8_t axis_0, uint8_t axis_1, uint8_t axis_2);
    void clearToolOffset();
    int get_active_extruder() const;
    void periodic_checks();

    std::array<wcs_t, MAX_WCS> wcs_offsets; // these are persistent once saved with M500
    uint8_t current_wcs{0}; // 0 means G54 is enabled this is persistent once saved with M500
    wcs_t g92_offset;
    wcs_t tool_offset; // used for multiple extruders, sets the tool offset for the current extruder applied first
    std::tuple<float, float, float, uint8_t> last_probe_position{0, 0, 0, 0};

    using saved_state_t = std::tuple<float, float, bool, bool, bool, uint8_t>; // save current feedrate and absolute mode, e absolute mode, inch mode, current_wcs
    std::stack<saved_state_t> state_stack;               // saves state from M120

    float machine_position[k_max_actuators]; // Last requested position, in millimeters, which is what we were requested to move to in the gcode after offsets applied but before compensation transform
    float compensated_machine_position[k_max_actuators]; // Last machine position, which is the position before converting to actuator coordinates (includes compensation transform)

    float default_seek_rate;                             // the default seek rate as specified in config.ini
    float seek_rate;                                     // Current rate for seeking moves ( mm/min )
    float feed_rate;                                     // Current rate for feeding moves ( mm/min )
    float mm_per_line_segment;                           // Setting : Used to split lines into segments
    float mm_per_arc_segment;                            // Setting : Used to split arcs into segments
    float mm_max_arc_error;                              // Setting : Used to limit total arc segments to max error
    float delta_segments_per_second;                     // Setting : Used to split lines into segments for delta based on speed
    float seconds_per_minute;                            // for realtime speed change
    float default_acceleration;                          // the defualt accleration if not set for each axis
    float s_value{0.8};                                       // modal S value

    // Number of arc generation iterations by small angle approximation before exact arc trajectory
    // correction. This parameter may be decreased if there are issues with the accuracy of the arc
    // generations. In general, the default value is more than enough for the intended CNC applications
    // of grbl, and should be on the order or greater than the size of the buffer to help with the
    // computational efficiency of generating arcs.
    int arc_correction;                                  // Setting : how often to rectify arc computation
    float max_speeds[3];                                 // Setting : max allowable speed in mm/s for each axis
    float max_speed;                                     // Setting : max allowable speed in mm/s for any move
    float park_position[2];

    uint8_t n_motors;                                    //count of the motors/axis registered

    volatile bool halted{false};
    bool new_status_format{true};
};
