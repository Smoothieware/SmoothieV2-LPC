#include "../Unity/src/unity.h"
#include "TestRegistry.h"
#include "tmr-setup.h"

#include "board.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>

using systime_t= uint32_t;
#define clock_systimer() ((systime_t)Chip_RIT_GetCounter(LPC_RITIMER))
#define TICK2USEC(x) ((systime_t)(((uint64_t)(x)*1000000)/timerFreq))
#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))

static systime_t unstep_start= 0, unstep_stop= 0;
static volatile int timer_cnt= 0;

static _ramfunc_ void step_timer_handler()
{
    if(timer_cnt == 100) {
        tmr0_mr1_start(); // kick off unstep timer
        unstep_start= clock_systimer();
    }
    ++timer_cnt;
}

static _ramfunc_ void unstep_timer_handler()
{
    unstep_stop= clock_systimer();
}

#define FREQUENCY 100000 // 100KHz
#define PULSE     1 // 1us
REGISTER_TEST(TMR0Test, test_10000_hz)
{
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_RITIMER);

    // this interrupt should continue to run regardless of RTOS being in critical condition
    taskENTER_CRITICAL();
    /* Start the timer 100KHz, with 1us delay */
    int permod = tmr0_setup(FREQUENCY, PULSE, (void *)step_timer_handler, (void *)unstep_timer_handler);
    if(permod <  0) {
        printf("ERROR: tmr0 setup failed\n");
        TEST_FAIL();
    }
    if(permod != 0) {
        printf("Warning: stepticker is not accurate: %d\n", permod);
    }

    // wait for 100,000 ticks
    timer_cnt= 0;
    systime_t t1= clock_systimer();
    while(timer_cnt < FREQUENCY) ;
    systime_t t2= clock_systimer();

    /* Stop the timer */
    tmr0_stop();
    systime_t unstep_time= TICK2USEC(unstep_stop-unstep_start);

    printf("unstep time: %lu - %lu = %lu us\n", unstep_stop, unstep_start, unstep_time);
    systime_t elapsed= TICK2USEC(t2-t1);

    printf("elapsed time %lu us, period %f, unstep time %lu us, timer cnt %d\n", elapsed, (float)elapsed/timer_cnt, unstep_time, timer_cnt);

    TEST_ASSERT_TRUE(unstep_stop != 0);
    TEST_ASSERT_INT_WITHIN(1, 1000000/FREQUENCY, elapsed/timer_cnt); // 50us period
    TEST_ASSERT_INT_WITHIN(1, PULSE, unstep_time);
    taskEXIT_CRITICAL();
}
