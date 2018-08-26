#pragma once

#include "Module.h"
#include "ConfigReader.h"

#include <string>

class OutputStream;
class GCode;

class TemperatureSwitch : public Module
{
public:
    TemperatureSwitch(const char *name);
    ~TemperatureSwitch();
    bool is_armed() const { return armed; }
    bool configure(ConfigReader& cr);
    void in_command_ctx();

private:
    enum TRIGGER_TYPE {LEVEL, RISING, FALLING};
    enum STATE {NONE, HIGH_TEMP, LOW_TEMP};

    bool configure(ConfigReader& cr, ConfigReader::section_map_t& m);
    bool handle_arm(GCode& gcode, OutputStream& os);

    // get the highest temperature from the set of configured temperature controllers
    float get_highest_temperature();

    // turn the switch on or off
    void set_switch(bool cooler_state);

    // temperature has changed state
    void set_state(STATE state);

    // temperatureswitch.hotend.threshold_temp
    float threshold_temp;

    // temperatureswitch.hotend.switch
    std::string switch_name;

    // check temps on heatup every X seconds
    // this can be set in config: temperatureswitch.hotend.heatup_poll
    uint16_t heatup_poll;

    // check temps on cooldown every X seconds
    // this can be set in config: temperatureswitch.hotend.cooldown_poll
    uint16_t cooldown_poll;

    // our internal second counter
    uint16_t second_counter;
    uint32_t last_time;

    // we are delaying for this many seconds
    uint16_t current_delay;

    // the mcode that will arm the switch, 0 means always armed
    uint16_t arm_mcode;

    struct {
        char designator: 8;
        bool inverted: 1;
        bool armed: 1;
        TRIGGER_TYPE trigger: 2;
        STATE current_state: 2;
    };
};
