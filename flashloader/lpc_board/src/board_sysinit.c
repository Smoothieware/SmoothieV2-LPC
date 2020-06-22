/*
 * Copyright(C) NXP Semiconductors, 2012
 * All rights reserved.
 *
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "board.h"

/* The System initialization code is called prior to the application and
   initializes the board for run-time operation. */

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/* Structure for initial base clock states */
struct CLK_BASE_STATES {
	CHIP_CGU_BASE_CLK_T clk;	/* Base clock */
	CHIP_CGU_CLKIN_T clkin;	/* Base clock source, see UM for allowable souorces per base clock */
	bool autoblock_enab;/* Set to true to enable autoblocking on frequency change */
	bool powerdn;		/* Set to true if the base clock is initially powered down */
};

/* Initial base clock states are mostly on */
STATIC const struct CLK_BASE_STATES InitClkStates[] = {
	{CLK_BASE_PHY_TX, CLKIN_ENET_TX, true, false},
#if defined(USE_RMII)
	{CLK_BASE_PHY_RX, CLKIN_ENET_TX, true, false},
#else
	{CLK_BASE_PHY_RX, CLKIN_ENET_RX, true, false},
#endif

	/* Clocks derived from dividers */
	{CLK_BASE_LCD, CLKIN_IDIVC, true, false},
	{CLK_BASE_USB1, CLKIN_IDIVD, true, true}
};

