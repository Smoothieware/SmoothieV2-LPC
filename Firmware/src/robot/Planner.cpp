#include "Planner.h"
#include "Block.h"
#include "PlannerQueue.h"
#include "ConfigReader.h"
#include "StepperMotor.h"
#include "AxisDefns.h"
#include "StepTicker.h"
#include "Robot.h"
#include "Conveyor.h"
#include "main.h"
#include "Module.h"

#include <math.h>
#include <algorithm>
#include <vector>

#define junction_deviation_key    "junction_deviation"
#define z_junction_deviation_key  "z_junction_deviation"
#define minimum_planner_speed_key "minimum_planner_speed"
#define planner_queue_size_key    "planner_queue_size"

Planner *Planner::instance= nullptr;

// The Planner does the acceleration math for the queue of Blocks ( movements ).
// It makes sure the speed stays within the configured constraints ( acceleration, junction_deviation, etc )
// It goes over the list in both direction, every time a block is added, re-doing the math to make sure everything is optimal

Planner::Planner()
{
    if(instance == nullptr) instance= this;
    memset(this->previous_unit_vec, 0, sizeof this->previous_unit_vec);
    fp_scale = (double)STEPTICKER_FPSCALE / pow((double)STEP_TICKER_FREQUENCY, 2.0); // we scale up by fixed point offset first to avoid tiny values
}

// Configure acceleration
bool Planner::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(cr.get_section("planner", m)) {
        xy_junction_deviation = cr.get_float(m, junction_deviation_key, 0.05F);
        z_junction_deviation = cr.get_float(m, z_junction_deviation_key, -1);
        minimum_planner_speed = cr.get_float(m, minimum_planner_speed_key, 0.0f);
        planner_queue_size= cr.get_int(m, planner_queue_size_key, 32);

    }else{
        printf("WARNING: configure-planner: no planner section found. defaults loaded\n");
    }

    return true;
}

bool Planner::initialize(uint8_t n)
{
    Block::init(n); // set the number of motors which determines how big the tick info vector is
    queue= new PlannerQueue(planner_queue_size);
    return queue != nullptr;
}

