#include <stdio.h>
#include <math.h>

#include "board.h"
#include "FreeRTOS.h"


// TODO move ramfunc define to a utils.h
#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))

static void (*tick_handler)();
static void (*untick_handler)();
// the period for the unstep tick
static uint32_t delay_period;

_ramfunc_ void TIMER0_IRQHandler(void)
{
    bool m1= false;
    bool m2= false;

    if (Chip_TIMER_MatchPending(LPC_TIMER0, 0)) {
        Chip_TIMER_ClearMatch(LPC_TIMER0, 0);
        m1= true;
    }

    if (Chip_TIMER_MatchPending(LPC_TIMER0, 1)) { // MR1 match interrupt
        Chip_TIMER_ClearMatch(LPC_TIMER0, 1);
        m2= true;
    }

    if(m2) { // MR1 match interrupt
        // disable the MR1 match interrupt as it is a one shot
        Chip_TIMER_MatchDisableInt(LPC_TIMER0, 1);
        // call upstream handler
        untick_handler();
    }

    if(m1) { // MR0 match interrupt
        tick_handler();
    }
}

// frequency in HZ, delay in microseconds
int tmr0_setup(uint32_t frequency, uint32_t delay, void *mr0handler, void *mr1handler)
{
    /* Enable timer 0 clock and reset it */
    Chip_TIMER_Init(LPC_TIMER0);
    Chip_RGU_TriggerReset(RGU_TIMER0_RST);
    while (Chip_RGU_InReset(RGU_TIMER0_RST)) {}

    /* Get timer 0 peripheral clock rate */
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_TIMER0);
    printf("TMR0 clock rate= %lu\n", timerFreq);

    /* Timer setup for match and interrupt at TICKRATE_HZ */
    Chip_TIMER_Reset(LPC_TIMER0);
    Chip_TIMER_MatchEnableInt(LPC_TIMER0, 0);

   // setup step tick period
    uint32_t period1 = timerFreq / frequency;
    Chip_TIMER_SetMatch(LPC_TIMER0, 0, period1);
    Chip_TIMER_ResetOnMatchEnable(LPC_TIMER0, 0);
    Chip_TIMER_Enable(LPC_TIMER0);

    printf("TMR0 MR0 period=%lu cycles; interrupt rate=%lu Hz\n", period1, timerFreq / period1);

    // calculate ideal period for MR1 for unstep interrupt
    // we do not set it here as it will need to add the current TC when it is enabled
    // note that the MR1 match interrupt starts off disabled
    delay_period = roundf(delay / (1000000.0F / timerFreq)); // delay is in us
    printf("TMR0 MR1 period=%lu cycles; pulse width=%f us\n", delay_period, ((float)delay_period*1000000)/timerFreq);

    // setup the upstream handlers for each interrupt
    tick_handler = mr0handler;
    untick_handler = mr1handler;

    /* Set the priority of the TMR0 interrupt vector */
    NVIC_SetPriority(TIMER0_IRQn, 0); //configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY-1); // cannot call any RTOS stuff from this IRQ
    /* Enable the timer interrupt at the NVIC and at TMR0 */
    NVIC_EnableIRQ(TIMER0_IRQn);
    NVIC_ClearPendingIRQ(TIMER0_IRQn);

    // return the inaccuracy of the frequency if it does not exactly divide the frequency
    return timerFreq % period1;
}

void tmr0_stop()
{
   Chip_TIMER_Disable(LPC_TIMER0);
   NVIC_DisableIRQ(TIMER0_IRQn);
}

// called from within TMR0 ISR so must be in SRAM
_ramfunc_ void tmr0_mr1_start()
{
    // we read the current TC and add that to the period we need so we get the full pulse width
    // as it takes so much time to call this from when the MR0 happend we could be be past the MR1 period
    // by adding in the current TC we guarantee we are going to hit the MR1 match point
    // TODO we should really check this does not exceed the MR0 match otherwise unstep will not happen this step cycle

    uint32_t tc = Chip_TIMER_ReadCount(LPC_TIMER0);
    Chip_TIMER_SetMatch(LPC_TIMER0, 1, delay_period+tc);

    // enable the MR1 match interrupt
    Chip_TIMER_MatchEnableInt(LPC_TIMER0, 1);
}

static void (*tmr1_handler)();
_ramfunc_ void TIMER1_IRQHandler(void)
{
    if (Chip_TIMER_MatchPending(LPC_TIMER1, 0)) {
        Chip_TIMER_ClearMatch(LPC_TIMER1, 0);
        tmr1_handler();
    }
}

static uint32_t tmr1_timerFreq;
// frequency in HZ
int tmr1_setup(uint32_t frequency, void *timer_handler)
{
    /* Enable timer 1 clock and reset it */
    Chip_TIMER_Init(LPC_TIMER1);
    Chip_RGU_TriggerReset(RGU_TIMER1_RST);
    while (Chip_RGU_InReset(RGU_TIMER1_RST)) {}

    /* Get timer 1 peripheral clock rate */
    tmr1_timerFreq = Chip_Clock_GetRate(CLK_MX_TIMER1);
    printf("TMR1 clock rate= %lu\n", tmr1_timerFreq);

    /* Timer setup for match and interrupt at TICKRATE_HZ */
    Chip_TIMER_Reset(LPC_TIMER1);
    Chip_TIMER_MatchEnableInt(LPC_TIMER1, 0);

    // setup step tick period
    uint32_t period = tmr1_timerFreq / frequency;
    Chip_TIMER_SetMatch(LPC_TIMER1, 0, period);
    Chip_TIMER_ResetOnMatchEnable(LPC_TIMER1, 0);
    Chip_TIMER_Enable(LPC_TIMER1);

    printf("TMR1 MR0 period=%lu cycles; interrupt rate=%lu Hz\n", period, tmr1_timerFreq / period);

    // setup the upstream handler
    tmr1_handler = timer_handler;

    /* Set the priority of the TMR1 interrupt vector */
    NVIC_SetPriority(TIMER1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    /* Enable the timer interrupt at the NVIC and at TMR1 */
    NVIC_EnableIRQ(TIMER1_IRQn);
    NVIC_ClearPendingIRQ(TIMER1_IRQn);

    // return the inaccuracy of the frequency if it does not exactly divide the frequency
    return tmr1_timerFreq % period;
}

int tmr1_set_frequency(uint32_t frequency)
{
    Chip_TIMER_Disable(LPC_TIMER1);
    NVIC_DisableIRQ(TIMER1_IRQn);
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    Chip_TIMER_Reset(LPC_TIMER1);
    Chip_TIMER_ClearMatch(LPC_TIMER1, 0);

    // setup new tick period
    uint32_t period = tmr1_timerFreq / frequency;
    Chip_TIMER_SetMatch(LPC_TIMER1, 0, period);
    Chip_TIMER_MatchEnableInt(LPC_TIMER1, 0);
    Chip_TIMER_ResetOnMatchEnable(LPC_TIMER1, 0);
    Chip_TIMER_Enable(LPC_TIMER1);

    printf("TMR1 new MR0 period=%lu cycles; interrupt rate=%lu Hz\n", period, tmr1_timerFreq / period);
    /* Enable the timer interrupt at the NVIC and at TMR1 */
    NVIC_EnableIRQ(TIMER1_IRQn);
    return tmr1_timerFreq % period;
}

void tmr1_stop()
{
   Chip_TIMER_Disable(LPC_TIMER1);
   NVIC_DisableIRQ(TIMER1_IRQn);
}
