#pragma once

#include "Module.h"
#include "Pin.h"

#include <bitset>
#include <array>
#include <map>

class StepperMotor;
class GCode;
class Pin;
class ConfigReader;
class OutputStream;

class Endstops : public Module
{
    public:
        Endstops();
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);
        bool request(const char *key, void *value);

    private:
        bool load_endstops(ConfigReader& cr);
        void read_endstops();
        void check_limits();

        using axis_bitmap_t = std::bitset<6>;
        void home(axis_bitmap_t a);
        void home_xy();
        void back_off_home(axis_bitmap_t axis);
        void move_to_origin(axis_bitmap_t axis);
        bool debounced_get(Pin *pin);
        void process_home_command(GCode& gcode, OutputStream& os);
        void set_homing_offset(GCode& gcode, OutputStream& os);
        bool handle_G28(GCode& gcode, OutputStream& os);
        bool handle_mcode(GCode& gcode, OutputStream& os);

        // global settings
        uint32_t debounce_ms;
        axis_bitmap_t axis_to_home;

        float trim_mm[3];
        bool limit_enabled{false};

        // per endstop settings
        using endstop_info_t = struct {
            Pin pin;
            struct {
                uint16_t debounce:16;
                char axis:8; // one of XYZABC
                uint8_t axis_index:3;
                bool limit_enable:1;
                bool triggered:1;
            };
        };

        using homing_info_t = struct {
            float homing_position;
            float home_offset;
            float max_travel;
            float retract;
            float fast_rate;
            float slow_rate;
            endstop_info_t *pin_info;

            struct {
                char axis:8; // one of XYZABC
                uint8_t axis_index:3;
                bool home_direction:1; // true min or false max
                bool homed:1;
            };
        };

        // array of endstops
        std::vector<endstop_info_t *> endstops;

        // axis that can be homed, 0,1,2 always there and optionally 3 is A, 4 is B, 5 is C
        std::vector<homing_info_t> homing_axis;

        // Global state
        struct {
            uint32_t homing_order:18;
            volatile char status:3;
            bool is_corexy:1;
            bool is_delta:1;
            bool is_rdelta:1;
            bool is_scara:1;
            bool home_z_first:1;
            bool move_to_origin_after_home:1;
        };
};
