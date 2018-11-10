#include "Block.h"
#include "AxisDefns.h"
#include "StepTicker.h"

uint8_t Block::n_actuators = 0;

// A block represents a movement, it's length for each stepper motor, and the corresponding acceleration curves.
// It's stacked on a queue, and that queue is then executed in order, to move the motors.
// Most of the accel math is also done in this class
// And GCode objects for use in on_gcode_execute are also help in here

Block::Block()
{
    tick_info = nullptr;
    clear();
}

void Block::init(uint8_t n)
{
    n_actuators = n;
}

void Block::clear()
{
    is_ready            = false;

    steps.fill(0);

    steps_event_count   = 0;
    nominal_rate        = 0.0F;
    nominal_speed       = 0.0F;
    millimeters         = 0.0F;
    entry_speed         = 0.0F;
    exit_speed          = 0.0F;
    acceleration        = 100.0F; // we don't want to get divide by zeroes if this is not set
    initial_rate        = 0.0F;
    accelerate_until    = 0;
    decelerate_after    = 0;
    direction_bits      = 0;
    recalculate_flag    = false;
    nominal_length_flag = false;
    max_entry_speed     = 0.0F;
    is_ticking          = false;
    is_g123             = false;
    locked              = false;
    s_value             = 0.0F;

    total_move_ticks = 0;
    if(tick_info == nullptr) {
        // we create this once for this block
        tick_info = new tickinfo_t[n_actuators]; //(tickinfo_t *)malloc(sizeof(tickinfo_t) * n_actuators);
        if(tick_info == nullptr) {
            // if we ran out of memory just stop here
            abort();
        }
    }

    for(int i = 0; i < n_actuators; ++i) {
        tick_info[i].steps_per_tick = 0;
        tick_info[i].counter = 0;
        tick_info[i].acceleration_change = 0;
        tick_info[i].deceleration_change = 0;
        tick_info[i].plateau_rate = 0;
        tick_info[i].steps_to_move = 0;
        tick_info[i].step_count = 0;
        tick_info[i].next_accel_event = 0;
    }
}

void Block::debug() const
{
    printf("%p: steps-X:%lu Y:%lu Z:%lu ", this, this->steps[0], this->steps[1], this->steps[2]);
    for (size_t i = E_AXIS; i < n_actuators; ++i) {
        printf("%c:%lu ", 'A' + i - E_AXIS, this->steps[i]);
    }
    printf("(max:%lu) nominal:r%1.4f/s%1.4f mm:%1.4f acc:%1.2f accu:%lu decu:%lu ticks:%lu rates:%1.4f/%1.4f entry/max:%1.4f/%1.4f exit:%1.4f primary:%d ready:%d locked:%d ticking:%d recalc:%d nomlen:%d time:%f\r\n",
           steps_event_count,
           nominal_rate,
           nominal_speed,
           millimeters,
           acceleration,
           accelerate_until,
           decelerate_after,
           total_move_ticks,
           initial_rate,
           maximum_rate,
           entry_speed,
           max_entry_speed,
           exit_speed,
           primary_axis,
           is_ready,
           locked,
           is_ticking,
           recalculate_flag ? 1 : 0,
           nominal_length_flag ? 1 : 0,
           total_move_ticks / STEP_TICKER_FREQUENCY
          );

    // TODO dump tickinfo
}

// returns current rate (steps/sec) for the given actuator
float Block::get_trapezoid_rate(int i) const
{
    // convert steps per tick from fixed point to float and convert to steps/sec
    // FIXME steps_per_tick can change at any time, potential race condition if it changes while being read here
    return STEPTICKER_FROMFP(tick_info[i].steps_per_tick) * STEP_TICKER_FREQUENCY;
}
