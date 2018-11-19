#include "TemperatureControl.h"
#include "TempSensor.h"
#include "SigmaDeltaPwm.h"
#include "Conveyor.h"
#include "OutputStream.h"
#include "GCode.h"
#include "SlowTicker.h"
#include "FastTicker.h"
#include "Dispatcher.h"
#include "main.h"
#include "PID_Autotuner.h"

#include <math.h>
#include <string.h>

// Temp sensor implementations:
#include "Thermistor.h"
//#include "max31855.h"
//#include "AD8495.h"
//#include "PT100_E3D.h"

#define UNDEFINED -1

#define sensor_key "sensor"

#define readings_per_second_key "readings_per_second"
#define max_pwm_key "max_pwm"
#define pwm_frequency_key "pwm_frequency"
#define bang_bang_key "bang_bang"
#define hysteresis_key "hysteresis"
#define heater_pin_key "heater_pin"
#define max_temp_key "max_temp"
#define min_temp_key "min_temp"

#define get_m_code_key "get_m_code"
#define set_m_code_key "set_m_code"
#define set_and_wait_m_code_key "set_and_wait_m_code"

#define designator_key "designator"
#define tool_id_key    "tool_id"

#define p_factor_key "p_factor"
#define i_factor_key "i_factor"
#define d_factor_key "d_factor"

#define i_max_key "i_max"
#define windup_key "windup"

#define preset1_key "preset1"
#define preset2_key "preset2"

#define runaway_range_key "runaway_range"
#define runaway_heating_timeout_key "runaway_heating_timeout"
#define runaway_cooling_timeout_key "runaway_cooling_timeout"
#define runaway_error_range_key "runaway_error_range"

#ifdef BOARD_PRIMEALPHA
Pin *TemperatureControl::vfet_enable_pin{nullptr};
#endif

TemperatureControl::TemperatureControl(const char *name) : Module("temperature control", name)
{
    temp_violated = false;
    sensor = nullptr;
    readonly = false;
    tick = 0;
    active = false;
}

TemperatureControl::~TemperatureControl()
{
    delete sensor;
    delete heater_pin;
}

bool TemperatureControl::load_controls(ConfigReader& cr)
{
    ConfigReader::sub_section_map_t ssmap;
    if(!cr.get_sub_sections("temperature control", ssmap)) {
        printf("configure-temperature control: no section found\n");
        return false;
    }

    int cnt = 0;
    for(auto& i : ssmap) {
        // foreach temp control
        std::string name = i.first;
        auto& m = i.second;
        if(cr.get_bool(m, "enable", false)) {
            TemperatureControl *tc = new TemperatureControl(name.c_str());
            if(tc->configure(cr, m)) {
                // make sure the first (or only) heater is selected
                if(tc->tool_id == 0) tc->active = true;
                if(tc->tool_id == 255) {
                    // config did not set a tool id (eg bed) but we need a unique one so...
                    tc->tool_id = 254 - cnt;
                    tc->active = true; // and they are always active
                }
                ++cnt;
            } else {
                printf("configure-temperature control: failed to configure temperature control %s\n", name.c_str());
                delete tc;
            }
        }
    }

    if(cnt > 0) {
        printf("configure-temperature control: NOTE: %d TemperatureControl(s) configured and enabled\n", cnt);
    } else {
        printf("configure-temperature control: NOTE: no TemperatureControl(s) configured\n");
    }

#ifdef BOARD_PRIMEALPHA
    if(cnt > 0) {
        // turn on the vfet enable
        vfet_enable_pin= new Pin("GPIO4_10", Pin::AS_OUTPUT);
        vfet_enable_pin->set(true);
    }
#endif

    return cnt > 0;
}

