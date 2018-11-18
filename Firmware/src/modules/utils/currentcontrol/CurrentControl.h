#pragma once

#include "Module.h"

#include <map>
#include <string>

class GCode;
class OutputStream;
class ConfigReader;
class Pwm;

class CurrentControl : public Module {
    public:
        CurrentControl();
        virtual ~CurrentControl() {};
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);

    private:
        bool handle_gcode(GCode& gcode, OutputStream& os);
        std::map<const std::string, float> currents;
        bool set_current(const std::string& name, float current);
        #ifdef BOARD_MINIALPHA
        float current_to_pwm(float c) const { return c/2.0625F; }
        std::map<std::string, Pwm *> pins;
        #endif
};
