/*
 * @brief SD/MMC example
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2013
 * All rights reserved.
 *
 * @par
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
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include <string.h>
#include <stdio.h>

#include "board.h"
#include "chip.h"
#include "rtc.h"
#include "ff.h"
#include "spifi_18xx_43xx.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

static void debugstr(const char *str) {
	Chip_UART_SendBlocking(LPC_USART0, str, strlen(str));
}

/* buffer size (in byte) for R/W operations */
#define BUFFER_SIZE     4096

static FATFS Fatfs;	/* File system object */
static FIL Fil;	/* File object */
static uint32_t Buff[BUFFER_SIZE/sizeof(uint32_t)];

static volatile UINT Timer = 0;		/* Performance timer (1kHz increment) */
static volatile int32_t sdio_wait_exit = 0;

const uint32_t ExtRateIn = 0;
const uint32_t OscRateIn = 12000000;


/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/* SDMMC card info structure */
mci_card_struct sdcardinfo;

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Delay callback for timed SDIF/SDMMC functions */
static void sdmmc_waitms(uint32_t time)
{
	/* In an RTOS, the thread would sleep allowing other threads to run.
	   For standalone operation, we just spin on RI timer */
	int32_t curr = (int32_t) Chip_RIT_GetCounter(LPC_RITIMER);
	int32_t final = curr + ((SystemCoreClock / 1000) * time);

	if (final == curr) return;

	if ((final < 0) && (curr > 0)) {
		while (Chip_RIT_GetCounter(LPC_RITIMER) < (uint32_t) final) {}
	}
	else {
		while ((int32_t) Chip_RIT_GetCounter(LPC_RITIMER) < final) {}
	}

	return;
}

/**
 * @brief	Sets up the SD event driven wakeup
 * @param	bits : Status bits to poll for command completion
 * @return	Nothing
 */
static void sdmmc_setup_wakeup(void *bits)
{
	uint32_t bit_mask = *((uint32_t *)bits);
	/* Wait for IRQ - for an RTOS, you would pend on an event here with a IRQ based wakeup. */
	NVIC_ClearPendingIRQ(SDIO_IRQn);
	sdio_wait_exit = 0;
	Chip_SDIF_SetIntMask(LPC_SDMMC, bit_mask);
	NVIC_EnableIRQ(SDIO_IRQn);
}

/**
 * @brief	A better wait callback for SDMMC driven by the IRQ flag
 * @return	0 on success, or failure condition (-1)
 */
static uint32_t sdmmc_irq_driven_wait(void)
{
	uint32_t status;

	/* Wait for event, would be nice to have a timeout, but keep it  simple */
	while (sdio_wait_exit == 0) {}

	/* Get status and clear interrupts */
	status = Chip_SDIF_GetIntStatus(LPC_SDMMC);
	Chip_SDIF_ClrIntStatus(LPC_SDMMC, status);
	Chip_SDIF_SetIntMask(LPC_SDMMC, 0);

	return status;
}

/* Initialize SD/MMC */
static void App_SDMMC_Init()
{
	memset(&sdcardinfo, 0, sizeof(sdcardinfo));
	sdcardinfo.card_info.evsetup_cb = sdmmc_setup_wakeup;
	sdcardinfo.card_info.waitfunc_cb = sdmmc_irq_driven_wait;
	sdcardinfo.card_info.msdelay_func = sdmmc_waitms;

	/*  SD/MMC initialization */
	//Board_SDMMC_Init();

	/* The SDIO driver needs to know the SDIO clock rate */
	Chip_SDIF_Init(LPC_SDMMC);
}


/*****************************************************************************
 * Public functions
 ****************************************************************************/

/**
 * @brief	Error processing function: stop with dying message
 * @param	rc	: FatFs return value
 * @return	Nothing
 */
void die(FRESULT rc)
{
	DEBUGSTR("Failed\n");
	__asm("bkpt #0");
}

