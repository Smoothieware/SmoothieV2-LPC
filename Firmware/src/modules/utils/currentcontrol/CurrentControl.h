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

        bool configure(ConfigReader& cr);
    private:
        bool handle_gcode(GCode& gcode, OutputStream& os);
        float current_to_pwm(float c) const { return c/2.0625F; }
        std::map<std::string, float> currents;
        std::map<std::string, Pwm *> pins;
};