bool TemperatureControl::configure(ConfigReader& cr, ConfigReader::section_map_t& m)
{
    // We start not desiring any temp
    this->target_temperature = UNDEFINED;
    this->sensor_settings = false; // set to true if sensor settings have been overriden

    // General config
    this->tool_id             = cr.get_int(m, tool_id_key, 255); // set to 255 by default which is a bed and not controlled by Tx, other extruders should be 0 or 1 etc
    this->set_m_code          = cr.get_int(m, set_m_code_key, tool_id < 100 ? 104 : 140); // hotend vs bed
    this->set_and_wait_m_code = cr.get_int(m, set_and_wait_m_code_key, tool_id < 100 ? 109 : 190); // hotend vs bed
    this->get_m_code          = cr.get_int(m, get_m_code_key, 105);
    this->readings_per_second = cr.get_int(m, readings_per_second_key, 20);
    this->designator          = cr.get_string(m, designator_key, "T");

    // Runaway parameters
    uint32_t n = cr.get_int(m, runaway_range_key, 20);
    if(n > 63) n = 63;
    this->runaway_range = n;

    // TODO probably do not need to pack these anymore
    // these need to fit in 9 bits after dividing by 8 so max is 4088 secs or 68 minutes
    n = cr.get_int(m, runaway_heating_timeout_key, 900);
    if(n > 4088) n = 4088;
    this->runaway_heating_timeout = n / 8; // we have 8 second ticks
    n = cr.get_int(m, runaway_cooling_timeout_key, 0); // disable by default
    if(n > 4088) n = 4088;
    this->runaway_cooling_timeout = n / 8;

    this->runaway_error_range = cr.get_float(m, runaway_error_range_key, 1.0F);

    // Max and min temperatures we are not allowed to get over (Safety)
    this->max_temp = cr.get_float(m, max_temp_key, 300);
    this->min_temp = cr.get_float(m, min_temp_key, 0);

    // Heater pin
    this->heater_pin = new SigmaDeltaPwm();
    this->heater_pin->from_string(cr.get_string(m, heater_pin_key, "nc"));
    if(this->heater_pin->connected()) {
        this->heater_pin->as_output();
        this->readonly = false;

    } else {
        this->readonly = true;
        delete heater_pin;
        heater_pin = nullptr;
    }

    // For backward compatibility, default to a thermistor sensor.
    std::string sensor_type = cr.get_string(m, sensor_key, "thermistor");

    // Instantiate correct sensor
    delete sensor;
    sensor = nullptr; // In case we fail to create a new sensor.
    if(sensor_type.compare("thermistor") == 0) {
        sensor = new Thermistor();
    // } else if(sensor_type.compare("max31855") == 0) { // needs porting
    //     sensor = new Max31855();
        // } else if(sensor_type.compare("ad8495") == 0) {
        //     sensor = new AD8495();
        // } else if(sensor_type.compare("pt100_e3d") == 0) {
        //     sensor = new PT100_E3D();
    } else {
        sensor = new TempSensor(); // A dummy implementation
    }

    // allow sensor to read the config
    if(!sensor->configure(cr, m)) {
        printf("configure-temperature: %s sensor %s failed to configure\n", get_instance_name(), sensor_type.c_str());
        return false;
    }

    this->preset1 = cr.get_float(m, preset1_key, 0);
    this->preset2 = cr.get_float(m, preset2_key, 0);


    // sigma-delta output modulation
    this->o = 0;

    if(!this->readonly) {
        // used to enable bang bang control of heater
        this->use_bangbang = cr.get_bool(m, bang_bang_key, false);
        this->hysteresis = cr.get_float(m, hysteresis_key, 2);
        this->windup = cr.get_bool(m, windup_key, false);
        this->heater_pin->max_pwm( cr.get_float(m, max_pwm_key, 255) );
        this->heater_pin->set(0);
        //set_low_on_debug(heater_pin->port_number, heater_pin->pin);
        // TODO use single fasttimer for all sigma delta
        float freq= cr.get_float(m, pwm_frequency_key, 2000);
        if(freq >= FastTicker::get_min_frequency()) { // if >= 1KHz use FastTicker
            if(FastTicker::getInstance()->attach((uint32_t)freq, std::bind(&SigmaDeltaPwm::on_tick, this->heater_pin)) < 0) {
                printf("configure-temperature: ERROR Fast Ticker was not set (Too slow?)\n");
                return false;
            }

        }else{
            if(SlowTicker::getInstance()->attach((uint32_t)freq, std::bind(&SigmaDeltaPwm::on_tick, this->heater_pin)) < 0) {
                printf("configure-temperature: ERROR Slow Ticker was not set (Too fast?)\n");
                return false;
            }
        }
    }

    // runaway timer
    SlowTicker::getInstance()->attach(1, std::bind(&TemperatureControl::check_runaway, this));

    // sensor reading tick
    SlowTicker::getInstance()->attach( this->readings_per_second, std::bind(&TemperatureControl::thermistor_read_tick, this));
    this->PIDdt = 1.0 / this->readings_per_second;

    // PID
    setPIDp( cr.get_float(m, p_factor_key, 10 ) );
    setPIDi( cr.get_float(m, i_factor_key, 0.3f) );
    setPIDd( cr.get_float(m, d_factor_key, 200) );

    if(!this->readonly) {
        // set to the same as max_pwm by default
        this->i_max = cr.get_float(m, i_max_key, this->heater_pin->max_pwm());
    }

    this->iTerm = 0.0;
    this->lastInput = -1.0;
    this->last_reading = 0.0;

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 6, std::bind(&TemperatureControl::handle_M6,    this, _1, _2));

    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, this->get_m_code, std::bind(&TemperatureControl::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 305, std::bind(&TemperatureControl::handle_mcode, this, _1, _2));

    if(!this->readonly) {
        Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 143, std::bind(&TemperatureControl::handle_mcode, this, _1, _2));
        Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 301, std::bind(&TemperatureControl::handle_mcode, this, _1, _2));
        Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 303, std::bind(&TemperatureControl::handle_autopid, this, _1, _2));
        Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 500, std::bind(&TemperatureControl::handle_mcode, this, _1, _2));

        Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, set_m_code, std::bind(&TemperatureControl::handle_mcode, this, _1, _2));
        Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, set_and_wait_m_code, std::bind(&TemperatureControl::handle_mcode, this, _1, _2));
    }

    return true;
}

