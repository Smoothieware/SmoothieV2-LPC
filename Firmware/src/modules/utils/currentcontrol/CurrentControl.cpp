#include "CurrentControl.h"
#include "ConfigReader.h"
#include "Pwm.h"
#include "GCode.h"
#include "OutputStream.h"
#include "Dispatcher.h"
#include "Robot.h"
#include "StepperMotor.h"

#include <string>
#include <map>
#include <tuple>

#define current_key "current"
#define control_key "control"
#define pin_key "pin"

static std::map<std::string, char> name_lut = {
    {"alpha", 'X'},
    {"beta", 'Y'},
    {"gamma", 'Z'},
    {"delta", 'A'},
    {"epsilon", 'B'},
    {"zeta", 'C'}
};
static std::map<char, std::string> axis_lut = {
    {'X', "alpha"},
    {'Y', "beta"},
    {'Z', "gamma"},
    {'A', "delta"},
    {'B', "epsilon"},
    {'C', "zeta"}
};

CurrentControl::CurrentControl() : Module("currentcontrol")
{
}

bool CurrentControl::configure(ConfigReader& cr)
{
    ConfigReader::sub_section_map_t ssmap;
    if(!cr.get_sub_sections("current control", ssmap)) {
        printf("configure-current-control: no current control section found\n");
        return false;
    }

    for(auto& i : ssmap) {
        // foreach channel
        std::string name = i.first;
        auto& m = i.second;

        // Get current settings
        float c= cr.get_float(m, current_key, -1);
        if(c <= 0) {
            printf("configure-current-control: %s - invalid current\n", name.c_str());
            continue;
        }

#ifdef BOARD_MINIALPHA
        // we have a PWM current control
        std::string pin= cr.get_string(m, pin_key, "nc");
        if(pin == "nc") continue;

        Pwm *pwm= new Pwm(pin.c_str());
        if(!pwm->is_valid()) {
            delete pwm;
            printf("configure-current-control: %s - pin %s in not a valid PWM pin\n", name.c_str(), pin.c_str());
            continue;
        }

        pins[name]= pwm;
        bool ok= set_current(name, c);

#elif defined(BOARD_PRIMEALPHA)
        // SPI defined current control, we ask actuator to deal with it
        bool ok= set_current(name, c);
#else
        bool ok= false;
#endif
        if(ok) {
            printf("configure-current-control: %s set to %1.5f amps\n", name.c_str(), c);
        }else{
            printf("configure-current-control: Failed to set current for %s\n", name.c_str());
        }
    }

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 503, std::bind(&CurrentControl::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 906, std::bind(&CurrentControl::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 907, std::bind(&CurrentControl::handle_gcode, this, _1, _2));

    return currents.size() > 0;
}

bool CurrentControl::set_current(const std::string& name, float current)
{
#ifdef BOARD_MINIALPHA
    auto x= pins.find(name);
    if(x == pins.end()){
        return false;
    }
    x->second->set(current_to_pwm(current));
    bool ok= true;

#elif defined(BOARD_PRIMEALPHA)
    // ask Actuator to set its current
    char axis= name_lut[name];
    int n= axis < 'X' ? axis-'A'+3 : axis-'X';
    if(n >= Robot::getInstance()->get_number_registered_motors()) return false;
    bool ok= Robot::getInstance()->actuators[n]->set_current(current);
#else
    bool ok= false;
#endif

    if(ok) {
        currents[name] = current;
    }

    return ok;
}

bool CurrentControl::handle_gcode(GCode& gcode, OutputStream& os)
{
    if(gcode.get_code() == 906) {
        // set current in mA
        for (int i = 0; i < Robot::getInstance()->get_number_registered_motors(); i++) {
            char axis= i < 3 ? 'X'+i : 'A'+i-3;
            if (gcode.has_arg(axis)) {
                float current= gcode.get_arg(axis);
                std::string& name= axis_lut[axis];
                set_current(name, current/1000.0F);
            }
        }
        return true;

    }else if(gcode.get_code() == 907) {
        if(gcode.has_no_args()) {
            for (auto i : currents) {
                os.printf("%s: %1.5f\n", i.first.c_str(), i.second);
            }
            return true;
        }

        // sets current in Amps
        // XYZ are the first 3 channels and ABC are the next channels
        for (int i = 0; i < 7; i++) {
            char axis= i < 3 ? 'X'+i : 'A'+i-3;
            if (gcode.has_arg(axis)) {
                float c = gcode.get_arg(axis);
                auto s = axis_lut.find(axis);
                if(s == axis_lut.end()) {
                    os.printf("Unknown axis %c\n", axis);
                    break;
                }
                std::string& p= s->second;
                if(!set_current(p, c)){
                    os.printf("axis %c is not configured for current control\n", axis);
                }
            }
        }

        return true;

    } else if(gcode.get_code() == 503) {
        os.printf(";Motor currents:\nM907 ");
        for (auto i : currents) {
            char axis= name_lut[i.first];
            os.printf("%c%1.5f ", axis, i.second);
        }
        os.printf("\n");
        return true;
    }

    return false;
}

