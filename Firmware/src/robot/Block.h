#pragma once

#include "ActuatorCoordinates.h"

#include <array>
#include <bitset>

class Block {
    public:
        Block();

        static void init(uint8_t);

        float get_trapezoid_rate(int i) const;
        void debug() const;
        void ready() { is_ready= true; }
        void clear();
    public:
        std::array<uint32_t, k_max_actuators> steps; // Number of steps for each axis for this block
        uint32_t steps_event_count;  // Steps for the longest axis
        float nominal_rate;       // Nominal rate in steps per second
        float nominal_speed;      // Nominal speed in mm per second
        float millimeters;        // Distance for this move
        float entry_speed;
        float exit_speed;
        float acceleration;       // the acceleration for this block
        float initial_rate;       // Initial rate in steps per second
        float maximum_rate;

        float max_entry_speed;

        // this is tick info needed for this block. applies to all motors
        uint32_t accelerate_until;
        uint32_t decelerate_after;
        uint32_t total_move_ticks;
        std::bitset<k_max_actuators> direction_bits;     // Direction for each axis in bit form, relative to the direction port's mask

        // this is the data needed to determine when each motor needs to be issued a step
        using tickinfo_t= struct {
            int64_t steps_per_tick; // 2.62 fixed point
            int64_t counter; // 2.62 fixed point
            int64_t acceleration_change; // 2.62 fixed point signed
            int64_t deceleration_change; // 2.62 fixed point
            int64_t plateau_rate; // 2.62 fixed point
            uint32_t steps_to_move;
            uint32_t step_count;
            uint32_t next_accel_event;
        };

        // need info for each active motor
        tickinfo_t *tick_info;

        static uint8_t n_actuators;

        struct {
            bool recalculate_flag:1;             // Planner flag to recalculate trapezoids on entry junction
            bool nominal_length_flag:1;          // Planner flag for nominal speed always reached
            bool is_ready:1;
            bool primary_axis:1;                 // set if this move is a primary axis
            bool is_g123:1;                      // set if this is a G1, G2 or G3
            volatile bool is_ticking:1;          // set when this block is being actively ticked by the stepticker
            volatile bool locked:1;              // set to true when the critical data is being updated, stepticker will have to skip if this is set
            uint16_t s_value:12;                 // for laser 1.11 Fixed point
        };
};