void TemperatureControl::on_halt(bool flg)
{
    if(flg) {
        if(readonly) return;

        // turn off heater
        this->o = 0;
        this->heater_pin->set(0);
        this->target_temperature = UNDEFINED;
    }
}

// handle tool change
bool TemperatureControl::handle_M6(GCode& gcode, OutputStream& os)
{
    // this replaces what toolmanager used to do
    if(gcode.has_t()) {
        if(this->tool_id >= 250) return true; // special ids of 250 to 255 means ignore tool change (ie a bed)
        this->active = (gcode.get_int_arg('T') == this->tool_id);

    } else {
        // this is an error as there was no T parameter to tell us what tool was selected
        return false;
    }

    return true;
}

// we no longer have abort auto pid so control X will abort it or kill button
// Also the parameter to select the tool is P not E
// if this is unacceptible then we will have to run autopid in a thread and it will get a lot more complex
bool TemperatureControl::handle_autopid(GCode& gcode, OutputStream& os)
{
    if (gcode.has_arg('P') && gcode.get_int_arg('P') == this->tool_id) {
        //os.printf("Running autopid on toolid %d, control X to abort\n", tool_id);
        PID_Autotuner *autopid = new PID_Autotuner(this);
        // will not return until complete
        autopid->start(gcode, os);
        delete autopid;
        return true;
    }

    return false;
}