/**
 * @brief	SDIO controller interrupt handler
 * @return	Nothing
 */
void SDIO_IRQHandler(void)
{
	/* All SD based register handling is done in the callback
	   function. The SDIO interrupt is not enabled as part of this
	   driver and needs to be enabled/disabled in the callbacks or
	   application as needed. This is to allow flexibility with IRQ
	   handling for applicaitons and RTOSes. */
	/* Set wait exit flag to tell wait function we are ready. In an RTOS,
	   this would trigger wakeup of a thread waiting for the IRQ. */
	NVIC_DisableIRQ(SDIO_IRQn);
	sdio_wait_exit = 1;
}

#define SPIFLASH_BASE_ADDRESS (0x14000000)

void spifi_reset(void) {
	LPC_SPIFI->STAT = SPIFI_STAT_RESET;
	while ((LPC_SPIFI->STAT & SPIFI_STAT_RESET) != 0) {}

	LPC_SPIFI->MEMCMD =0;
	LPC_SPIFI->DATINTM =0;

	LPC_SPIFI->STAT = SPIFI_STAT_RESET;
	while ((LPC_SPIFI->STAT & SPIFI_STAT_RESET) != 0) {}
}

void spifi_init(void) {

	// when booting from SPIFI these pins are all enabled as SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS | SCU_MODE_FUNC3

	// LPC_SCU->SFSP[0x3][3] = (SCU_PINIO_FAST | SCU_MODE_FUNC3);	/* SPIFI CLK */
	// LPC_SCU->SFSP[0x3][4] = (SCU_PINIO_FAST | SCU_MODE_FUNC3);	/* SPIFI D3 */
	// LPC_SCU->SFSP[0x3][5] = (SCU_PINIO_FAST | SCU_MODE_FUNC3);	/* SPIFI D2 */
	// LPC_SCU->SFSP[0x3][6] = (SCU_PINIO_FAST | SCU_MODE_FUNC3);	/* SPIFI D1 */
	// LPC_SCU->SFSP[0x3][7] = (SCU_PINIO_FAST | SCU_MODE_FUNC3);	/* SPIFI D0 */
	// LPC_SCU->SFSP[0x3][8] = (SCU_PINIO_FAST | SCU_MODE_FUNC3);	/* SPIFI CS/SSEL */

	// enable FBCLK and change defaults

	LPC_SPIFI->CTRL = SPIFI_CTRL_TO(1000) |			// default value is 0xFFFF
										SPIFI_CTRL_CSHI(15) |			// this is default value
										SPIFI_CTRL_RFCLK(1) |			// this is default value
										SPIFI_CTRL_FBCLK(1);

};

/*****************************************************************************
 * Switch between command mode and memory mode on SPIFI
 *	-- memory mode reads as code/data memory at 0x14000000 / 0x80000000
 ****************************************************************************/
void spifi_memory_mode(void) {
  #ifdef CORE_M4
	SCnSCB->ACTLR &= ~2; // disable Cortex write buffer to avoid exceptions when switching back to SPIFI for execution
  #endif

	LPC_SPIFI->STAT = 0x10;	// reset memory mode
	while(LPC_SPIFI->STAT & 0x10);	// wait for reset to complete

	LPC_SPIFI->CTRL = // set up new CTRL register with high speed options
		(0x100 << 0) | 	// timeout
		(0x1 << 16) | 	// cs high, this parameter is dependent on the SPI Flash part and is in SPIFI_CLK cycles. A lower cs high performs faster
		(1 << 29) | 		// receive full clock (rfclk) - allows for higher speeds
		(1 << 30); 			// feedback clock (fbclk) - allows for higher speeds

	// put part in high speed mode (skipping opcodes)
	LPC_SPIFI->DATINTM = 0xa5; // 0xAx will cause part to use high performace reads (skips opcode on subsequent accesses)
	LPC_SPIFI->CMD =
		(0xebul << 24) | 	// opcode 0xeb Quad IO High Performance Read for Spansion
		(0x4 << 21) | 		// frame form indicating opcode and 3 address bytes
		(0x2 << 19) | 		// field form indicating serial opcode and dual/quad other fields
		(0x3 << 16); 			// 3 intermediate data bytes
	while(LPC_SPIFI->STAT & 2); // wait for command to complete

	LPC_SPIFI->MEMCMD =
		(0xebul << 24) | 	// opcode 0xeb Quad IO High Performance Read for Spansion
		(0x6 << 21) | 		// frame form indicating no opcode and 3 address bytes
		(0x2 << 19) | 		// field form indicating serial opcode and dual/quad other fields
		(0x3 << 16); 			// 3 intermediate data bytes

  #ifdef CORE_M4
	SCnSCB->ACTLR |= 2; // reenable Cortex write buffer
  #endif
}

