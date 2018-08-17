/*
 * @brief State Configurable Timer Simple Match example
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

#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "sct_fsm.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/
#if defined(BOARD_HITEX_EVA_1850) || defined(BOARD_HITEX_EVA_4350)
#define MCSEL_BIT           23
#define MCSEL_PORT          6
#endif

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Initialise the SCT Pins */
void SCT_PinsConfigure(void)
{
#if (defined(BOARD_HITEX_EVA_1850) || defined(BOARD_HITEX_EVA_4350))
	/* Enable signals on MC connector X19 */
	Chip_SCU_PinMuxSet(0xD, 9, (SCU_MODE_PULLUP | SCU_MODE_FUNC4));		/* PD_9:  GPIO 6.23, MCSEL */
	Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, MCSEL_PORT, MCSEL_BIT);
	Chip_GPIO_SetPinState(LPC_GPIO_PORT, MCSEL_PORT, MCSEL_BIT, false);

	Chip_SCU_PinMuxSet(0xE, 6, (SCU_MODE_INACT | SCU_MODE_FUNC1));	/* PE_6:  SCTOUT_2 connected to RGB green */
	Chip_SCU_PinMuxSet(0xE, 5, (SCU_MODE_INACT | SCU_MODE_FUNC1));	/* PE_5:  SCTOUT_3 connected to RGB red */
	Chip_SCU_PinMuxSet(0xE, 8, (SCU_MODE_INACT | SCU_MODE_FUNC1));	/* P4_4:  SCTOUT_4 connected to RGB blue */
	Chip_SCU_PinMuxSet(0xE, 7, (SCU_MODE_INACT | SCU_MODE_FUNC1));	/* P4_3:  SCTOUT_5 */
	Chip_SCU_PinMuxSet(0xD, 3, (SCU_MODE_INACT | SCU_MODE_FUNC1));	/* PD_3:  SCTOUT_6 */

	/* Global configuration of the SCT */
	/* use Core clock */
	Chip_SCT_Config(LPC_SCT, (SCT_CONFIG_16BIT_COUNTER));

#elif (defined(BOARD_KEIL_MCB_4357) || defined(BOARD_KEIL_MCB_1857))

	Chip_SCU_PinMuxSet(0xD, 11, (SCU_MODE_INACT | SCU_MODE_FUNC6));	/* PD_11:  SCTOUT_14 connected to LED1 */
	Chip_SCU_PinMuxSet(0xD, 12, (SCU_MODE_INACT | SCU_MODE_FUNC6));	/* PD_12:  SCTOUT_10 connected to LED2 */
	Chip_SCU_PinMuxSet(0xD, 13, (SCU_MODE_INACT | SCU_MODE_FUNC6));	/* PD_13:  SCTOUT_13 connected to LED3 */
	Chip_SCU_PinMuxSet(0xD, 14, (SCU_MODE_INACT | SCU_MODE_FUNC6));	/* PD_14:  SCTOUT_11 connected to LED4 */
	Chip_SCU_PinMuxSet(0xD, 3, (SCU_MODE_INACT | SCU_MODE_FUNC1));		/* PD_3:  SCTOUT_6 connected  */

	/* Global configuration of the SCT */
	/* use Core clock */
	Chip_SCT_Config(LPC_SCT, (SCT_CONFIG_16BIT_COUNTER));

#elif (defined(BOARD_NGX_XPLORER_4330) || defined(BOARD_NGX_XPLORER_1830))

	Chip_SCU_PinMuxSet(0x2, 11, (SCU_MODE_INACT | SCU_MODE_FUNC1));	/* P2_11:  SCTOUT_5 connected to LED1 */
	Chip_SCU_PinMuxSet(0x2, 12, (SCU_MODE_INACT | SCU_MODE_FUNC1));	/* P2_12:  SCTOUT_4 connected to LED2 */
	Chip_SCU_PinMuxSet(0x2, 10, (SCU_MODE_INACT | SCU_MODE_FUNC1));	/* P2_10:  SCTOUT_2 */
	Chip_SCU_PinMuxSet(0x2, 9, (SCU_MODE_INACT | SCU_MODE_FUNC1));	/* P2_9:   SCTOUT_3 */
	Chip_SCU_PinMuxSet(0x6, 5, (SCU_MODE_INACT | SCU_MODE_FUNC1));		/* P6_5:   SCTOUT_6 */

	/* Global configuration of the SCT */
	/* use Core clock */
	Chip_SCT_Config(LPC_SCT, (SCT_CONFIG_16BIT_COUNTER));

#else
#error Board not supported!
#endif

}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/**
 * @brief	Main entry point
 * @return	Nothing
 */
int main(void)
{
	SystemCoreClockUpdate();
	Board_Init();

	/* Initialize SCT */
	Chip_SCT_Init(LPC_SCT);

	/* Configure SCT pins */
	SCT_PinsConfigure();

	Chip_SCT_SetClrControl(LPC_SCT, SCT_CTRL_CLRCTR_L | SCT_CTRL_HALT_L | SCT_CTRL_PRE_L(100 - 1)
						   | SCT_CTRL_HALT_H | SCT_CTRL_CLRCTR_H | SCT_CTRL_PRE_H(256 - 1),
						   ENABLE);

	/* Now use the FSM code to configure the state machine */
	sct_fsm_init();

	/* Start the SCT */
	Chip_SCT_SetClrControl(LPC_SCT, SCT_CTRL_STOP_L | SCT_CTRL_HALT_L | SCT_CTRL_STOP_H | SCT_CTRL_HALT_H, DISABLE);

	while (1) {}
}