// Append a block to the queue, compute it's speed factors
bool Planner::append_block(ActuatorCoordinates& actuator_pos, uint8_t n_motors, float rate_mm_s, float distance, float *unit_vec, float acceleration, float s_value, bool g123)
{
    // get the head block
    Block* block = queue->get_head();
    block->clear();

    // Direction bits
    bool has_steps = false;
    for (size_t i = 0; i < n_motors; i++) {
        int32_t steps = Robot::getInstance()->actuators[i]->steps_to_target(actuator_pos[i]);
        // Update current position
        if(steps != 0) {
            Robot::getInstance()->actuators[i]->update_last_milestones(actuator_pos[i], steps);
            has_steps = true;
        }

        // find direction
        block->direction_bits[i] = (steps < 0) ? 1 : 0;
        // save actual steps in block
        block->steps[i] = labs(steps);
    }

    // sometimes even though there is a detectable movement it turns out there are no steps to be had from such a small move
    // However we return true as we need to accumulate the move
    if(!has_steps) {
        block->clear();
        return true;
    }

    // info needed by laser
    block->s_value = roundf(s_value*(1<<11)); // 1.11 fixed point
    block->is_g123 = g123;

    // use default JD
    float junction_deviation = this->xy_junction_deviation;

    // use either regular junction deviation or z specific and see if a primary axis move
    block->primary_axis = true;
    if(block->steps[ALPHA_STEPPER] == 0 && block->steps[BETA_STEPPER] == 0) {
        if(block->steps[GAMMA_STEPPER] != 0) {
            // z only move
            if(this->z_junction_deviation >= 0.0F) junction_deviation = this->z_junction_deviation;

        } else {
            // is not a primary axis move
            block->primary_axis= false;
            #if N_PRIMARY_AXIS > 3
                for (int i = 3; i < N_PRIMARY_AXIS; ++i) {
                    if(block->steps[i] != 0){
                        block->primary_axis= true;
                        break;
                    }
                }
            #endif

        }
    }

    block->acceleration = acceleration; // save in block

    // Max number of steps, for all axes
    auto mi = std::max_element(block->steps.begin(), block->steps.end());
    block->steps_event_count = *mi;

    block->millimeters = distance;

    // Calculate speed in mm/sec for each axis. No divide by zero due to previous checks.
    if( distance > 0.0F ) {
        block->nominal_speed = rate_mm_s;           // (mm/s) Always > 0
        block->nominal_rate = block->steps_event_count * rate_mm_s / distance; // (step/s) Always > 0
    } else {
        block->nominal_speed = 0.0F;
        block->nominal_rate  = 0;
    }

    // Compute the acceleration rate for the trapezoid generator. Depending on the slope of the line
    // average travel per step event changes. For a line along one axis the travel per step event
    // is equal to the travel/step in the particular axis. For a 45 degree line the steppers of both
    // axes might step for every step event. Travel per step event is then sqrt(travel_x^2+travel_y^2).

    // Compute maximum allowable entry speed at junction by centripetal acceleration approximation.
    // Let a circle be tangent to both previous and current path line segments, where the junction
    // deviation is defined as the distance from the junction to the closest edge of the circle,
    // colinear with the circle center. The circular segment joining the two paths represents the
    // path of centripetal acceleration. Solve for max velocity based on max acceleration about the
    // radius of the circle, defined indirectly by junction deviation. This may be also viewed as
    // path width or max_jerk in the previous grbl version. This approach does not actually deviate
    // from path, but used as a robust way to compute cornering speeds, as it takes into account the
    // nonlinearities of both the junction angle and junction velocity.

    // NOTE however it does not take into account independent axis, in most cartesian X and Y and Z are totally independent
    // and this allows one to stop with little to no decleration in many cases. This is particualrly bad on leadscrew based systems that will skip steps.
    float vmax_junction = minimum_planner_speed; // Set default max junction speed

    // if unit_vec was null then it was not a primary axis move so we skip the junction deviation stuff
    if (unit_vec != nullptr && !queue->empty()) {
        queue->start_iteration(); // reset to head
        Block *prev_block = queue->tailward_get(); // gets block prior to head, ie last block
        float previous_nominal_speed = prev_block->primary_axis ? prev_block->nominal_speed : 0;

        if (junction_deviation > 0.0F && previous_nominal_speed > 0.0F) {
            // Compute cosine of angle between previous and current path. (prev_unit_vec is negative)
            // NOTE: Max junction velocity is computed without sin() or acos() by trig half angle identity.
            float cos_theta = - this->previous_unit_vec[X_AXIS] * unit_vec[X_AXIS]
                              - this->previous_unit_vec[Y_AXIS] * unit_vec[Y_AXIS]
                              - this->previous_unit_vec[Z_AXIS] * unit_vec[Z_AXIS];
            #if N_PRIMARY_AXIS > 3
                for (int i = 3; i < N_PRIMARY_AXIS; ++i) {
                    cos_theta -= this->previous_unit_vec[i] * unit_vec[i];
                }
            #endif

            // Skip and use default max junction speed for 0 degree acute junction.
            if (cos_theta <= 0.9999F) {
                vmax_junction = std::min(previous_nominal_speed, block->nominal_speed);
                // Skip and avoid divide by zero for straight junctions at 180 degrees. Limit to min() of nominal speeds.
                if (cos_theta >= -0.9999F) {
                    // Compute maximum junction velocity based on maximum acceleration and junction deviation
                    float sin_theta_d2 = sqrtf(0.5F * (1.0F - cos_theta)); // Trig half angle identity. Always positive.
                    vmax_junction = std::min(vmax_junction, sqrtf(acceleration * junction_deviation * sin_theta_d2 / (1.0F - sin_theta_d2)));
                }
            }
        }
    }
    block->max_entry_speed = vmax_junction;

    // Initialize block entry speed. Compute based on deceleration to user-defined minimum_planner_speed.
    float v_allowable = max_allowable_speed(-acceleration, minimum_planner_speed, block->millimeters);
    block->entry_speed = std::min(vmax_junction, v_allowable);

    // Initialize planner efficiency flags
    // Set flag if block will always reach maximum junction speed regardless of entry/exit speeds.
    // If a block can de/ac-celerate from nominal speed to zero within the length of the block, then
    // the current block and next block junction speeds are guaranteed to always be at their maximum
    // junction speeds in deceleration and acceleration, respectively. This is due to how the current
    // block nominal speed limits both the current and next maximum junction speeds. Hence, in both
    // the reverse and forward planners, the corresponding block junction speed will always be at the
    // the maximum junction speed and may always be ignored for any speed reduction checks.
    if (block->nominal_speed <= v_allowable) { block->nominal_length_flag = true; }
    else { block->nominal_length_flag = false; }

    // Always calculate trapezoid for new block
    block->recalculate_flag = true;

    // Update previous path unit_vector and nominal speed
    if(unit_vec != nullptr) {
        memcpy(previous_unit_vec, unit_vec, sizeof(previous_unit_vec)); // previous_unit_vec[] = unit_vec[]
    } else {
        memset(previous_unit_vec, 0, sizeof(previous_unit_vec));
    }

    // Math-heavy re-computing of the whole queue to take the new
    this->recalculate();

    // The block can now be used
    block->ready();

    //block->debug();

    while(!queue->queue_head()) {
        // queue is full
        // stall the command thread until we have room in the queue
        safe_sleep(10); // is 10ms a good stall time?

        if(Module::is_halted()) {
            // we do not want to stick more stuff on the queue if we are in halt state
            // clear the block on the head
            block->clear();
            return false; // if we got a halt then we are done here
        }

        // we check the queue to see if it is ready to run
        Conveyor::getInstance()->check_queue();
    }

    return true;
}

