#include "Switch.h"

#include "GCode.h"
#include "OutputStream.h"
#include "ConfigReader.h"
#include "SlowTicker.h"
#include "FastTicker.h"
#include "SigmaDeltaPwm.h"
#include "Pwm.h"
#include "GCodeProcessor.h"
#include "Dispatcher.h"
#include "main.h"

#include <algorithm>
#include <math.h>
#include <string.h>

#define startup_state_key       "startup_state"
#define startup_value_key       "startup_value"
#define input_pin_key           "input_pin"
#define input_pin_behavior_key  "input_pin_behavior"
#define command_subcode_key     "subcode"
#define input_on_command_key    "input_on_command"
#define input_off_command_key   "input_off_command"
#define output_pin_key          "output_pin"
#define output_type_key         "output_type"
#define max_pwm_key             "max_pwm"
#define output_on_command_key   "output_on_command"
#define output_off_command_key  "output_off_command"
#define failsafe_key            "failsafe_set_to"
#define ignore_onhalt_key       "ignore_on_halt"

// register this module for creation in main
REGISTER_MODULE(Switch, Switch::load_switches)

bool Switch::load_switches(ConfigReader& cr)
{
    printf("DEBUG: configure switches\n");
    ConfigReader::sub_section_map_t ssmap;
    if(!cr.get_sub_sections("switch", ssmap)) {
        printf("configure-switch: no switch section found\n");
        return false;
    }

    int cnt = 0;
    for(auto& i : ssmap) {
        // foreach switch
        std::string name = i.first;
        auto& m = i.second;
        if(cr.get_bool(m, "enable", false)) {
            Switch *sw = new Switch(name.c_str());
            if(sw->configure(cr, m)){
                ++cnt;
            }else{
                printf("configure-switch: failed to configure switch %s\n", name.c_str());
                delete sw;
            }
        }
    }

    printf("INFO: %d switche(s) loaded\n", cnt);

    return cnt > 0;
}

Switch::Switch(const char *name) : Module("switch", name)
{ }

