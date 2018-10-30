#include "StepperMotor.h"
#include "StepTicker.h"

#include <math.h>

StepperMotor::StepperMotor(Pin &step, Pin &dir, Pin &en) : step_pin(step), dir_pin(dir), en_pin(en)
{
    if(en.connected()) {
        //set_high_on_debug(en.port_number, en.pin);
    }

    steps_per_mm         = 1.0F;
    max_rate             = 50.0F;

    last_milestone_steps = 0;
    last_milestone_mm    = 0.0F;
    current_position_steps= 0;
    moving= false;
    acceleration= NAN;
    selected= true;
    extruder= false;
    vbblost= false;

    enable(false);
    unstep(); // initialize step pin
    set_direction(false); // initialize dir pin
}

StepperMotor::~StepperMotor()
{
}

void StepperMotor::change_steps_per_mm(float new_steps)
{
    steps_per_mm = new_steps;
    last_milestone_steps = roundf(last_milestone_mm * steps_per_mm);
    current_position_steps = last_milestone_steps;
}

void StepperMotor::change_last_milestone(float new_milestone)
{
    last_milestone_mm = new_milestone;
    last_milestone_steps = roundf(last_milestone_mm * steps_per_mm);
    current_position_steps = last_milestone_steps;
}

void StepperMotor::set_last_milestones(float mm, int32_t steps)
{
    last_milestone_mm= mm;
    last_milestone_steps= steps;
    current_position_steps= last_milestone_steps;
}

void StepperMotor::update_last_milestones(float mm, int32_t steps)
{
    last_milestone_steps += steps;
    last_milestone_mm = mm;
}

int32_t StepperMotor::steps_to_target(float target)
{
    int32_t target_steps = roundf(target * steps_per_mm);
    return target_steps - last_milestone_steps;
}

// Does a manual step pulse, used for direct encoder control of a stepper
// NOTE manual step is experimental and may change and/or be removed in the future, it is an unsupported feature.
// use at your own risk
void StepperMotor::manual_step(bool dir)
{
    if(!is_enabled()) enable(true);

    // set direction if needed
    if(this->direction != dir) {
        this->direction= dir;
        this->dir_pin.set(dir);
        //wait_us(1);
    }

    // pulse step pin
    this->step_pin.set(1);
    //wait_us(3);
    this->step_pin.set(0);


    // keep track of actuators actual position in steps
    this->current_position_steps += (dir ? -1 : 1);
}


#ifdef BOARD_PRIMEALPHA
// prime Alpha has TMC2660 drivers so this handles the setup of those drivers
#include "TMC26X.h"

bool StepperMotor::setup_tmc2660(ConfigReader& cr, const char *actuator_name)
{
    char axis= motor_id<3?'X'+motor_id:'A'+motor_id-3;
    tmc2660= new TMC26X(axis);
    if(!tmc2660->config(cr, actuator_name)) {
        delete tmc2660;
        return false;
    }
    tmc2660->init();

    return true;
}

bool StepperMotor::init_tmc2660()
{
    tmc2660->init();
    return true;
}

bool StepperMotor::set_current(float c)
{
    // send current to TMC2660
    tmc2660->setCurrent(c*1000.0F); // sets current in milliamps
    return true;
}

bool StepperMotor::set_microsteps(uint16_t ms)
{
    tmc2660->setMicrosteps(ms); // sets microsteps
    return true;
}

int StepperMotor::get_microsteps()
{
    return tmc2660->getMicrosteps();
}

void StepperMotor::enable(bool state)
{
    // TODO if we have lost Vbb since last time then we need to re load all the drivers configs
    if(state && vbblost) {
        tmc2660->init();
        vbblost= false;
    }
    tmc2660->setEnabled(state);
}

bool StepperMotor::is_enabled() const
{
    return tmc2660->isEnabled();
}

void StepperMotor::dump_status(OutputStream& os, bool flag)
{
    tmc2660->dump_status(os, flag);
}

void StepperMotor::set_raw_register(OutputStream& os, uint32_t reg, uint32_t val)
{
    tmc2660->set_raw_register(os, reg, val);
}

bool StepperMotor::set_options(GCode& gcode)
{
    return tmc2660->set_options(gcode);
}


#else

//Minialpha has enable pins on the drivers

void StepperMotor::enable(bool state)
{
    en_pin.set(!state);
}

bool StepperMotor::is_enabled() const
{
    return !en_pin.get();
}

#endif
