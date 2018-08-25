#pragma once

#include "Pin.h"

class StepperMotor
{
    public:
        StepperMotor(Pin& step, Pin& dir, Pin& en);
        ~StepperMotor();

        void set_motor_id(uint8_t id) { motor_id= id; }
        uint8_t get_motor_id() const { return motor_id; }

        // called from step ticker ISR
        inline bool step() { step_pin.set(1); current_position_steps += (direction?-1:1); return moving; }
        // called from unstep ISR
        inline void unstep() { step_pin.set(0); }
        // called from step ticker ISR
        inline void set_direction(bool f) { dir_pin.set(f); direction= f; }

        // TODO for SPI motors, we could check the en_pin and if nc then we call the linked advanced motor driver and tell it to enable
        // the driver can set a function pointer to the enable call it needs and set it in the appropriate actuator.
        void enable(bool state) { en_pin.set(!state); };
        bool is_enabled() const { return !en_pin.get(); };
        bool is_moving() const { return moving; };
        void start_moving() { moving= true; }
        void stop_moving() { moving= false; }

        void manual_step(bool dir);

        bool which_direction() const { return direction; }

        float get_steps_per_second()  const { return steps_per_second; }
        float get_steps_per_mm()  const { return steps_per_mm; }
        void change_steps_per_mm(float);
        void change_last_milestone(float);
        void set_last_milestones(float, int32_t);
        void update_last_milestones(float mm, int32_t steps);
        float get_last_milestone(void) const { return last_milestone_mm; }
        int32_t get_last_milestone_steps(void) const { return last_milestone_steps; }
        float get_current_position(void) const { return (float)current_position_steps/steps_per_mm; }
        uint32_t get_current_step(void) const { return current_position_steps; }
        float get_max_rate(void) const { return max_rate; }
        void set_max_rate(float mr) { max_rate= mr; }
        void set_acceleration(float a) { acceleration= a; }
        float get_acceleration() const { return acceleration; }
        bool is_selected() const { return selected; }
        void set_selected(bool b) { selected= b; }
        bool is_extruder() const { return extruder; }
        void set_extruder(bool b) { extruder= b; }

        int32_t steps_to_target(float);


    private:

        Pin step_pin;
        Pin dir_pin;
        Pin en_pin;

        float steps_per_second;
        float steps_per_mm;
        float max_rate; // this is not really rate it is in mm/sec, misnamed used in Robot and Extruder
        float acceleration;

        volatile int32_t current_position_steps;
        int32_t last_milestone_steps;
        float   last_milestone_mm;

        volatile struct {
            uint8_t motor_id:8;
            volatile bool direction:1;
            volatile bool moving:1;
            bool selected:1;
            bool extruder:1;
        };
};