bool Switch::configure(ConfigReader& cr, ConfigReader::section_map_t& m)
{
    this->input_pin.from_string( cr.get_string(m, input_pin_key, "nc") )->as_input();
    this->subcode = cr.get_int(m, command_subcode_key, 0);
    std::string input_on_command = cr.get_string(m, input_on_command_key, "");
    std::string input_off_command = cr.get_string(m, input_off_command_key, "");
    this->output_on_command = cr.get_string(m, output_on_command_key, "");
    this->output_off_command = cr.get_string(m, output_off_command_key, "");

    this->switch_state = cr.get_bool(m, startup_state_key, false);
    this->failsafe = cr.get_int(m, failsafe_key, 0);
    this->ignore_on_halt = cr.get_bool(m, ignore_onhalt_key, false);

    std::string ipb = cr.get_string(m, input_pin_behavior_key, "momentary");
    this->input_pin_behavior = (ipb == "momentary") ? momentary_behavior : toggle_behavior;

    std::string output_pin = cr.get_string(m, output_pin_key, "nc");

    // output pin type
    std::string type = cr.get_string(m, output_type_key, "");
    if(type == "sigmadeltapwm") {
        this->output_type = SIGMADELTA;
        this->sigmadelta_pin = new SigmaDeltaPwm();
        this->sigmadelta_pin->from_string(output_pin)->as_output();
        if(this->sigmadelta_pin->connected()) {
            if(failsafe == 1) {
                //set_high_on_debug(sigmadelta_pin->port_number, sigmadelta_pin->pin);
            } else {
                //set_low_on_debug(sigmadelta_pin->port_number, sigmadelta_pin->pin);
            }
        } else {
            printf("configure-switch: Selected sigmadelta pin invalid - disabled\n");
            this->output_type = NONE;
            delete this->sigmadelta_pin;
            this->sigmadelta_pin = nullptr;
        }

    } else if(type == "digital") {
        this->output_type = DIGITAL;
        this->digital_pin = new Pin();
        this->digital_pin->from_string(output_pin)->as_output();
        if(this->digital_pin->connected()) {
            if(failsafe == 1) {
                //set_high_on_debug(digital_pin->port_number, digital_pin->pin);
            } else {
                //set_low_on_debug(digital_pin->port_number, digital_pin->pin);
            }
        } else {
            printf("configure-switch: Selected digital pin invalid - disabled\n");
            this->output_type = NONE;
            delete this->digital_pin;
            this->digital_pin = nullptr;
        }

    } else if(type == "hwpwm") {
        this->output_type = HWPWM;
        pwm_pin = new Pwm(output_pin.c_str());
        if(failsafe == 1) {
            //set_high_on_debug(pin->port_number, pin->pin);
        } else {
            //set_low_on_debug(pin->port_number, pin->pin);
        }
        if(!pwm_pin->is_valid()) {
            printf("configure-switch: Selected Switch PWM output pin is not PWM capable - disabled");
            delete pwm_pin;
            pwm_pin= nullptr;
            this->output_type = NONE;
        }

    } else {
        this->digital_pin = nullptr;
        this->output_type = NONE;
        if(output_pin != "nc") {
            printf("WARNING: switch config: output pin has no known type: %s\n", type.c_str());
        }
    }

    if(this->output_type == SIGMADELTA) {
        this->sigmadelta_pin->max_pwm(cr.get_int(m,  max_pwm_key, 255));
        this->switch_value = cr.get_int(m, startup_value_key, this->sigmadelta_pin->max_pwm());
        if(this->switch_state) {
            this->sigmadelta_pin->pwm(this->switch_value); // will be truncated to max_pwm
        } else {
            this->sigmadelta_pin->set(false);
        }

    } else if(this->output_type == HWPWM) {
        // We can;t set the PWM here it is global
        // float p = cr.get_float(m, pwm_period_ms_key, 20) * 1000.0F; // ms but fractions are allowed
        // this->pwm_pin->period_us(p);

        // default is 0% duty cycle
        this->switch_value = cr.get_int(m, startup_value_key, 0);
        if(this->switch_state) {
            pwm_pin->set(this->switch_value / 100.0F);
        } else {
            pwm_pin->set(0);
        }

    } else if(this->output_type == DIGITAL) {
        this->digital_pin->set(this->switch_state);
    }

    // Set the on/off command codes
    input_on_command_letter = 0;
    input_off_command_letter = 0;

    if(input_on_command.size() >= 2) {
        input_on_command_letter = input_on_command.front();
        const char *p = input_on_command.c_str();
        p++;
        std::tuple<uint16_t, uint16_t, float> c = GCodeProcessor::parse_code(p);
        input_on_command_code = std::get<0>(c);
        if(std::get<1>(c) != 0) {
            this->subcode = std::get<1>(c); // override any subcode setting
        }
        // add handler for this code
        using std::placeholders::_1;
        using std::placeholders::_2;
        THEDISPATCHER->add_handler(input_on_command_letter == 'G' ? Dispatcher::GCODE_HANDLER : Dispatcher::MCODE_HANDLER, input_on_command_code, std::bind(&Switch::handle_gcode, this, _1, _2));

    }

    if(input_off_command.size() >= 2) {
        input_off_command_letter = input_off_command.front();
        const char *p = input_off_command.c_str();
        p++;
        std::tuple<uint16_t, uint16_t, float> c = GCodeProcessor::parse_code(p);
        input_off_command_code = std::get<0>(c);
        if(std::get<1>(c) != 0) {
            this->subcode = std::get<1>(c); // override any subcode setting
        }
        using std::placeholders::_1;
        using std::placeholders::_2;
        THEDISPATCHER->add_handler(input_off_command_letter == 'G' ? Dispatcher::GCODE_HANDLER : Dispatcher::MCODE_HANDLER, input_off_command_code, std::bind(&Switch::handle_gcode, this, _1, _2));
    }

    if(input_pin.connected()) {
        // set to initial state
        this->input_pin_state = this->input_pin.get();
        if(this->input_pin_behavior == momentary_behavior) {
            // initialize switch state to same as current pin level
            this->switch_state = this->input_pin_state;
        }

        // input pin polling
        // TODO we should only have one of these in Switch and call each switch instance
        SlowTicker::getInstance()->attach(100, std::bind(&Switch::pinpoll_tick, this));
    }

    if(this->output_type == SIGMADELTA) {
        // SIGMADELTA tick
        // TODO We should probably have one timer for all sigmadelta pins
        // TODO we should be allowed to set the frequency for this
        FastTicker::getInstance()->attach(1000, std::bind(&SigmaDeltaPwm::on_tick, this->sigmadelta_pin));
    }

    // for commands we may need to replace _ for space for old configs
    std::replace(output_on_command.begin(), output_on_command.end(), '_', ' '); // replace _ with space
    std::replace(output_off_command.begin(), output_off_command.end(), '_', ' '); // replace _ with space

    return true;
}