bool TemperatureControl::handle_mcode(GCode & gcode, OutputStream & os)
{
    if( gcode.get_code() == this->get_m_code) {
        char buf[32]; // should be big enough for any status
        snprintf(buf, sizeof(buf), "%s:%3.1f /%3.1f @%d ", this->designator.c_str(), this->get_temperature(), ((target_temperature <= 0) ? 0.0 : target_temperature), this->o);
        os.set_prepend_ok();
        os.set_append_nl();
        os.puts(buf);
        return true;
    }

    if (gcode.get_code() == 305) { // set or get sensor settings
        if (gcode.has_arg('S') && (gcode.get_int_arg('S') == this->tool_id)) {
            TempSensor::sensor_options_t args = gcode.get_args();
            args.erase('S'); // don't include the S
            if(args.size() > 0) {
                // set the new options
                if(sensor->set_optional(args)) {
                    this->sensor_settings = true;
                } else {
                    os.printf("Unable to properly set sensor settings, make sure you specify all required values\n");
                }
            } else {
                // don't override
                this->sensor_settings = false;
            }

            return true;

        } else if(!gcode.has_arg('S')) {
            os.printf("%s(S%d): using %s - active: %d\n", this->designator.c_str(), this->tool_id, this->readonly ? "Readonly" : this->use_bangbang ? "Bangbang" : "PID", active);
            sensor->get_raw(os);
            TempSensor::sensor_options_t options;
            if(sensor->get_optional(options)) {
                for(auto &i : options) {
                    // foreach optional value
                    os.printf("%s(S%d): %c %1.18f\n", this->designator.c_str(), this->tool_id, i.first, i.second);
                }
            }
            os.printf("\n");

            return true;
        }

        return false;
    }

    // readonly sensors don't handle the rest
    if(this->readonly) return false;

    if (gcode.get_code() == 143) {
        if (gcode.has_arg('S') && (gcode.get_int_arg('S') == this->tool_id)) {
            if(gcode.has_arg('P')) {
                max_temp = gcode.get_arg('P');

            } else {
                os.printf("Nothing set NOTE Usage is M143 S0 P300 where <S> is the hotend index and <P> is the maximum temp to set\n");
            }

        } else if(gcode.get_num_args() == 0) {
            os.printf("Maximum temperature for %s(%d) is %f°C\n", this->designator.c_str(), this->tool_id, max_temp);
        }

        return true;

    } else if (gcode.get_code() == 301) {
        if (gcode.has_arg('S') && (gcode.get_int_arg('S') == this->tool_id)) {
            if (gcode.has_arg('P'))
                setPIDp( gcode.get_arg('P') );
            if (gcode.has_arg('I'))
                setPIDi( gcode.get_arg('I') );
            if (gcode.has_arg('D'))
                setPIDd( gcode.get_arg('D') );
            if (gcode.has_arg('X'))
                this->i_max = gcode.get_arg('X');
            if (gcode.has_arg('Y'))
                this->heater_pin->max_pwm(gcode.get_arg('Y'));

        } else if(!gcode.has_arg('S')) {
            os.printf("%s(S%d): Pf:%g If:%g Df:%g X(I_max):%g max pwm: %d O:%d\n", this->designator.c_str(), this->tool_id, this->p_factor, this->i_factor / this->PIDdt, this->d_factor * this->PIDdt, this->i_max, this->heater_pin->max_pwm(), o);
        }

        return true;

    } else if (gcode.get_code() == 500) { // M500 saves some volatile settings to config override file
        os.printf(";PID settings, i_max, max_pwm:\nM301 S%d P%1.4f I%1.4f D%1.4f X%1.4f Y%d\n", this->tool_id, this->p_factor, this->i_factor / this->PIDdt, this->d_factor * this->PIDdt, this->i_max, this->heater_pin->max_pwm());

        os.printf(";Max temperature setting:\nM143 S%d P%1.4f\n", this->tool_id, this->max_temp);

        if(this->sensor_settings) {
            // get or save any sensor specific optional values
            TempSensor::sensor_options_t options;
            if(sensor->get_optional(options) && !options.empty()) {
                os.printf(";Optional temp sensor specific settings:\nM305 S%d", this->tool_id);
                for(auto &i : options) {
                    os.printf(" %c%1.18f", i.first, i.second);
                }
                os.printf("\n");
            }
        }

        return true;

    } else if( ( gcode.get_code() == this->set_m_code || gcode.get_code() == this->set_and_wait_m_code ) && gcode.has_arg('S')) {
        // if there is a Tn argument then we use that and ignore if we are active or not
        bool is_selected = gcode.has_arg('T') && gcode.get_int_arg('T') == this->tool_id;

        if(this->tool_id >= 250) this->active = true; // special tool ids are always active (eg bed)

        if( (this->active && !gcode.has_arg('T')) || is_selected) {

            // required so temp change happens in order
            Conveyor::getInstance()->wait_for_idle();

            float v = gcode.get_arg('S');

            if (v == 0.0) {
                this->target_temperature = UNDEFINED;
                this->heater_pin->set((this->o = 0));

            } else {
                this->set_desired_temperature(v);
                // wait for temp to be reached, no more gcodes will be fetched until this is complete
                if( gcode.get_code() == this->set_and_wait_m_code) {
                    // FIXME does isinf work?
                    if(isinf(get_temperature()) && isinf(sensor->get_temperature())) {
                        os.printf("Temperature reading is unreliable on %s HALT asserted - reset or M999 required\n", designator.c_str());
                        broadcast_halt(true);
                        return true;
                    }

                    int cnt = 0;
                    while ( get_temperature() < target_temperature ) {
                        safe_sleep(200); // wait 200 ms
                        // check if ON_HALT was called (usually by kill button)
                        if(is_halted() || this->target_temperature == UNDEFINED) {
                            os.printf("Wait on temperature aborted by kill\n");
                            break;
                        }
                        if(++cnt > 5) {
                            os.printf("%s:%3.1f /%3.1f @%d\n", designator.c_str(), get_temperature(), ((target_temperature <= 0) ? 0.0 : target_temperature), o);
                            cnt = 0;
                        }
                    }
                }
            }

            return true;
        }
    }

    // should return false if we get here
    return false;
}

