#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstring>

#include "TestRegistry.h"

#include "board.h"

#include "FreeRTOS.h"
#include "task.h"

#define LPC_SSP LPC_SSP0

#ifdef BOARD_PRIMEALPHA
REGISTER_TEST(SPITest, polling)
{
	/* SSP initialization */
	Board_SSP_Init(LPC_SSP);

	Chip_SSP_Init(LPC_SSP);
	Chip_SSP_SetBitRate(LPC_SSP, 100000);
	Chip_SCU_PinMuxSet(0x7, 0, (SCU_PINIO_FAST | SCU_MODE_FUNC0)); // GPIO 3.8
	Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 0x3, 8);	/* CE X driver */
	Chip_GPIO_SetPinState(LPC_GPIO_PORT, 0x3, 8, true);

	static SSP_ConfigFormat ssp_format;
	static Chip_SSP_DATA_SETUP_T xf_setup;
	static uint8_t tx_buf[3];
	static uint8_t rx_buf[3];

	memset(tx_buf, 0, 3);
	xf_setup.length = 3;
	xf_setup.tx_data = tx_buf;
	xf_setup.rx_data = rx_buf;

	ssp_format.frameFormat = SSP_FRAMEFORMAT_SPI;
	ssp_format.bits = SSP_BITS_8;
	ssp_format.clockMode = SSP_CLOCK_MODE3;
    Chip_SSP_SetFormat(LPC_SSP, ssp_format.bits, ssp_format.frameFormat, ssp_format.clockMode);
	Chip_SSP_Enable(LPC_SSP);

	xf_setup.rx_cnt = xf_setup.tx_cnt = 0;
	// enable CS
	Chip_GPIO_SetPinState(LPC_GPIO_PORT, 0x3, 8, false);
	uint32_t n= Chip_SSP_RWFrames_Blocking(LPC_SSP, &xf_setup);
	// disable CS
	Chip_GPIO_SetPinState(LPC_GPIO_PORT, 0x3, 8, true);
	TEST_ASSERT_EQUAL_INT(3, n);
	TEST_ASSERT_EQUAL_INT(3, xf_setup.rx_cnt);
	printf("%02X %02X %02X\n", rx_buf[0], rx_buf[1], rx_buf[2]);

	Chip_SSP_DeInit(LPC_SSP);
}
#endif
