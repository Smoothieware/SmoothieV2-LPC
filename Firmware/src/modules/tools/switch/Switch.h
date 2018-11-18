#pragma once

#include "Module.h"
#include "ConfigReader.h"
#include "Pin.h"


#include <string>
#include <atomic>

class GCode;
class OutputStream;
class SigmaDeltaPwm;
class Pwm;

class Switch : public Module {
    public:
        Switch(const char *name);

        static bool load_switches(ConfigReader& cr);

        void on_halt(bool);
        void in_command_ctx();
        bool request(const char *key, void *value) ;

        enum OUTPUT_TYPE {NONE, SIGMADELTA, DIGITAL, HWPWM};
        std::string get_info() const;

    private:
        bool configure(ConfigReader& cr, ConfigReader::section_map_t& m);
        void pinpoll_tick(void);

        bool handle_gcode(GCode& gcode, OutputStream& os);
        void handle_switch_changed();
        bool match_input_on_gcode(const GCode& gcode) const;
        bool match_input_off_gcode(const GCode& gcode) const;

        Pin       input_pin;
        float     switch_value;
        OUTPUT_TYPE output_type;
        union {
            Pin *digital_pin;
            SigmaDeltaPwm *sigmadelta_pin;
            Pwm *pwm_pin;
        };
        std::string    output_on_command;
        std::string    output_off_command;
        enum {momentary_behavior, toggle_behavior};
        uint16_t  input_pin_behavior;
        uint16_t  input_on_command_code;
        uint16_t  input_off_command_code;
        char      input_on_command_letter;
        char      input_off_command_letter;
        uint8_t   subcode;
        bool      ignore_on_halt;
        uint8_t   failsafe;

        // only accessed in ISR
        bool input_pin_state;
        bool switch_state;
};

