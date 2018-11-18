#pragma once

#include "Module.h"

#include <stdint.h>
#include <string>

class Pin;
class Pwm;
class Block;
class ConfigReader;
class GCode;
class OutputStream;

class Laser : public Module
{
    public:
        Laser();
        virtual ~Laser() {};
        static bool create(ConfigReader& cr);

        bool configure(ConfigReader&);
        bool request(const char *key, void *value);

        void set_scale(float s) { scale= s/100; }
        float get_scale() const { return scale*100; }
        bool set_laser_power(float p);
        float get_current_power() const;

    private:
        void on_halt(bool flg);
        bool handle_M221(GCode& gcode, OutputStream& os);
        bool handle_fire_cmd( std::string& params, OutputStream& os );

        void set_proportional_power(void);
        bool get_laser_power(float& power) const;
        float current_speed_ratio(const Block *block) const;

        Pwm *pwm_pin;    // PWM output to regulate the laser power
        Pin *ttl_pin;				// TTL output to fire laser
        float laser_maximum_power; // maximum allowed laser power to be output on the pwm pin
        float laser_minimum_power; // value used to tickle the laser on moves.  Also minimum value for auto-scaling
        float laser_maximum_s_value; // Value of S code that will represent max power
        float scale;
        struct {
            bool laser_on:1;      // set if the laser is on
            bool pwm_inverting:1; // stores whether the PWM period should be inverted
            bool ttl_used:1;		// stores whether we have a TTL output
            bool ttl_inverting:1;   // stores whether the TTL output should be inverted
            bool manual_fire:1;     // set when manually firing
        };
};
