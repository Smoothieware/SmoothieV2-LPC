#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "SlowTicker.h"

static volatile int timer_cnt20= 0;
static volatile int timer_cnt100= 0;

static void timer_callback20(void)
{
    ++timer_cnt20;
}

static void timer_callback100(void)
{
    ++timer_cnt100;
}

REGISTER_TEST(SlowTicker, test_20_and_100_hz)
{
    // SlowTicker *slowticker= new SlowTicker;
    SlowTicker *slt= SlowTicker::getInstance();
    // TEST_ASSERT_TRUE(slowticker == slt);
    // TEST_ASSERT_TRUE(slt->start());

    // 20 Hz
    int n1= slt->attach(20, timer_callback20);
    TEST_ASSERT_TRUE(n1 >= 0);

    // 100 Hz
    int n2= slt->attach(100, timer_callback100);
    TEST_ASSERT_TRUE(n2 >= 0 && n2 > n1);

    timer_cnt20= 0;
    timer_cnt100= 0;
    // test for 5 seconds which should be around 100 callbacks for 20 and 500 for 100
    for (int i = 0; i < 5; ++i) {
        vTaskDelay(pdMS_TO_TICKS(1000));;
        printf("time %d seconds, timer20 %d, timer100 %d\n", i+1, timer_cnt20, timer_cnt100);
    }

    slt->detach(n1);
    slt->detach(n2);
    //TEST_ASSERT_TRUE(slt->stop());

    TEST_ASSERT_INT_WITHIN(2, 100, timer_cnt20);
    TEST_ASSERT_INT_WITHIN(2, 500, timer_cnt100);
}

using systime_t= uint32_t;
#define clock_systimer() ((systime_t)Chip_RIT_GetCounter(LPC_RITIMER))
#define TICK2USEC(x) ((systime_t)(((uint64_t)(x)*1000000)/timerFreq))
#define TICKS2MS( xTicks ) ( (uint32_t) ( ((uint64_t)(xTicks) * 1000) / configTICK_RATE_HZ ) )
REGISTER_TEST(SlowTicker, xTaskGetTickCount)
{
    /* Get RIT timer peripheral clock rate */
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_RITIMER);
    static TickType_t last_time_check = xTaskGetTickCount();
    int cnt= 0;

    systime_t st = clock_systimer();
    while(cnt < 10) {
        if(TICKS2MS(xTaskGetTickCount() - last_time_check) >= 100) {
            last_time_check = xTaskGetTickCount();
            cnt++;
        }
    }
    systime_t en = clock_systimer();
    printf("elapsed time %lu us\n", TICK2USEC(en-st));
    TEST_ASSERT_INT_WITHIN(1000, 1000000, TICK2USEC(en-st));
}
