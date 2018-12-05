#include "i2c.h"

#include <stdlib.h>
#include <string.h>
#include "board.h"

static I2C_ID_T i2cdev[2]= {I2C0, I2C1};

I2C::I2C(int ch)
{
	channel= ch;
}

I2C::~I2C()
{
	NVIC_DisableIRQ(channel == 0 ? I2C0_IRQn : I2C1_IRQn);
	Chip_I2C_DeInit(i2cdev[channel]);
}

/* State machine handler for I2C0 and I2C1 */
static void i2c_state_handling(I2C_ID_T id)
{
	if (Chip_I2C_IsMasterActive(id)) {
		Chip_I2C_MasterStateHandler(id);
	} else {
		Chip_I2C_SlaveStateHandler(id);
	}
}

extern "C"
void I2C0_IRQHandler(void)
{
	i2c_state_handling(I2C0);
}

extern "C"
void I2C1_IRQHandler(void)
{
	i2c_state_handling(I2C1);
}

bool I2C::init()
{
	/* Initialize I2C */
	Board_I2C_Init(i2cdev[channel]);
	Chip_I2C_Init(i2cdev[channel]);
	Chip_I2C_SetClockRate(i2cdev[channel], frequency);

	/* Set mode to interrupt */
	Chip_I2C_SetMasterEventHandler(i2cdev[channel], Chip_I2C_EventHandler);
	NVIC_EnableIRQ(channel == 0 ? I2C0_IRQn : I2C1_IRQn);
	// set priority lower than stepper but higher than most other ones
	NVIC_SetPriority(channel == 0 ? I2C0_IRQn : I2C1_IRQn, 1); // cannot call any RTOS stuff from this IRQ
	return true;
}

bool I2C::set_address(uint8_t addr)
{
	slave_addr = addr;
	return true;
}

bool I2C::read(uint8_t *buf, int len)
{
	// Read data
	int n= Chip_I2C_MasterRead(i2cdev[channel], slave_addr, buf, len);
	//printf("read %d bytes\n", n);
	return n == len;
}

bool I2C::write(uint8_t *buf, int len)
{
	// Send data
	int n= Chip_I2C_MasterSend(i2cdev[channel], slave_addr, buf, len);
	//printf("sent %d bytes\n", n);
	return n == len;
}
