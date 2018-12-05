#pragma once

#include <cstdint>

class I2C
{
public:
	I2C(int channel);
	virtual ~I2C();

	bool init();
	bool set_address(uint8_t addr);
	bool read(uint8_t *buf, int len);
	bool write(uint8_t *buf, int len);
	void set_frequency(int f) { frequency= f; }

private:
	int frequency{100000};
	int channel;
	uint8_t slave_addr;
};
