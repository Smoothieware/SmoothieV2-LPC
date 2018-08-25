#pragma once

#include <stdint.h>
#include <cmath>
#include "ActuatorCoordinates.h"

class Block;
class PlannerQueue;
class ConfigReader;
class Conveyor;

#define N_PRIMARY_AXIS 3
class Robot;

class Planner
{
public:
    static Planner *getInstance() { return instance; }
    Planner();
    // delete copy and move constructors and assign operators
    Planner(Planner const&) = delete;             // Copy construct
    Planner(Planner&&) = delete;                  // Move construct
    Planner& operator=(Planner const&) = delete;  // Copy assign
    Planner& operator=(Planner &&) = delete;      // Move assign

    bool configure(ConfigReader& cr);
    bool initialize(uint8_t n);

private:
    static Planner *instance;
    float max_exit_speed(Block *);
    float max_allowable_speed( float acceleration, float target_velocity, float distance);

    void calculate_trapezoid(Block *, float entry_speed, float exit_speed );
    float reverse_pass(Block *, float exit_speed);
    float forward_pass(Block *, float next_entry_speed);
    void prepare(Block *, float acceleration_in_steps, float deceleration_in_steps);

    bool append_block(ActuatorCoordinates& target, uint8_t n_motors, float rate_mm_s, float distance, float unit_vec[], float accleration, float s_value, bool g123);
    void recalculate();

    double fp_scale; // optimize to store this as it does not change

    PlannerQueue *queue{nullptr};
    float previous_unit_vec[N_PRIMARY_AXIS];

    float xy_junction_deviation{0.05F};    // Setting
    float z_junction_deviation{NAN};  // Setting
    float minimum_planner_speed{0.0F}; // Setting
    int planner_queue_size{32}; // setting

    // FIXME should really just make getters and setters or handle the set/get gcode here
    friend Robot;
    friend Conveyor;
};