void spifi_command_mode(void) {
	LPC_SPIFI->STAT = 0x10;	// reset memory mode
	while(LPC_SPIFI->STAT & 0x10);	// wait for reset to complete

	LPC_SPIFI->ADDR = 0xffffffff;
	LPC_SPIFI->DATINTM = 0xffffffff;
	LPC_SPIFI->CMD = 		// send all ones for a while to hopefully reset SPI Flash
		(0xfful << 24) | 	// opcode 0xff
		(0x5 << 21) | 		// frame form indicating opcode and 4 address bytes
		(0x0 << 19) | 		// field form indicating all serial fields
		(0x4 << 16); 			// 3 intermediate data bytes
	while(LPC_SPIFI->STAT & 2); // wait for command to complete
}

#define QUAD_WRITE
void spifi_4K_write(int address, int * copy) {
	int aligned_address = address & ~(0xfff);
	int offset = (address & 0xfff)>>2;
	int i, j;

	LPC_SPIFI->DATINTM = 0x0; // next read command will remove high performance mode
	LPC_SPIFI->ADDR = aligned_address;

	LPC_SPIFI->CMD =
		(0x06ul << 24) | 	// opcode 0x06 Write Enable for Spansion
		(0x1 << 21) | 		// frame form indicating opcode only
		(0x0 << 19); 			// field form indicating all serial
	while(LPC_SPIFI->STAT & 2); // wait for command to complete
	LPC_SPIFI->CMD =
		(0x20 << 24) | 		// opcode 0x20 Sector Erase for Spansion
		(0x4 << 21) | 		// frame form indicating opcode and 3 address bytes
		(0x0 << 19); 			// field form indicating all serial
	while(LPC_SPIFI->STAT & 2); // wait for command to complete
	LPC_SPIFI->CMD =
		(0x05ul << 24) | 	// opcode 0x05 Read Status for Spansion
		(0x1 << 21) | 		// frame form indicating opcode only
		(0x0 << 19) | 		// field form indicating all serial
		(0x1 << 14) | 		// POLLRS polling command
		(0x0 << 2) | 			// check bit 0
		(0x0 << 0); 			// wait till 0
	while(LPC_SPIFI->STAT & 2); // wait for command to complete
	*(volatile char*)&LPC_SPIFI->DAT8;

	for(j = 0; j < 1024; j += 64) {
		LPC_SPIFI->CMD =
			(0x06ul << 24) | // opcode 0x06 Write Enable for Spansion
			(0x1 << 21) | 	// frame form indicating opcode only
			(0x0 << 19); 		// field form indicating all serial
		while(LPC_SPIFI->STAT & 2); // wait for command to complete
#ifdef QUAD_WRITE
		LPC_SPIFI->CMD =
			(0x32ul << 24) | // opcode 0x32 Quad Page Programming for Spansion
			(0x4 << 21) | 	// frame form indicating opcode and 3 address bytes
			(0x1 << 19) | 	// field form indicating quad data field, others serial
			(0x0 << 16) | 	// 0 intermediate data bytes
			(0x1 << 15) | 	// dout indicates that it is a write
			(256); 					// datalen
#else	// SPI Serial WRITE
		LPC_SPIFI->CMD =
			(0x02ul << 24) | // opcode 0x02 Page Programming for Spansion
			(0x4 << 21) | 	// frame form indicating opcode and 3 address bytes
			(0x0 << 19) |		// field form indicating all serial
			(0x0 << 16) | 	// 0 intermediate data bytes
			(0x1 << 15) | 	// dout indicates that it is a write
			(256); 					// datalen
#endif
		for(i = 0; i < 64; i++) {
			if (j+i >= offset) {
				*(volatile int*)&LPC_SPIFI->DAT32 = copy[j + i - offset];
			} else {
				*(volatile int*)&LPC_SPIFI->DAT32 = 0;
			}
		}
		while(LPC_SPIFI->STAT & 2); // wait for command to complete
		LPC_SPIFI->CMD =
			(0x05ul << 24) | 	// opcode 0x05 Read Status for Spansion
			(0x1 << 21) | 		// frame form indicating opcode only
			(0x0 << 19) | 		// field form indicating all serial
			(0x1 << 14) | 		// POLLRS polling command
			(0x0 << 2) | 			// check bit 0
			(0x0 << 0); 			// wait till 0
		while(LPC_SPIFI->STAT & 2); // wait for command to complete
		*(volatile char*)&LPC_SPIFI->DAT8;
		LPC_SPIFI->ADDR += 256;
	}
}

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