void Planner::recalculate()
{
    Block* previous;
    Block* current;

    /*
     * a newly added block is decel limited
     *
     * we find its max entry speed given its exit speed
     *
     * for each block, walking backwards in the queue:
     *
     * if max entry speed == current entry speed
     * then we can set recalculate to false, since clearly adding another block didn't allow us to enter faster
     * and thus we don't need to check entry speed for this block any more
     *
     * once we find an accel limited block, we must find the max exit speed and walk the queue forwards
     *
     * for each block, walking forwards in the queue:
     *
     * given the exit speed of the previous block and our own max entry speed
     * we can tell if we're accel or decel limited (or coasting)
     *
     * if prev_exit > max_entry
     *     then we're still decel limited. update previous trapezoid with our max entry for prev exit
     * if max_entry >= prev_exit
     *     then we're accel limited. set recalculate to false, work out max exit speed
     *
     * finally, work out trapezoid for the final (and newest) block.
     */

    /*
     * Step 1:
     * For each block, given the exit speed and acceleration, find the maximum entry speed
     */

    float entry_speed = minimum_planner_speed;

    // start the iteration at the head
    queue->start_iteration();
    current = queue->get_head();

    if (!queue->empty()) {
        while (!queue->is_at_tail() && current->recalculate_flag) {
            entry_speed = reverse_pass(current, entry_speed);
            current = queue->tailward_get(); // walk towards the tail
        }

        /*
         * Step 2:
         * now current points to either tail or first non-recalculate block
         * and has not had its reverse_pass called
         * or its calculate_trapezoid
         * entry_speed is set to the *exit* speed of current.
         * each block from current to head has its entry speed set to its max entry speed- limited by decel or nominal_rate
         */

        float exit_speed = max_exit_speed(current);

        while (!queue->is_at_head()) {
            previous = current;
            current = queue->headward_get();

            // we pass the exit speed of the previous block
            // so this block can decide if it's accel or decel limited and update its fields as appropriate
            exit_speed = forward_pass(current, exit_speed);

            calculate_trapezoid(previous, previous->entry_speed, current->entry_speed);
        }
    }

    /*
     * Step 3:
     * work out trapezoid for final (and newest) block
     */

    // now current points to the head item
    // which has not had calculate_trapezoid run yet
    calculate_trapezoid(current, current->entry_speed, minimum_planner_speed);
}

// Calculates the maximum allowable speed at this point when you must be able to reach target_velocity using the
// acceleration within the allotted distance.
float Planner::max_allowable_speed(float acceleration, float target_velocity, float distance)
{
    // Was acceleration*60*60*distance, in case this breaks, but here we prefer to use seconds instead of minutes
    return(sqrtf(target_velocity * target_velocity - 2.0F * acceleration * distance));
}


