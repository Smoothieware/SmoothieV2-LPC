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
#include "string.h"

/** @ingroup BOARD_NGX_XPLORER_18304330
 * @{
 */

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/* System configuration variables used by chip driver */
const uint32_t ExtRateIn = 0;
const uint32_t OscRateIn = 12000000;

void Board_UART_Init(LPC_USART_T *pUART)
{
	if (pUART == LPC_USART0) {
#if defined(UART0_PINSET) && UART0_PINSET == 2
		Chip_SCU_PinMuxSet(0x2, 0, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC1));					/* P2.0 : UART0_TXD */
		Chip_SCU_PinMuxSet(0x2, 1, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC1));/* P2.1 : UART0_RXD */
#elif defined(UART0_PINSET) && UART0_PINSET == 6
		Chip_SCU_PinMuxSet(0x6, 4, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC2));					/* P6.5 : UART0_TXD */
		Chip_SCU_PinMuxSet(0x6, 5, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC2));/* P6.4 : UART0_RXD */
#elif defined(UART0_PINSET) && UART0_PINSET == 15
		Chip_SCU_PinMuxSet(0xF, 10, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC1));					/* PF.10 : UART0_TXD */
		Chip_SCU_PinMuxSet(0xF, 11, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC1));/* PF.11 : UART0_RXD */
#elif defined(USE_UART0)
#error UART0 needs to have the pinset defined [2|6|15]
#else
		printf("FATAL: no valid UART defined\n");
		__asm("bkpt #0");
#endif

	} else if (pUART == LPC_UART1) {
#if defined(UART1_PINSET) && UART1_PINSET == 1
		Chip_SCU_PinMuxSet(0x1, 13, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC1));				/* P1.13 : UART1_TXD */
		Chip_SCU_PinMuxSet(0x1, 14, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC1));	/* P1.14 : UART1_RX */
#elif defined(UART1_PINSET) && UART1_PINSET == 12
		Chip_SCU_PinMuxSet(0xC, 13, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC2));				/* PC.13 : UART1_TXD */
		Chip_SCU_PinMuxSet(0xC, 14, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC2));	/* PC.14 : UART1_RX */
#elif defined(USE_UART1)
#error UART1 needs to have the pinset defined [1|12]
#else
		printf("FATAL: no valid UART defined\n");
		__asm("bkpt #0");
#endif

	} else if (pUART == LPC_USART2) {
		Chip_SCU_PinMuxSet(0xa, 1, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC3));				/* PA.1 : UART2_TXD */
		Chip_SCU_PinMuxSet(0xa, 2, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC3));	/* PA.2 : UART2_RX */

	} else if (pUART == LPC_USART3) {
		Chip_SCU_PinMuxSet(0x2, 3, (SCU_MODE_PULLDOWN | SCU_MODE_FUNC2)); /* P2.3 : UART3_TXD */
		Chip_SCU_PinMuxSet(0x2, 4, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC2)); /* P2.4 : UART3_RX */
	}
}

#if 0
/* Initialize debug output via UART for board */
void Board_Debug_Init(void)
{
#if defined(DEBUG_UART)
	Board_UART_Init(DEBUG_UART);

	Chip_UART_Init(DEBUG_UART);
	Chip_UART_SetBaud(DEBUG_UART, 115200);
	Chip_UART_ConfigData(DEBUG_UART, UART_LCR_WLEN8 | UART_LCR_SBS_1BIT | UART_LCR_PARITY_DIS);

	/* Enable UART Transmit */
	Chip_UART_TXEnable(DEBUG_UART);
#endif
}

/* Sends a character on the UART */
void Board_UARTPutChar(char ch)
{
#if defined(DEBUG_UART)
	/* Wait for space in FIFO */
	while ((Chip_UART_ReadLineStatus(DEBUG_UART) & UART_LSR_THRE) == 0) {}
	Chip_UART_SendByte(DEBUG_UART, (uint8_t) ch);
#endif
}

