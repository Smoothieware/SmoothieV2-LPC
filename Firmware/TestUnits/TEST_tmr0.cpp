#include "../Unity/src/unity.h"
#include "TestRegistry.h"
#include "Timers.h"

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
    if(timer_cnt == 0) {
        tmr0_mr1_start(); // kick off unstep timer
        unstep_start= clock_systimer();
    }
    ++timer_cnt;
}

static _ramfunc_ void unstep_timer_handler()
{
    unstep_stop= clock_systimer();
}


REGISTER_TEST(TMR0Test, test_10000_hz)
{
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_RITIMER);

    /* Start the timer 10KHz, with 10us delay */
    int permod = tmr0_setup(10000, 10, (void *)step_timer_handler, (void *)unstep_timer_handler);
    if(permod <  0) {
        printf("ERROR: tmr0 setup failed\n");
        TEST_FAIL();
    }
    if(permod != 0) {
        printf("Warning: stepticker is not accurate: %d\n", permod);
    }

    // wait 1 second
    systime_t t1= clock_systimer();
    vTaskDelay(pdMS_TO_TICKS(1000));
    systime_t t2= clock_systimer();

    /* Stop the timer */
    tmr0_stop();
    systime_t unstep_time= TICK2USEC(unstep_stop-unstep_start);

    // TODO time the period between step ticks
    printf("%lu - %lu\n", unstep_start, unstep_stop);

    printf("elapsed time %lu us, unstep time %lu us, timer cnt %d\n", TICK2USEC(t2-t1), unstep_time, timer_cnt);

    TEST_ASSERT_INT_WITHIN(10, 10000, timer_cnt);
    TEST_ASSERT_INT_WITHIN(1, 10, unstep_time);

}