std::string Switch::get_info() const
{
    std::string s;

    if(digital_pin != nullptr) {
        s.append("OUTPUT:");
        s.append(digital_pin->to_string());
        s.append(",");

        switch(this->output_type) {
            case DIGITAL: s.append("digital,"); break;
            case SIGMADELTA: s.append("sigmadeltapwm,"); break;
            case HWPWM: s.append("hwpwm,"); break;
            case NONE: s.append("none,"); break;
        }
    }
    if(input_pin.connected()) {
        s.append("INPUT:");
        s.append(input_pin.to_string());
        s.append(",");

        switch(this->input_pin_behavior) {
            case momentary_behavior: s.append("momentary,"); break;
            case toggle_behavior: s.append("toggle,"); break;
        }
    }
    if(input_on_command_letter) {
        s.append("INPUT_ON_COMMAND:");
        s.push_back(input_on_command_letter);
        s.append(std::to_string(input_on_command_code));
        s.append(",");
    }
    if(input_off_command_letter) {
        s.append("INPUT_OFF_COMMAND:");
        s.push_back(input_off_command_letter);
        s.append(std::to_string(input_off_command_code));
        s.append(",");
    }
    if(subcode != 0) {
        s.append("SUBCODE:");
        s.append(std::to_string(subcode));
        s.append(",");
    }
    if(!output_on_command.empty()) {
        s.append("OUTPUT_ON_COMMAND:");
        s.append(output_on_command);
        s.append(",");
    }
    if(!output_off_command.empty()) {
        s.append("OUTPUT_OFF_COMMAND:");
        s.append(output_off_command);
        s.append(",");
    }

    return s;
}

// set the pin to the fail safe value on halt
void Switch::on_halt(bool flg)
{
    if(flg) {
        if(this->ignore_on_halt) return;

        // set pin to failsafe value
        switch(this->output_type) {
            case DIGITAL: this->digital_pin->set(this->failsafe); break;
            case SIGMADELTA: this->sigmadelta_pin->set(this->failsafe); break;
            case HWPWM: this->pwm_pin->set(0); break;
            case NONE: break;
        }
        this->switch_state = this->failsafe;
    }
}

bool Switch::match_input_on_gcode(const GCode& gcode) const
{
    bool b = ((input_on_command_letter == 'M' && gcode.has_m() && gcode.get_code() == input_on_command_code) ||
              (input_on_command_letter == 'G' && gcode.has_g() && gcode.get_code() == input_on_command_code));

    return (b && gcode.get_subcode() == this->subcode);
}

bool Switch::match_input_off_gcode(const GCode& gcode) const
{
    bool b = ((input_off_command_letter == 'M' && gcode.has_m() && gcode.get_code() == input_off_command_code) ||
              (input_off_command_letter == 'G' && gcode.has_g() && gcode.get_code() == input_off_command_code));
    return (b && gcode.get_subcode() == this->subcode);
}

// this is always called in the command thread context
bool Switch::handle_gcode(GCode& gcode, OutputStream& os)
{
    // Add the gcode to the queue ourselves if we need it
    if (!(match_input_on_gcode(gcode) || match_input_off_gcode(gcode))) {
        return false;
    }

    // TODO we need to sync this with the queue, so we need to wait for queue to empty, however due to certain slicers
    // issuing redundant switch on calls regularly we need to optimize by making sure the value is actually changing
    // hence we need to do the wait for queue in each case rather than just once at the start
    if(match_input_on_gcode(gcode)) {
        if (this->output_type == SIGMADELTA) {
            // SIGMADELTA output pin turn on (or off if S0)
            if(gcode.has_arg('S')) {
                int v = roundf(gcode.get_arg('S') * sigmadelta_pin->max_pwm() / 255.0F); // scale by max_pwm so input of 255 and max_pwm of 128 would set value to 128
                if(v != this->sigmadelta_pin->get_pwm()) { // optimize... ignore if already set to the same pwm
                    // drain queue
                    //THEKERNEL->conveyor->wait_for_idle();
                    this->sigmadelta_pin->pwm(v);
                    this->switch_state = (v > 0);
                }
            } else {
                // drain queue
                //THEKERNEL->conveyor->wait_for_idle();
                this->sigmadelta_pin->pwm(this->switch_value);
                this->switch_state = (this->switch_value > 0);
            }

        } else if (this->output_type == HWPWM) {
            // drain queue
            //THEKERNEL->conveyor->wait_for_idle();
            // PWM output pin set duty cycle 0 - 100
            if(gcode.has_arg('S')) {
                float v = gcode.get_arg('S');
                if(v > 100) v = 100;
                else if(v < 0) v = 0;
                this->pwm_pin->set(v / 100.0F);
                this->switch_state = (v != 0);
            } else {
                this->pwm_pin->set(this->switch_value);
                this->switch_state = (this->switch_value != 0);
            }

        } else if (this->output_type == DIGITAL) {
            // drain queue
            //THEKERNEL->conveyor->wait_for_idle();
            // logic pin turn on
            this->digital_pin->set(true);
            this->switch_state = true;
        }

    } else if(match_input_off_gcode(gcode)) {
        // drain queue
        //THEKERNEL->conveyor->wait_for_idle();
        this->switch_state = false;

        if (this->output_type == SIGMADELTA) {
            // SIGMADELTA output pin
            this->sigmadelta_pin->set(false);

        } else if (this->output_type == HWPWM) {
            this->pwm_pin->set(0);

        } else if (this->output_type == DIGITAL) {
            // logic pin turn off
            this->digital_pin->set(false);
        }
    }

    // TODO could just call handle_switch_changed() instead of duplicating above

    return true;
}