/* Gets a character from the UART, returns EOF if no character is ready */
int Board_UARTGetChar(void)
{
#if defined(DEBUG_UART)
	if (Chip_UART_ReadLineStatus(DEBUG_UART) & UART_LSR_RDR) {
		return (int) Chip_UART_ReadByte(DEBUG_UART);
	}
#endif
	return EOF;
}

/* Outputs a string on the debug UART */
void Board_UARTPutSTR(const char *str)
{
#if defined(DEBUG_UART)
	while (*str != '\0') {
		Board_UARTPutChar(*str++);
	}
#endif
}

void Board_Buttons_Init(void)	// FIXME not functional ATM
{
	Chip_SCU_PinMuxSet(0x2, 7, (SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC0));		// P2_7 as GPIO0[7]
	Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, BUTTONS_BUTTON1_GPIO_PORT_NUM, BUTTONS_BUTTON1_GPIO_BIT_NUM);	// input
}

uint32_t Buttons_GetStatus(void)
{
	uint8_t ret = NO_BUTTON_PRESSED;
	if (Chip_GPIO_GetPinState(LPC_GPIO_PORT, BUTTONS_BUTTON1_GPIO_PORT_NUM, BUTTONS_BUTTON1_GPIO_BIT_NUM) == 0) {
		ret |= BUTTONS_BUTTON1;
	}
	return ret;
}

void Board_Joystick_Init(void)
{}

uint8_t Joystick_GetStatus(void)
{
	return NO_BUTTON_PRESSED;
}
#endif

#ifdef BOARD_PRIMEALPHA
static void Board_LED_Init()
{
    const PINMUX_GRP_T ledpinmuxing[] = {
		/* Board LEDs */
		{0x7, 4, (SCU_MODE_INBUFF_EN | SCU_MODE_PULLDOWN | SCU_MODE_FUNC0)}, // GPIO3_12
		{0x7, 5, (SCU_MODE_INBUFF_EN | SCU_MODE_PULLDOWN | SCU_MODE_FUNC0)}, // GPIO3_13
		{0x7, 6, (SCU_MODE_INBUFF_EN | SCU_MODE_PULLDOWN | SCU_MODE_FUNC0)}, // GPIO3_14
		{0xB, 6, (SCU_MODE_INBUFF_EN | SCU_MODE_PULLDOWN | SCU_MODE_FUNC4)}  // GPIO5_26
	};
	Chip_SCU_SetPinMuxing(ledpinmuxing, sizeof(ledpinmuxing) / sizeof(PINMUX_GRP_T));

	// setup the 4 system leds
	Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 3, 12);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 3, 13);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 3, 14);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 5, 26);

	/* Set initial states to off */
	Chip_GPIO_SetPinState(LPC_GPIO_PORT, 3, 12, (bool) false);
	Chip_GPIO_SetPinState(LPC_GPIO_PORT, 3, 13, (bool) false);
	Chip_GPIO_SetPinState(LPC_GPIO_PORT, 3, 14, (bool) false);
	Chip_GPIO_SetPinState(LPC_GPIO_PORT, 5, 26, (bool) false);
}

void Board_LED_Set(uint8_t LEDNumber, bool On)
{
	switch(LEDNumber) {
		case 0: Chip_GPIO_SetPinState(LPC_GPIO_PORT, 3, 12, (bool)On); break;
		case 1: Chip_GPIO_SetPinState(LPC_GPIO_PORT, 3, 13, (bool)On); break;
		case 2: Chip_GPIO_SetPinState(LPC_GPIO_PORT, 3, 14, (bool)On); break;
		case 3: Chip_GPIO_SetPinState(LPC_GPIO_PORT, 5, 26, (bool)On); break;
	}
}