/* Calculates trapezoid parameters so that the entry- and exit-speed is compensated by the provided factors.
// The factors represent a factor of braking and must be in the range 0.0-1.0.
//                                +--------+ <- nominal_rate
//                               /          \
// nominal_rate*entry_factor -> +            \
//                              |             + <- nominal_rate*exit_factor
//                              +-------------+
//                                  time -->
*/
void Planner::calculate_trapezoid(Block *block, float entryspeed, float exitspeed )
{
    // if block is currently executing, don't touch anything!
    if (block->is_ticking) return;

    float initial_rate = block->nominal_rate * (entryspeed / block->nominal_speed); // steps/sec
    float final_rate = block->nominal_rate * (exitspeed / block->nominal_speed);
    //printf("Initial rate: %f, final_rate: %f\n", initial_rate, final_rate);
    // How many steps ( can be fractions of steps, we need very precise values ) to accelerate and decelerate
    // This is a simplification to get rid of rate_delta and get the steps/s² accel directly from the mm/s² accel
    float acceleration_per_second = (block->acceleration * block->steps_event_count) / block->millimeters;

    float maximum_possible_rate = sqrtf( ( block->steps_event_count * acceleration_per_second ) + ( ( powf(initial_rate, 2) + powf(final_rate, 2) ) / 2.0F ) );

    //printf("id %d: acceleration_per_second: %f, maximum_possible_rate: %f steps/sec, %f mm/sec\n", block->id, acceleration_per_second, maximum_possible_rate, maximum_possible_rate/100);

    // Now this is the maximum rate we'll achieve this move, either because
    // it's the higher we can achieve, or because it's the higher we are
    // allowed to achieve
    block->maximum_rate = std::min(maximum_possible_rate, block->nominal_rate);

    // Now figure out how long it takes to accelerate in seconds
    float time_to_accelerate = ( block->maximum_rate - initial_rate ) / acceleration_per_second;

    // Now figure out how long it takes to decelerate
    float time_to_decelerate = ( final_rate -  block->maximum_rate ) / -acceleration_per_second;

    // Now we know how long it takes to accelerate and decelerate, but we must
    // also know how long the entire move takes so we can figure out how long
    // is the plateau if there is one
    float plateau_time = 0;

    // Only if there is actually a plateau ( we are limited by nominal_rate )
    if(maximum_possible_rate > block->nominal_rate) {
        // Figure out the acceleration and deceleration distances ( in steps )
        float acceleration_distance = ( ( initial_rate + block->maximum_rate ) / 2.0F ) * time_to_accelerate;
        float deceleration_distance = ( ( block->maximum_rate + final_rate ) / 2.0F ) * time_to_decelerate;

        // Figure out the plateau steps
        float plateau_distance = block->steps_event_count - acceleration_distance - deceleration_distance;

        // Figure out the plateau time in seconds
        plateau_time = plateau_distance / block->maximum_rate;
    }

    // Figure out how long the move takes total ( in seconds )
    float total_move_time = time_to_accelerate + time_to_decelerate + plateau_time;
    //puts "total move time: #{total_move_time}s time to accelerate: #{time_to_accelerate}, time to decelerate: #{time_to_decelerate}"

    // We now have the full timing for acceleration, plateau and deceleration,
    // yay \o/ Now this is very important these are in seconds, and we need to
    // round them into ticks. This means instead of accelerating in 100.23
    // ticks we'll accelerate in 100 ticks. Which means to reach the exact
    // speed we want to reach, we must figure out a new/slightly different
    // acceleration/deceleration to be sure we accelerate and decelerate at
    // the exact rate we want

    // First off round total time, acceleration time and deceleration time in ticks
    uint32_t acceleration_ticks = floorf( time_to_accelerate * STEP_TICKER_FREQUENCY );
    uint32_t deceleration_ticks = floorf( time_to_decelerate * STEP_TICKER_FREQUENCY );
    uint32_t total_move_ticks   = floorf( total_move_time    * STEP_TICKER_FREQUENCY );

    // Now deduce the plateau time for those new values expressed in tick
    //uint32_t plateau_ticks = total_move_ticks - acceleration_ticks - deceleration_ticks;

    // Now we figure out the acceleration value to reach EXACTLY maximum_rate(steps/s) in EXACTLY acceleration_ticks(ticks) amount of time in seconds
    float acceleration_time = acceleration_ticks / STEP_TICKER_FREQUENCY;  // This can be moved into the operation below, separated for clarity, note we need to do this instead of using time_to_accelerate(seconds) directly because time_to_accelerate(seconds) and acceleration_ticks(seconds) do not have the same value anymore due to the rounding
    float deceleration_time = deceleration_ticks / STEP_TICKER_FREQUENCY;

    float acceleration_in_steps = (acceleration_time > 0.0F ) ? ( block->maximum_rate - initial_rate ) / acceleration_time : 0;
    float deceleration_in_steps =  (deceleration_time > 0.0F ) ? ( block->maximum_rate - final_rate ) / deceleration_time : 0;

    // we have a potential race condition here as we could get interrupted anywhere in the middle of this call, we need to lock
    // the updates to the blocks to get around it
    block->locked= true;
    // Now figure out the two acceleration ramp change events in ticks
    block->accelerate_until = acceleration_ticks;
    block->decelerate_after = total_move_ticks - deceleration_ticks;

    // We now have everything we need for this block to call a Steppermotor->move method !!!!
    // Theorically, if accel is done per tick, the speed curve should be perfect.
    block->total_move_ticks = total_move_ticks;

    block->initial_rate = initial_rate;
    block->exit_speed = exitspeed;

    // prepare the block for stepticker
    prepare(block, acceleration_in_steps, deceleration_in_steps);

    block->locked= false;
}


