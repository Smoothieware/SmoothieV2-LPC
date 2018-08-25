#include "Pwm.h"

#include <string>
#include <cstring>
#include <cctype>
#include <tuple>
#include <vector>
#include <cmath>

#include "lpc_types.h"
#include "chip-defs.h"
#include "sct_18xx_43xx.h"
#include "sct_pwm_18xx_43xx.h"
#include "scu_18xx_43xx.h"



/* 43xx Pinmap for PWM to CTOUT and function
Pin  a, b, COUT#, Function
*/
static const std::vector<std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>> lut {
    {0x01, 1,  7,  1},
    {0x01, 2,  6,  1},
    {0x01, 3,  8,  1},
    {0x01, 4,  9,  1},
    {0x01, 5,  10, 1},
    {0x01, 7,  13, 2},
    {0x01, 8,  12, 2},
    {0x01, 9,  11, 2},
    {0x01, 10, 14, 2},
    {0x01, 11, 15, 2},
    {0x02, 7,  1,  1},
    {0x02, 8,  0,  1},
    {0x02, 9,  3,  1},
    {0x02, 10, 2,  1},
    {0x02, 11, 5,  1},
    {0x02, 12, 4,  1},
    {0x04, 1,  1,  1},
    {0x04, 2,  0,  1},
    {0x04, 3,  3,  1},
    {0x04, 4,  2,  1},
    {0x04, 5,  5,  1},
    {0x04, 6,  4,  1},
    {0x06, 5,  6,  1},
    {0x06, 12, 7,  1},
    {0x07, 0,  14, 1},
    {0x07, 1,  15, 1},
    {0x07, 4,  13, 1},
    {0x07, 5,  12, 1},
    {0x07, 6,  11, 1},
    {0x07, 7,  8,  1},
    {0x0A, 4,  9,  1},
    {0x0B, 0,  10, 1},
    {0x0B, 1,  6,  5},
    {0x0B, 2,  7,  5},
    {0x0B, 3,  8,  5},
    {0x0D, 0,  15, 1},
    {0x0D, 2,  7,  1},
    {0x0D, 3,  6,  1},
    {0x0D, 4,  8,  1},
    {0x0D, 5,  9,  1},
    {0x0D, 6,  10, 1},
    {0x0D, 9,  13, 1},
    {0x0D, 11, 14, 6},
    {0x0D, 12, 10, 6},
    {0x0D, 13, 13, 6},
    {0x0D, 14, 11, 6},
    {0x0D, 15, 8,  6},
    {0x0D, 16, 12, 6},
    {0x0E, 5,  3,  1},
    {0x0E, 6,  2,  1},
    {0x0E, 7,  5,  1},
    {0x0E, 8,  4,  1},
    {0x0E, 11, 12, 1},
    {0x0E, 12, 11, 1},
    {0x0E, 13, 14, 1},
    {0x0E, 15, 0,  1},
    {0x0F, 9,  1,  2}
};

bool Pwm::lookup_pin(uint8_t port, uint8_t pin, uint8_t& ctout, uint8_t& func)
{
    for(auto &p : lut) {
        if(port == std::get<0>(p) && pin == std::get<1>(p)) {
            ctout= std::get<2>(p);
            func= std::get<3>(p);
            return true;
        }
    }

    return false;
}

// static
int Pwm::pwm_index= 1;
int Pwm::map_pin_to_pwm(const char *name)
{
    // specify pin name P1.6 and check it is mappable to a PWM channel
    if(tolower(name[0]) == 'p') {
        // pin specification
        std::string str(name);
        uint16_t port = strtol(str.substr(1).c_str(), nullptr, 16);
        size_t pos = str.find_first_of("._", 1);
        if(pos == std::string::npos) return 0;
        uint16_t pin = strtol(str.substr(pos + 1).c_str(), nullptr, 10);

        // now map to a PWM output
        uint8_t ctout, func;
        if(!lookup_pin(port, pin, ctout, func)) {
            return 0;
        }

        // check if ctoun is already in use
        // TODO

        // setup pin for the PWM function
        Chip_SCU_PinMuxSet(port, pin, func);

        // index is incremented for each pin
        Chip_SCTPWM_SetOutPin(LPC_SCT, pwm_index, ctout);

        return pwm_index++;
    }

    return 0;
}

Pwm::Pwm()
{
	valid= false;
	index= 0;
}

Pwm::Pwm(const char *pin)
{
	from_string(pin);
}

// static
bool Pwm::setup(float freq)
{
    /* Initialize the SCT as PWM and set frequency */
    Chip_SCTPWM_Init(LPC_SCT);
    Chip_SCTPWM_SetRate(LPC_SCT, freq); // 10KHz
	Chip_SCTPWM_Start(LPC_SCT);
	return true;
}

bool Pwm::from_string(const char *pin)
{
	int xind= map_pin_to_pwm(pin);
    if(xind > 0){
    	valid= true;
    	index= xind;
    	return true;
    }

 	valid= false;
 	index= 0;
 	return false;
}

void Pwm::set(float v)
{
	if(!valid) return;

	if(v < 0) v= 0;
	else if(v > 1) v= 1;

	uint32_t ticks= floorf(Chip_SCTPWM_GetTicksPerCycle(LPC_SCT) * v);
	Chip_SCTPWM_SetDutyCycle(LPC_SCT, index, ticks);

	value= v;
}
