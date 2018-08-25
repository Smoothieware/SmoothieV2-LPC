#include "CurrentControl.h"
#include "ConfigReader.h"
#include "Pwm.h"
#include "GCode.h"
#include "OutputStream.h"
#include "Dispatcher.h"

#include <string>
#include <map>
#include <tuple>

#define current_key "current"
#define control_key "control"
#define pin_key "pin"

static std::map<std::string, char> lut = {
    {"alpha", 'X'},
    {"beta", 'Y'},
    {"gamma", 'Z'},
    {"delta", 'A'},
    {"epsilon", 'B'},
    {"zeta", 'C'}
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


        std::string ctrltype= cr.get_string(m, control_key, "pwm");
        if(ctrltype != "pwm") {
            printf("configure-current-control: %s - only pwm type current control currently supported\n", name.c_str());
            continue;
        }

        // we have a PWM current control
        std::string pin= cr.get_string(m, pin_key, "nc");
        if(pin == "nc") continue;

        Pwm *pwm= new Pwm(pin.c_str());
        if(!pwm->is_valid()) {
            delete pwm;
            printf("configure-current-control: %s - pin %s in not a valid PWM pin\n", name.c_str(), pin.c_str());
            continue;
        }

        // Get current settings
        float c= cr.get_float(m, current_key, -1);
        if(c <= 0) {
            delete pwm;
            printf("configure-current-control: %s - invalid current\n", name.c_str());
            continue;
        }

        currents[name] = c;
        pins[name]= pwm;

        pwm->set(current_to_pwm(c));
        printf("configure-current-control: %s set to %1.5f amps\n", name.c_str(), c);
    }

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 503, std::bind(&CurrentControl::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 907, std::bind(&CurrentControl::handle_gcode, this, _1, _2));

    return currents.size() > 0;
}


bool CurrentControl::handle_gcode(GCode& gcode, OutputStream& os)
{
    if(gcode.get_code() == 907) {
        if(gcode.has_no_args()) {
            for (auto i : currents) {
                os.printf("%s: %1.5f\n", i.first.c_str(), i.second);
            }
            return true;
        }

        // XYZ are the first 3 channels and ABCD are the next channels
        for (int i = 0; i < 7; i++) {
            char axis= i < 3 ? 'X'+i : 'A'+i-3;
            if (gcode.has_arg(axis)) {
                float c = gcode.get_arg(axis);
                std::string p;
                switch(axis) {
                    case 'X': p= "alpha"; break;
                    case 'Y': p= "beta"; break;
                    case 'Z': p= "gamma"; break;
                    case 'A': p= "delta"; break;
                    case 'B': p= "epsilon"; break;
                    case 'C': p= "zeta"; break;
                    default: os.printf("Unknown axis %c\n", axis); continue;
                }

                auto x= pins.find(p);
                if(x == pins.end()){
                    os.printf("axis %c is not configured for current control\n", axis);
                    continue;
                }

                x->second->set(current_to_pwm(c));
                currents[p]= c;
            }
        }

        return true;

    } else if(gcode.get_code() == 503) {
        os.printf(";Motor currents:\nM907 ");
        for (auto i : currents) {
            char axis= lut[i.first];
            os.printf("%c%1.5f ", axis, i.second);
        }
        os.printf("\n");
        return true;
    }

    return false;
}

