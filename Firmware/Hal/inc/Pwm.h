#pragma once

#include <stdint.h>

class Pwm
{
public:
	Pwm();
	~Pwm(){};
	Pwm(const char* pin);
	bool from_string(const char *pin);
	bool is_valid() const { return valid; }
	// set duty cycle 0-1
	void set(float v);
	float get() const { return value; }

	static bool setup(float freq);

private:
	bool lookup_pin(uint8_t port, uint8_t pin, uint8_t& ctout, uint8_t& func);
	int map_pin_to_pwm(const char *name);

	static int pwm_index;
	float value;
	bool valid{false};
	uint8_t index;

};