bool Board_LED_Test(uint8_t LEDNumber)
{
	switch(LEDNumber) {
		case 0: return (bool)Chip_GPIO_GetPinState(LPC_GPIO_PORT, 3, 12);
		case 1: return (bool)Chip_GPIO_GetPinState(LPC_GPIO_PORT, 3, 13);
		case 2: return (bool)Chip_GPIO_GetPinState(LPC_GPIO_PORT, 3, 14);
		case 3: return (bool)Chip_GPIO_GetPinState(LPC_GPIO_PORT, 5, 26);
	}

	return false;
}

void Board_LED_Toggle(uint8_t LEDNumber)
{
	Board_LED_Set(LEDNumber, !Board_LED_Test(LEDNumber));
}
#else
static void Board_LED_Init(){}
void Board_LED_Set(uint8_t LEDNumber, bool On){}
bool Board_LED_Test(uint8_t LEDNumber){return false;}
void Board_LED_Toggle(uint8_t LEDNumber){}
#endif

/* Returns the MAC address assigned to this board */
void Board_ENET_GetMacADDR(uint8_t *mcaddr)
{
	uint8_t boardmac[] = {0x00, 0x60, 0x37, 0x12, 0x34, 0x56};

	memcpy(mcaddr, boardmac, 6);
}

/* Set up and initialize all required blocks and functions related to the
   board hardware */
void Board_Init(void)
{
	/* Sets up DEBUG UART */
	DEBUGINIT();

	/* Initializes GPIO */
	Chip_GPIO_Init(LPC_GPIO_PORT);

	/* Initialize LEDs */
	Board_LED_Init();

#if defined(USE_RMII)
	Chip_ENET_RMIIEnable(LPC_ETHERNET);
#else
	Chip_ENET_MIIEnable(LPC_ETHERNET);
#endif
}

void Board_I2C_Init(I2C_ID_T id)
{
	if (id == I2C1) {
		/* Configure pin function for I2C1*/
		Chip_SCU_PinMuxSet(0x2, 3, (SCU_MODE_ZIF_DIS | SCU_MODE_INBUFF_EN | SCU_MODE_FUNC1));		/* P2.3 : I2C1_SDA */
		Chip_SCU_PinMuxSet(0x2, 4, (SCU_MODE_ZIF_DIS | SCU_MODE_INBUFF_EN | SCU_MODE_FUNC1));		/* P2.4 : I2C1_SCL */
	} else {
		Chip_SCU_I2C0PinConfig(I2C0_STANDARD_FAST_MODE);
	}
}