void flashloader()
{
	FRESULT rc;		/* Result code */
	UINT br;
	char *cbuf = (char *) Buff;

	Board_LED_Init();

	SystemCoreClockUpdate();

	// do not need to do this if called from a running system
	//Board_Init();

	App_SDMMC_Init();

    // Reenable interrupts
	__enable_irq();

	/* There is no need to initialize RTC here it is called by fs init
	   but it takes long time hence doing it before calling f_open */
	//debugstr("Initializing RTC (might take few seconds)...");
	//rtc_initialize();
	//debugstr("Done\r\n");

	debugstr("Standalone flash loader\n");

	NVIC_DisableIRQ(SDIO_IRQn);
	/* Enable SD/MMC Interrupt */
	NVIC_EnableIRQ(SDIO_IRQn);

	f_mount(0, &Fatfs);		/* Register volume work area (never fails) */

	debugstr("Opening flashme.bin from SD Card...\n");
	rc = f_open(&Fil, "flashme.bin", FA_READ);
	if (rc) {
		debugstr("no flashme.bin found\r\n");
		sdmmc_waitms(5000);
		*(volatile int*)0x40053100 = 1; // reset core
	}
	debugstr("Done.\r\n");

	debugstr("Flashing contents of flashme.bin...\r\n");

	spifi_init();
 	spifi_command_mode();

 	uint8_t cnt= 0;
 	uint32_t page= 0;
	for (;; ) {
		/* Read a chunk of file */
		rc = f_read(&Fil, (void *)cbuf, BUFFER_SIZE, &br);
		if (rc || !br) {
			break;					/* Error or end of file */
		}

		// flash it
		spifi_4K_write(SPIFLASH_BASE_ADDRESS+page, (int *)cbuf);
		page += 4096;

		// count up leds to show progress
		cnt++;
		Board_LED_Set(0, cnt&1);
		Board_LED_Set(1, cnt&2);
		Board_LED_Set(2, cnt&4);
		Board_LED_Set(3, cnt&8);
	}

	spifi_memory_mode();

	if (rc) {
		debugstr("Flash Failed.\r\n");
		die(rc);
	}

	rc = f_close(&Fil);

	// we are readonly
    // rc= f_rename("flashme.bin", "flashme.old");
	// if (rc) {
	// 	debugstr("Rename Failed.\r\n");
	// }

	debugstr("Flash completed. Rebooting in 5 seconds\r\n");
	sdmmc_waitms(5000);

	*(volatile int*)0x40053100 = 1; // reset core
	for (;; ) {}
}

