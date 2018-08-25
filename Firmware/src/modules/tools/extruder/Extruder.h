#pragma once

#include "Module.h"
#include "Pin.h"
#include "ConfigReader.h"

#include <tuple>

class StepperMotor;
class GCode;
class OutputStream;

class Extruder : public Module
{
public:
    Extruder(const char *name);
    virtual ~Extruder();

    bool configure(ConfigReader& cr);

    void select();
    void deselect();
    float get_e_scale(void) const { return volumetric_multiplier * extruder_multiplier; }

    bool request(const char *key, void *value);
    using pad_extruder_t = struct pad_extruder {
        float steps_per_mm;
        float filament_diameter;
        float flow_rate;
        float accleration;
        float retract_length;
        float current_position;
    };

private:
    bool configure(ConfigReader& cr, ConfigReader::section_map_t& m);
    float check_max_speeds(float target, float isecs);
    void save_position();
    void restore_position();
    bool handle_M6(GCode& gcode, OutputStream& os);
    bool handle_gcode(GCode& gcode, OutputStream& os);
    bool handle_mcode(GCode& gcode, OutputStream& os);

    StepperMotor *stepper_motor;

    float tool_offset[3] {0, 0, 0};

    float extruder_multiplier;          // flow rate 1.0 == 100%
    float filament_diameter;            // filament diameter
    float volumetric_multiplier;
    float max_volumetric_rate;      // used for calculating volumetric rate in mm³/sec

    // for firmware retract
    float retract_length;               // firmware retract length
    float retract_feedrate;
    float retract_recover_feedrate;
    float retract_recover_length;
    float retract_zlift_length;
    float retract_zlift_feedrate;

    // for saving and restoring extruder position
    std::tuple<float, float, int32_t> saved_position;

    uint8_t tool_id{0}; // the tool id for this instance
    struct {
        uint8_t motor_id: 8;
        bool retracted: 1;
        bool cancel_zlift_restore: 1; // hack to stop a G11 zlift restore from overring an absolute Z setting
        bool selected: 1;
        bool saved_selected: 1;
        bool g92e0_detected: 1;
    };
};