#if 0
/* SPIFI high speed pin mode setup */
STATIC const PINMUX_GRP_T spifipinmuxing[] = {
	{0x3, 3,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	/* SPIFI CLK */
	{0x3, 4,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	/* SPIFI D3 */
	{0x3, 5,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	/* SPIFI D2 */
	{0x3, 6,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	/* SPIFI D1 */
	{0x3, 7,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	/* SPIFI D0 */
	{0x3, 8,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)}	/* SPIFI CS/SSEL */
};
#endif

#ifdef BOARD_PRIMEALPHA
/* EMC 16bit flash high speed pin mode setup */
STATIC const PINMUX_GRP_T emc16_pinmuxing[] = {
	{0x1,  3,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_OE
	{0x1,  5,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_CS0
	{0x1,  6,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_WE
	{0x1,  7,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_D0
	{0x1,  8,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_D1
	{0x1,  9,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_D2
	{0x1, 10,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_D3
	{0x1, 11,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_D4
	{0x1, 12,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_D5
	{0x1, 13,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_D6
	{0x1, 14,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_D7
	{0x5,  4,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_D8
	{0x5,  5,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_D9
	{0x5,  6,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_D10
	{0x5,  7,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_D11
	{0x5,  0,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_D12
	{0x5,  1,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_D13
	{0x5,  2,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_D14
	{0x5,  3,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_D15
	{0x2, 10,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_A1
	{0x2, 11,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_A2
	{0x2, 12,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_A3
	{0x2, 13,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_A4
	{0x1,  0,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_A5
	{0x1,  1,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_A6
	{0x1,  2,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_A7
	{0x2,  8,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_A8
	{0x2,  7,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_A9
	{0x2,  6,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_A10
	{0x2,  2,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_A11
	{0x2,  1,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_A12
	{0x2,  0,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_A13
	{0x6,  8,  (SCU_PINIO_FAST | SCU_MODE_FUNC1)},	// EMC_A14
	{0x6,  7,  (SCU_PINIO_FAST | SCU_MODE_FUNC1)},	// EMC_A15
	{0xd, 16,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_A16
	{0xd, 15,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},	// EMC_A17
	{0xe,  0,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_A18
	{0xe,  1,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_A19
	{0xe,  2,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_A20
	{0xe,  3,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_A21
	{0xe,  4,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_A22
	//{0xa,  4,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},	// EMC_A23 not used

	{0xa,  4,  (SCU_PINIO_FAST | SCU_MODE_FUNC4)},	// EMC_A23 GPIO5_19 not used
	{0xc,  1,  (SCU_PINIO_FAST | SCU_MODE_FUNC4)},	// EMC_A24 GPIO6_0 not used
	{0xc,  2,  (SCU_PINIO_FAST | SCU_MODE_FUNC4)},	// EMC_A25 GPIO6_1 not used
	{0xc,  3,  (SCU_PINIO_FAST | SCU_MODE_FUNC4)},	// EMC_A26 GPIO6_2 not used
};
#endif

STATIC const PINMUX_GRP_T pinmuxing[] = {
	#ifndef NOETHERNET
	/* RMII pin group */
	{0x1, 15, (SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC3)},
	{0x0,  0, (SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC2)},
	{0x1, 16, (SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC7)},
	{0x0,  1, (SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_INACT | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC6)},
	{0x1, 19, (SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC0)},
	{0x1, 18, (SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_INACT | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC3)},
	{0x1, 20, (SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_INACT | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC3)},
	{0x1, 17, (SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC3)},

	#if !(defined(UART0_PINSET) && UART0_PINSET == 2)
	#ifdef BOARD_BAMBINO
	{0x2, 0,  (SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_INACT | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC7)}, // ETH MDC
	#else
	{0x7, 7,  (SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_INACT | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC6)}, // ETH MDC
	#endif
	#else
	#warning can not have both RMII and UART0_PINSET 2 as P2.0 is in conflict
	#endif
	#endif // NOETHERNET

	/*  I2S  */
	#if 0
	{0x3, 0,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},
	{0x6, 0,  (SCU_PINIO_FAST | SCU_MODE_FUNC4)},
	{0x7, 2,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},
	{0x6, 2,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},
	{0x7, 1,  (SCU_PINIO_FAST | SCU_MODE_FUNC2)},
	{0x6, 1,  (SCU_PINIO_FAST | SCU_MODE_FUNC3)},
	#endif
};

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Sets up system pin muxing */
void Board_SetupMuxing(void)
{
	/* Setup system level pin muxing */
	Chip_SCU_SetPinMuxing(pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));

	/* SPIFI pin setup is done prior to setting up system clocking */
	//Chip_SCU_SetPinMuxing(spifipinmuxing, sizeof(spifipinmuxing) / sizeof(PINMUX_GRP_T));

#ifdef BOARD_PRIMEALPHA
	// EMC pins setup
	Chip_SCU_SetPinMuxing(emc16_pinmuxing, sizeof(emc16_pinmuxing) / sizeof(PINMUX_GRP_T));
	// set the unused addresses A23-A26 pins to low GPIO5_19, GPIO6_0 - 2
	// NOTE Not actually needed but may as well
    typedef struct pin_tuple { uint8_t port; uint8_t pin; } PIN_TUPLE;
	PIN_TUPLE pins[4] = {{5, 19}, {6, 0}, {6, 1}, {6, 2}};
	for (int i = 0; i < 4 ; ++i) {
		Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, pins[i].port, pins[i].pin);
    	Chip_GPIO_SetPinState(LPC_GPIO_PORT, pins[i].port, pins[i].pin, (bool) false);
    }
    // TODO initialize EMC here
#endif
}

/* Set up and initialize clocking prior to call to main */
void Board_SetupClocking(void)
{
	int i;

	// Chip_Clock_SetBaseClock(CLK_BASE_SPIFI, CLKIN_IRC, true, false);	// change SPIFI to IRC during clock programming
	// LPC_SPIFI->CTRL |= SPIFI_CTRL_FBCLK(1);								// and set FBCLK in SPIFI controller

	Chip_SetupCoreClock(CLKIN_CRYSTAL, MAX_CLOCK_FREQ, true);

	/* Reset and enable 32Khz oscillator */
	LPC_CREG->CREG0 &= ~((1 << 3) | (1 << 2));
	LPC_CREG->CREG0 |= (1 << 1) | (1 << 0);

	/* Setup a divider E for main PLL clock switch SPIFI clock to that divider.
	   Divide rate is based on CPU speed and speed of SPI FLASH part. */
#if (MAX_CLOCK_FREQ > 180000000)
	Chip_Clock_SetDivider(CLK_IDIV_B, CLKIN_MAINPLL, 5);
#else
	Chip_Clock_SetDivider(CLK_IDIV_E, CLKIN_MAINPLL, 4);
#endif
	Chip_Clock_SetBaseClock(CLK_BASE_SPIFI, CLKIN_IDIVB, true, false);

	/* Setup system base clocks and initial states. This won't enable and
	   disable individual clocks, but sets up the base clock sources for
	   each individual peripheral clock. */
	for (i = 0; i < (sizeof(InitClkStates) / sizeof(InitClkStates[0])); i++) {
		Chip_Clock_SetBaseClock(InitClkStates[i].clk, InitClkStates[i].clkin,
								InitClkStates[i].autoblock_enab, InitClkStates[i].powerdn);
	}
}

/* Set up and initialize hardware prior to call to main */
void Board_SystemInit(void)
{
	/* Setup system clocking and memory. This is done early to allow the
	   application and tools to clear memory and use scatter loading to
	   external memory. */
	Board_SetupMuxing();
	Board_SetupClocking();
}






