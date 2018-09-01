#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "FastTicker.h"

#include "FreeRTOS.h"
#include "task.h"

static volatile int timer_cnt2khz= 0;
static volatile int timer_cnt10khz= 0;

static void timer_callback2khz(void)
{
    ++timer_cnt2khz;
}

static void timer_callback10khz(void)
{
    ++timer_cnt10khz;
}

REGISTER_TEST(FastTicker, test_20_and_100_hz)
{
    FastTicker *flt= FastTicker::getInstance();
    if(flt == nullptr) {
        // This just allows FastTicker to already have been setup
        flt= new FastTicker;
        TEST_ASSERT_TRUE(flt == FastTicker::getInstance());
    }
    TEST_ASSERT_FALSE(flt->is_running());

    // 2 KHz
    int n1= flt->attach(2000, timer_callback2khz);
    TEST_ASSERT_TRUE(n1 >= 0);
    TEST_ASSERT_TRUE(flt->is_running());

    // 10 KHz
    int n2= flt->attach(10000, timer_callback10khz);
    TEST_ASSERT_TRUE(n2 >= 0 && n2 > n1);

    timer_cnt2khz= 0;
    timer_cnt10khz= 0;
    // test for 5 seconds
    for (int i = 0; i < 5; ++i) {
        vTaskDelay(pdMS_TO_TICKS(1000));;
        printf("time %d seconds, timer2khz %d, timer10khz %d\n", i+1, timer_cnt2khz, timer_cnt10khz);
    }
    TEST_ASSERT_TRUE(flt->stop());

    flt->detach(n1);
    flt->detach(n2);

    printf("timer2khz %d, timer10khz %d\n", timer_cnt2khz, timer_cnt10khz);
    TEST_ASSERT_INT_WITHIN(5, 5*2000, timer_cnt2khz);
    TEST_ASSERT_INT_WITHIN(10, 5*10000, timer_cnt10khz);
}