void SysTick_IRQHandler(void)
{
	// do nothing
}

//*****************************************************************************
// Functions to carry out the initialization of RW and BSS data sections. These
// are written as separate functions rather than being inlined within the
// ResetISR() function in order to cope with MCUs with multiple banks of
// memory.
//*****************************************************************************
__attribute__((section(".after_vectors")))
void data_init(unsigned int romstart, unsigned int start, unsigned int len) {
    unsigned int *pulDest = (unsigned int*) start;
    unsigned int *pulSrc = (unsigned int*) romstart;
    unsigned int loop;
    for (loop = 0; loop < len; loop = loop + 4)
        *pulDest++ = *pulSrc++;
}

__attribute__ ((section(".after_vectors")))
void bss_init(unsigned int start, unsigned int len) {
    unsigned int *pulDest = (unsigned int*) start;
    unsigned int loop;
    for (loop = 0; loop < len; loop = loop + 4)
        *pulDest++ = 0;
}

//*****************************************************************************
// The following symbols are constructs generated by the linker, indicating
// the location of various points in the "Global Section Table". This table is
// created by the linker via the Code Red managed linker script mechanism. It
// contains the load address, execution address and length of each RW data
// section and the execution and length of each BSS (zero initialized) section.
//*****************************************************************************
extern unsigned int __data_section_table;
extern unsigned int __data_section_table_end;
extern unsigned int __bss_section_table;
extern unsigned int __bss_section_table_end;
extern void _vStackTop(void);

/**
 * @brief	Main routine for flashloader
 * @return	Nothing
 */
__attribute__ ((used,section(".flash_text")))
void flashloaderinit(void)
{
	// set stack pointer
	__set_PSP((uint32_t)&_vStackTop-8);
	__set_MSP((uint32_t)&_vStackTop-8);

    volatile unsigned int *NVIC_ICPR = (unsigned int *) 0xE000E280;
    unsigned int irqpendloop;
    for (irqpendloop = 0; irqpendloop < 8; irqpendloop++) {
        *(NVIC_ICPR + irqpendloop) = 0xFFFFFFFF;
    }

    // set vector to point to our handler
    // SDIO_IRQHandler
    uint32_t *pVectors= (uint32_t *)(0x10000000+(22*4));
    *pVectors= (uint32_t)SDIO_IRQHandler;
    uint32_t *p2Vectors= (uint32_t *)(0x10000000+(15*4));
    *p2Vectors= (uint32_t)SysTick_IRQHandler;

    //
    // Copy the text and data sections from flash to SRAM.
    //
    unsigned int LoadAddr, ExeAddr, SectionLen;
    unsigned int *SectionTableAddr;

    // Load base address of Global Section Table
    SectionTableAddr = &__data_section_table;

    // Copy the data sections from flash to SRAM.
    while (SectionTableAddr < &__data_section_table_end) {
        LoadAddr = *SectionTableAddr++;
        ExeAddr = *SectionTableAddr++;
        SectionLen = *SectionTableAddr++;
        data_init(LoadAddr, ExeAddr, SectionLen);
    }
    // At this point, SectionTableAddr = &__bss_section_table;
    // Zero fill the bss segment
    while (SectionTableAddr < &__bss_section_table_end) {
        ExeAddr = *SectionTableAddr++;
        SectionLen = *SectionTableAddr++;
        bss_init(ExeAddr, SectionLen);
    }

    flashloader();
}


__attribute__ ((used,section(".isr_vector")))
void *g_pfnVectors[] = {
 	(void *)0x5555AAAAUL, // magic number
    (void*)&flashloaderinit       // start address
};
