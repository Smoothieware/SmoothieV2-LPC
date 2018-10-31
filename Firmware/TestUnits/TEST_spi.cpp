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
#if 0
REGISTER_TEST(SPITest, basic_lowlevel)
{
	/* SSP initialization */
	Board_SSP_Init(LPC_SSP);

	Chip_SSP_Init(LPC_SSP);
	Chip_SSP_SetBitRate(LPC_SSP, 100000);
	Chip_SCU_PinMuxSet(0x7, 0, (SCU_PINIO_FAST | SCU_MODE_FUNC0)); // GPIO 3.8
	Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 0x3, 8);	/* CE X driver */
	Chip_GPIO_SetPinState(LPC_GPIO_PORT, 0x3, 8, true);

	SSP_ConfigFormat ssp_format;
	Chip_SSP_DATA_SETUP_T xf_setup;
	uint8_t tx_buf[3];
	uint8_t rx_buf[3];

	uint32_t data= 0xE0000ul;
	tx_buf[0]= data>>16;
	tx_buf[1]= data>>8;
	tx_buf[2]= data&0xFF;
	xf_setup.length = 3;
	xf_setup.tx_data = tx_buf;
	xf_setup.rx_data = rx_buf;

	ssp_format.frameFormat = SSP_FRAMEFORMAT_SPI;
	ssp_format.bits = SSP_BITS_8;
	ssp_format.clockMode = SSP_CLOCK_MODE3;
    Chip_SSP_SetFormat(LPC_SSP, ssp_format.bits, ssp_format.frameFormat, ssp_format.clockMode);
	Chip_SSP_Enable(LPC_SSP);

	for (int i = 0; i < 2; ++i) {
		xf_setup.rx_cnt = xf_setup.tx_cnt = 0;
		// enable CS
		Chip_GPIO_SetPinState(LPC_GPIO_PORT, 0x3, 8, false);
		uint32_t n= Chip_SSP_RWFrames_Blocking(LPC_SSP, &xf_setup);
		// disable CS
		Chip_GPIO_SetPinState(LPC_GPIO_PORT, 0x3, 8, true);
		TEST_ASSERT_EQUAL_INT(3, n);
		TEST_ASSERT_EQUAL_INT(3, xf_setup.rx_cnt);
		TEST_ASSERT_EQUAL_INT(3, xf_setup.tx_cnt);
		uint32_t tdata= ((tx_buf[0] << 16) | (tx_buf[1] << 8) | (tx_buf[2])) >> 4;
		printf("sent: %02X %02X %02X (%08lX)\n", tx_buf[0], tx_buf[1], tx_buf[2], tdata);
		uint32_t rdata= ((rx_buf[0] << 16) | (rx_buf[1] << 8) | (rx_buf[2])) >> 4;
		printf("rcvd: %02X %02X %02X (%08lX)\n", rx_buf[0], rx_buf[1], rx_buf[2], rdata);
	}

	Chip_SSP_DeInit(LPC_SSP);
}
#endif

#include "Pin.h"
#include "Spi.h"
static int sendSPI(SPI *spi, Pin& cs, uint8_t *b, int cnt, uint8_t *r)
{
    cs.set(false);
    for (int i = 0; i < cnt; ++i) {
        r[i] = spi->write(b[i]);
    }
    cs.set(true);
    return cnt;
}

REGISTER_TEST(SPITest, Spi_class)
{
    SPI *spi = new SPI(0);
    spi->frequency(100000);
    spi->format(8, 3); // 8bit, mode3

    Pin cs[]= {
    	Pin("gpio3_8", Pin::AS_OUTPUT),
    	Pin("gpio7_12", Pin::AS_OUTPUT),
    	Pin("gpio7_7", Pin::AS_OUTPUT),
    	Pin("gpio2_8", Pin::AS_OUTPUT)
    };

    for(auto& p : cs) {
    	printf("checking cs pin: %s\n", p.to_string().c_str());
    	TEST_ASSERT_TRUE(p.connected());
    	p.set(true);
    }


    for(auto& p : cs) {
		uint32_t data= 0xE0000ul;
		uint8_t tx_buf[3]{(uint8_t)(data>>16), (uint8_t)(data>>8), (uint8_t)(data&0xFF)};
		uint8_t rx_buf[3]{0};

    	printf("testing channel with cs pin: %s\n", p.to_string().c_str());
    	int n= sendSPI(spi, p, tx_buf, 3, rx_buf);
    	TEST_ASSERT_EQUAL_INT(3, n);
    	uint32_t tdata= ((tx_buf[0] << 16) | (tx_buf[1] << 8) | (tx_buf[2]));
		printf("  sent: %02X %02X %02X (%08lX)\n", tx_buf[0], tx_buf[1], tx_buf[2], tdata);
		uint32_t rdata= ((rx_buf[0] << 16) | (rx_buf[1] << 8) | (rx_buf[2])) >> 4;
		printf("  rcvd: %02X %02X %02X (%08lX)\n", rx_buf[0], rx_buf[1], rx_buf[2], rdata);
	}
	delete spi;
}
#endif