// Called by Planner::recalculate() when scanning the plan from last to first entry.
float Planner::reverse_pass(Block *block, float exit_speed)
{
    // If entry speed is already at the maximum entry speed, no need to recheck. Block is cruising.
    // If not, block in state of acceleration or deceleration. Reset entry speed to maximum and
    // check for maximum allowable speed reductions to ensure maximum possible planned speed.
    if (block->entry_speed != block->max_entry_speed) {
        // If nominal length true, max junction speed is guaranteed to be reached. Only compute
        // for max allowable speed if block is decelerating and nominal length is false.
        if ((!block->nominal_length_flag) && (block->max_entry_speed > exit_speed)) {
            float max_entry_speed = max_allowable_speed(-block->acceleration, exit_speed, block->millimeters);

            block->entry_speed = std::min(max_entry_speed, block->max_entry_speed);

            return block->entry_speed;
        } else
            block->entry_speed = block->max_entry_speed;
    }

    return block->entry_speed;
}


// Called by Planner::recalculate() when scanning the plan from first to last entry.
// returns maximum exit speed of this block
float Planner::forward_pass(Block *block, float prev_max_exit_speed)
{
    // If the previous block is an acceleration block, but it is not long enough to complete the
    // full speed change within the block, we need to adjust the entry speed accordingly. Entry
    // speeds have already been reset, maximized, and reverse planned by reverse planner.
    // If nominal length is true, max junction speed is guaranteed to be reached. No need to recheck.

    // TODO: find out if both of these checks are necessary
    if (prev_max_exit_speed > block->nominal_speed)
        prev_max_exit_speed = block->nominal_speed;
    if (prev_max_exit_speed > block->max_entry_speed)
        prev_max_exit_speed = block->max_entry_speed;

    if (prev_max_exit_speed <= block->entry_speed) {
        // accel limited
        block->entry_speed = prev_max_exit_speed;
        // since we're now acceleration or cruise limited
        // we don't need to recalculate our entry speed anymore
        block->recalculate_flag = false;
    }
    // else
    // // decel limited, do nothing

    return max_exit_speed(block);
}

float Planner::max_exit_speed(Block *block)
{
    // if block is currently executing, return cached exit speed from calculate_trapezoid
    // this ensures that a block following a currently executing block will have correct entry speed
    if(block->is_ticking)
        return block->exit_speed;

    // if nominal_length_flag is asserted
    // we are guaranteed to reach nominal speed regardless of entry speed
    // thus, max exit will always be nominal
    if (block->nominal_length_flag)
        return block->nominal_speed;

    // otherwise, we have to work out max exit speed based on entry and acceleration
    float max = max_allowable_speed(-block->acceleration, block->entry_speed, block->millimeters);

    return std::min(max, block->nominal_speed);
}