// this can only be called from the command thread context
bool Switch::request(const char *key, void *value)
{
    if(strcmp(key, "state") == 0) {
        *(bool *)value = this->switch_state;

    } else if(strcmp(key, "set-state") == 0) {
        // TODO should we check and see if we are already in this state and ignore if we are?
        this->switch_state = *(bool*)value;
        handle_switch_changed();

        // if there is no gcode to be sent then we can do this now (in on_idle)
        // Allows temperature switch to turn on a fan even if main loop is blocked with heat and wait
        //if(this->output_on_command.empty() && this->output_off_command.empty()) on_main_loop(nullptr);

    } else if(strcmp(key, "value") == 0) {
        *(float *)value = this->switch_value;

    } else if(strcmp(key, "set-value") == 0) {
        // TODO should we check and see if we already have this value and ignore if we do?
        this->switch_value = *(float*)value;
        handle_switch_changed();

    } else {
        return false;
    }

    return true;
}

// This is called periodically to allow commands to be issued in the command thread context
// but only when want_command_ctx is set to true
void Switch::in_command_ctx()
{
    handle_switch_changed();
    want_command_ctx= false;
}

// Switch changed by some external means so do the action required
// needs to be only called when in command thread context
void Switch::handle_switch_changed()
{
    if(this->switch_state) {
        if(!this->output_on_command.empty()){
            OutputStream os; // null output stream
            dispatch_line(os, this->output_on_command.c_str());
        }

        if(this->output_type == SIGMADELTA) {
            this->sigmadelta_pin->pwm(this->switch_value); // this requires the value has been set otherwise it switches on to whatever it last was

        } else if (this->output_type == HWPWM) {
            this->pwm_pin->set(this->switch_value / 100.0F);

        } else if (this->output_type == DIGITAL) {
            this->digital_pin->set(true);
        }

    } else {
        if(!this->output_off_command.empty()){
            OutputStream os; // null output stream
            dispatch_line(os, this->output_off_command.c_str());
        }

        if(this->output_type == SIGMADELTA) {
            this->sigmadelta_pin->set(false);

        } else if (this->output_type == HWPWM) {
            this->pwm_pin->set(0);

        } else if (this->output_type == DIGITAL) {
            this->digital_pin->set(false);
        }
    }
}

// Check the state of the button and act accordingly
// This is an ISR
// we need to protect switch_state from concurrent access so it is an atomic_bool
// this just sets the state and lets handle_switch_changed() change the actual pins
// TODO however if there is no output_on_command and output_off_command set it could set the pins here instead
// FIXME there is a race condition where if the button is pressed and released faster than the command loop runs then it will not see the button as active
void Switch::pinpoll_tick()
{
    if(!input_pin.connected()) return;

    bool switch_changed = false;

    // See if pin changed
    bool current_state = this->input_pin.get();
    if(this->input_pin_state != current_state) {
        this->input_pin_state = current_state;
        // If pin high
        if( this->input_pin_state ) {
            // if switch is a toggle switch
            if( this->input_pin_behavior == toggle_behavior ) {
                this->switch_state= !this->switch_state;

            } else {
                // else default is momentary
                this->switch_state = this->input_pin_state;
            }
            switch_changed = true;

        } else {
            // else if button released
            if( this->input_pin_behavior == momentary_behavior ) {
                // if switch is momentary
                this->switch_state = this->input_pin_state;
                switch_changed = true;
            }
        }
    }

    if(switch_changed) {
        // we need to call handle_switch_changed but in the command thread context
        // in case there is a command to be issued
        want_command_ctx= true;
    }
}