void Board_SDMMC_Init(void)
{

#ifdef BOARD_BAMBINO
	Chip_SCU_PinMuxSet(0x1, 9, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* P1.9 connected to SDIO_D0 */
	Chip_SCU_PinMuxSet(0x1, 10, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* P1.10 connected to SDIO_D1 */
	Chip_SCU_PinMuxSet(0x1, 11, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* P1.11 connected to SDIO_D2 */
	Chip_SCU_PinMuxSet(0x1, 12, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* P1.12 connected to SDIO_D3 */
	Chip_SCU_PinMuxSet(0x1, 6, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* P1.6 connected to SDIO_CMD */
	Chip_SCU_PinMuxSet(0x1, 13, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* P1.13 connected to SD_CD */
	Chip_SCU_ClockPinMuxSet(2, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_FUNC4));	/* CLK2 connected to SDIO_CLK */
#elif defined(BOARD_MINIALPHA)
	Chip_SCU_PinMuxSet(0x1, 9, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* P1.9 connected to SDIO_D0 */
	Chip_SCU_PinMuxSet(0x1, 10, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* P1.10 connected to SDIO_D1 */
	Chip_SCU_PinMuxSet(0x1, 11, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* P1.11 connected to SDIO_D2 */
	Chip_SCU_PinMuxSet(0x1, 12, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* P1.12 connected to SDIO_D3 */
	Chip_SCU_PinMuxSet(0x1, 6, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* P1.6 connected to SDIO_CMD */
	Chip_SCU_PinMuxSet(0xC, 8, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* PC.8 connected to SD_CD */
	Chip_SCU_PinMuxSet(0xC, 0, (SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC7));	/* PC.0 connected to SDIO_CLK */
#elif defined(BOARD_PRIMEALPHA)
	Chip_SCU_PinMuxSet(0xC, 4, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* PC.4 connected to SDIO_D0 */
	Chip_SCU_PinMuxSet(0xC, 5, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* PC.5 connected to SDIO_D1 */
	Chip_SCU_PinMuxSet(0xC, 6, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* PC.6 connected to SDIO_D2 */
	Chip_SCU_PinMuxSet(0xC, 7, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* PC.7 connected to SDIO_D3 */
	Chip_SCU_PinMuxSet(0xC, 10, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* PC.10 connected to SDIO_CMD */
	Chip_SCU_PinMuxSet(0xC, 8, (SCU_PINIO_FAST | SCU_MODE_FUNC7));	/* PC.8 connected to SD_CD */
	Chip_SCU_PinMuxSet(0xC, 0, (SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_HIGHSPEEDSLEW_EN | SCU_MODE_FUNC7));	/* PC.0 connected to SDIO_CLK */
#else
#error board not defined for SDMMC
#endif
}

void Board_SSP_Init(LPC_SSP_T *pSSP)
{
#ifdef BOARD_BAMBINO
	if (pSSP == LPC_SSP1) {
		Chip_SCU_PinMuxSet(0xf, 4, (SCU_PINIO_FAST | SCU_MODE_FUNC0)); /*  PF.4 CLK for SSP1*/
		Chip_SCU_PinMuxSet(0x1, 5, (SCU_PINIO_FAST | SCU_MODE_FUNC5)); /* P1.5  SSEL1 */
		Chip_SCU_PinMuxSet(0x1, 3, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC5));/* P1.3  MISO1 */
		Chip_SCU_PinMuxSet(0x1, 4, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC5));/* P1.4  MOSI1 */
	} else if(pSSP == LPC_SSP0) {
		Chip_SCU_PinMuxSet(0x3, 0, (SCU_PINIO_FAST | SCU_MODE_FUNC4)); /*  P3.0 CLK for SSP0*/
		Chip_SCU_PinMuxSet(0x1, 0, (SCU_PINIO_FAST | SCU_MODE_FUNC5)); /* P1.0  SSEL0 */
		Chip_SCU_PinMuxSet(0x1, 1, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC5));/* P1.1 MISO0 */
		Chip_SCU_PinMuxSet(0x1, 2, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC5));/* P1.2  MOSI0 */
	}

#elif defined(BOARD_PRIMEALPHA)

	if (pSSP == LPC_SSP1) {
		// on G3
		Chip_SCU_PinMuxSet(0xf, 4, (SCU_PINIO_FAST | SCU_MODE_FUNC0)); /*  PF.4 CLK for SSP1*/
		Chip_SCU_PinMuxSet(0xf, 5, (SCU_PINIO_FAST | SCU_MODE_FUNC2)); /* PF.5  SSEL1 */
		Chip_SCU_PinMuxSet(0xf, 6, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC2));/* PF.6  MISO1 */
		Chip_SCU_PinMuxSet(0xf, 7, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC2));/* PF.7  MOSI1 */
	} else if(pSSP == LPC_SSP0) {
		// motor drivers
		Chip_SCU_PinMuxSet(0xF, 0, (SCU_PINIO_FAST | SCU_MODE_FUNC0)); /*  PF.0 CLK for SSP0*/
		Chip_SCU_PinMuxSet(0xF, 1, (SCU_PINIO_FAST | SCU_MODE_FUNC2)); /* PF.1  SSEL0 */
		Chip_SCU_PinMuxSet(0xF, 2, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC2));/* PF.2 MISO0 */
		Chip_SCU_PinMuxSet(0xF, 3, (SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC2));/* PF.3  MOSI0 */
	}
#elif defined(BOARD_MINIALPHA)
	// TODO
	#warning "SSP on minialpha not setup"
#else
#error board not defined for SSP1
#endif
}

/**
 * @}
 */