// prepare block for the step ticker, called everytime the block changes
// this is done during planning so does not delay tick generation and step ticker can simply grab the next block during the interrupt
void Planner::prepare(Block *block, float acceleration_in_steps, float deceleration_in_steps)
{

    float inv = 1.0F / block->steps_event_count;

    // Now figure out the acceleration PER TICK, this should ideally be held as a double as it's very critical to the block timing
    // steps/tick^2
    // was....
    // float acceleration_per_tick = acceleration_in_steps / STEP_TICKER_FREQUENCY_2; // that is 100,000² too big for a float
    // float deceleration_per_tick = deceleration_in_steps / STEP_TICKER_FREQUENCY_2;
    double acceleration_per_tick = acceleration_in_steps * fp_scale; // this is now scaled to fit a 2.30 fixed point number
    double deceleration_per_tick = deceleration_in_steps * fp_scale;

    for (uint8_t m = 0; m < Block::n_actuators; m++) {
        uint32_t steps = block->steps[m];
        block->tick_info[m].steps_to_move = steps;
        if(steps == 0) continue;

        float aratio = inv * steps;

        block->tick_info[m].steps_per_tick = (int64_t)round((((double)block->initial_rate * aratio) / STEP_TICKER_FREQUENCY) * STEPTICKER_FPSCALE); // steps/sec / tick frequency to get steps per tick in 2.62 fixed point
        block->tick_info[m].counter = 0; // 2.62 fixed point
        block->tick_info[m].step_count = 0;
        block->tick_info[m].next_accel_event = block->total_move_ticks + 1;

        double acceleration_change = 0;
        if(block->accelerate_until != 0) { // If the next accel event is the end of accel
            block->tick_info[m].next_accel_event = block->accelerate_until;
            acceleration_change = acceleration_per_tick;

        } else if(block->decelerate_after == 0 /*&& block->accelerate_until == 0*/) {
            // we start off decelerating
            acceleration_change = -deceleration_per_tick;

        } else if(block->decelerate_after != block->total_move_ticks /*&& block->accelerate_until == 0*/) {
            // If the next event is the start of decel ( don't set this if the next accel event is accel end )
            block->tick_info[m].next_accel_event = block->decelerate_after;
        }

        // already converted to fixed point just needs scaling by ratio
        //#define STEPTICKER_TOFP(x) ((int64_t)round((double)(x)*STEPTICKER_FPSCALE))
        block->tick_info[m].acceleration_change= (int64_t)round(acceleration_change * aratio);
        block->tick_info[m].deceleration_change= -(int64_t)round(deceleration_per_tick * aratio);
        block->tick_info[m].plateau_rate= (int64_t)round(((block->maximum_rate * aratio) / STEP_TICKER_FREQUENCY) * STEPTICKER_FPSCALE);

        #if 0
        THEKERNEL->streams->printf("spt: %08lX %08lX, ac: %08lX %08lX, dc: %08lX %08lX, pr: %08lX %08lX\n",
            (uint32_t)(this->tick_info[m].steps_per_tick>>32), // 2.62 fixed point
            (uint32_t)(this->tick_info[m].steps_per_tick&0xFFFFFFFF), // 2.62 fixed point
            (uint32_t)(this->tick_info[m].acceleration_change>>32), // 2.62 fixed point signed
            (uint32_t)(this->tick_info[m].acceleration_change&0xFFFFFFFF), // 2.62 fixed point signed
            (uint32_t)(this->tick_info[m].deceleration_change>>32), // 2.62 fixed point
            (uint32_t)(this->tick_info[m].deceleration_change&0xFFFFFFFF), // 2.62 fixed point
            (uint32_t)(this->tick_info[m].plateau_rate>>32), // 2.62 fixed point
            (uint32_t)(this->tick_info[m].plateau_rate&0xFFFFFFFF) // 2.62 fixed point
        );
        #endif
    }
}