bool TemperatureControl::request(const char *key, void *value)
{
    if(strcmp(key, "get_current_temperature") == 0) {
        // we are passed a pad_temperature
        pad_temperature_t *t = static_cast<pad_temperature_t *>(value);

        // setup data
        t->current_temperature = this->get_temperature();
        t->target_temperature = (target_temperature <= 0) ? 0 : this->target_temperature;
        t->pwm = this->o;
        t->designator = this->designator;
        t->tool_id = this->tool_id;
        return true;
    }

    if(strcmp(key, "set_temperature") == 0) {
        // NOTE unlike the M code this will set the temp now not when the queue is empty
        float t = *static_cast<float *>(value);
        this->set_desired_temperature(t);
        return true;
    }

    return false;
}


void TemperatureControl::set_desired_temperature(float desired_temperature)
{
    // Never go over the configured max temperature
    if( desired_temperature > this->max_temp ) {
        desired_temperature = this->max_temp;
    }

    if (desired_temperature == 1.0F) {
        desired_temperature = preset1;
    } else if (desired_temperature == 2.0F) {
        desired_temperature = preset2;
    }

    float last_target_temperature = target_temperature;
    target_temperature = desired_temperature;
    if (desired_temperature <= 0.0F) {
        // turning it off
        heater_pin->set((this->o = 0));

    } else if(last_target_temperature <= 0.0F) {
        // if it was off and we are now turning it on we need to initialize
        this->lastInput = last_reading;
        // set to whatever the output currently is See http://brettbeauregard.com/blog/2011/04/improving-the-beginner%E2%80%99s-pid-initialization/
        this->iTerm = this->o;
        if (this->iTerm > this->i_max) this->iTerm = this->i_max;
        else if (this->iTerm < 0.0) this->iTerm = 0.0;
    }

    // reset the runaway state, even if it was a temp change
    this->runaway_state = NOT_HEATING;
}

float TemperatureControl::get_temperature()
{
    return last_reading;
}

void TemperatureControl::thermistor_read_tick()
{
    float temperature = sensor->get_temperature();
    if(!this->readonly && target_temperature > 2) {
        if (isinf(temperature) || temperature < min_temp || temperature > max_temp) {
            target_temperature = UNDEFINED;
            heater_pin->set((this->o = 0));

            // we schedule a call back in command context to print the errors
            char error_msg[132];
            snprintf(error_msg, sizeof(error_msg), "ERROR: MINTEMP or MAXTEMP triggered on %s. Check your temperature sensors!\nHALT asserted - reset or M999 required\n", designator.c_str());
            print_to_all_consoles(error_msg);

            // force into ALARM state
            broadcast_halt(true);

        } else {
            pid_process(temperature);
        }
    }

    last_reading = temperature;
    return;
}

/**
 * Based on https://github.com/br3ttb/Arduino-PID-Library
 */
void TemperatureControl::pid_process(float temperature)
{
    if(use_bangbang) {
        // bang bang is very simple, if temp is < target - hysteresis turn on full else if  temp is > target + hysteresis turn heater off
        // good for relays
        if(temperature > (target_temperature + hysteresis) && this->o > 0) {
            heater_pin->set(false);
            this->o = 0; // for display purposes only

        } else if(temperature < (target_temperature - hysteresis) && this->o <= 0) {
            if(heater_pin->max_pwm() >= 255) {
                // turn on full
                this->heater_pin->set(true);
                this->o = 255; // for display purposes only
            } else {
                // only to whatever max pwm is configured
                this->heater_pin->pwm(heater_pin->max_pwm());
                this->o = heater_pin->max_pwm(); // for display purposes only
            }
        }
        return;
    }

    // regular PID control
    float error = target_temperature - temperature;

    float new_I = this->iTerm + (error * this->i_factor);
    if (new_I > this->i_max) new_I = this->i_max;
    else if (new_I < 0.0) new_I = 0.0;
    if(!this->windup) this->iTerm = new_I;

    float d = (temperature - this->lastInput);

    // calculate the PID output
    // TODO does this need to be scaled by max_pwm/256? I think not as p_factor already does that
    this->o = (this->p_factor * error) + new_I - (this->d_factor * d);

    if (this->o >= heater_pin->max_pwm())
        this->o = heater_pin->max_pwm();
    else if (this->o < 0)
        this->o = 0;
    else if(this->windup)
        this->iTerm = new_I; // Only update I term when output is not saturated.

    this->heater_pin->pwm(this->o);
    this->lastInput = temperature;
}

