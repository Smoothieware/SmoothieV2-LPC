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