// called every second
void TemperatureControl::check_runaway()
{
    if(is_halted()) return;

    // see if runaway detection is enabled
    if(this->runaway_heating_timeout == 0 && this->runaway_range == 0) return;

    // check every 8 seconds, depends on tick being 3 bits
    if(++tick != 0) return;

    // Check whether or not there is a temperature runaway issue, if so stop everything and report it

    if(this->target_temperature <= 0) { // If we are not trying to heat, state is NOT_HEATING
        this->runaway_state = NOT_HEATING;

    } else {
        float current_temperature = this->get_temperature();
        // heater is active
        switch( this->runaway_state ) {
            case NOT_HEATING: // If we were previously not trying to heat, but we are now, change to state WAITING_FOR_TEMP_TO_BE_REACHED
                this->runaway_state = (this->target_temperature >= current_temperature || this->runaway_cooling_timeout == 0) ? HEATING_UP : COOLING_DOWN;
                this->runaway_timer = 0;
                tick = 0;
                break;

            case HEATING_UP:
            case COOLING_DOWN:
                // check temp has reached the target temperature within the given error range
                if( (runaway_state == HEATING_UP && current_temperature >= (this->target_temperature - this->runaway_error_range)) ||
                    (runaway_state == COOLING_DOWN && current_temperature <= (this->target_temperature + this->runaway_error_range)) ) {
                    this->runaway_state = TARGET_TEMPERATURE_REACHED;
                    this->runaway_timer = 0;
                    tick = 0;

                } else {
                    uint16_t t = (runaway_state == HEATING_UP) ? this->runaway_heating_timeout : this->runaway_cooling_timeout;
                    // we are still heating up see if we have hit the max time allowed
                    if(t > 0 && ++this->runaway_timer > t) {
                        // this needs to go to any connected terminal, so do it in command thread context
                        char error_msg[132];
                        snprintf(error_msg, sizeof(error_msg), "ERROR: Temperature took too long to be reached on %s, HALT asserted, TURN POWER OFF IMMEDIATELY - reset or M999 required\n", designator.c_str());
                        print_to_all_consoles(error_msg);

                        broadcast_halt(true);
                        this->runaway_state = NOT_HEATING;
                        this->runaway_timer = 0;
                    }
                }
                break;

            case TARGET_TEMPERATURE_REACHED:
                if(this->runaway_range != 0) {
                    // we are in state TARGET_TEMPERATURE_REACHED, check for thermal runaway
                    float delta = current_temperature - this->target_temperature;

                    // If the temperature is outside the acceptable range for 8 seconds, this allows for some noise spikes without halting
                    if(fabsf(delta) > this->runaway_range) {
                        if(this->runaway_timer++ >= 1) { // this being 8 seconds
                            char error_msg[132];
                            snprintf(error_msg, sizeof(error_msg), "ERROR: Temperature runaway on %s (delta temp %f), HALT asserted, TURN POWER OFF IMMEDIATELY - reset or M999 required\n", designator.c_str(), delta);
                            print_to_all_consoles(error_msg);

                            broadcast_halt(true);
                            this->runaway_state = NOT_HEATING;
                            this->runaway_timer = 0;
                            #ifdef BOARD_PRIMEALPHA
                            // as this is a potential mosfet failing on we shut off all mosfets
                            vfet_enable_pin->set(false);
                            print_to_all_consoles("WARNING: All mosfets have been turned off until reset\n");
                            #endif
                        }

                    } else {
                        this->runaway_timer = 0;
                    }
                }

                break;
        }
    }
}

void TemperatureControl::setPIDp(float p)
{
    this->p_factor = p;
}

void TemperatureControl::setPIDi(float i)
{
    this->i_factor = i * this->PIDdt;
}

void TemperatureControl::setPIDd(float d)
{
    this->d_factor = d / this->PIDdt;
}
