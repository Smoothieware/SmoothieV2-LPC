#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "SlowTicker.h"

static volatile int timer_cnt= 0;

static void timer_callback(void)
{
    ++timer_cnt;
}

REGISTER_TEST(SlowTicker, test_20_hz)
{
    SlowTicker *slowticker= new SlowTicker;
    SlowTicker *slt= SlowTicker::getInstance();

    TEST_ASSERT_TRUE(slowticker == slt);

    TEST_ASSERT_TRUE(slt->start());

    int n= slt->attach(20, timer_callback);

    for (int i = 0; i < 5; ++i) {
        vTaskDelay(pdMS_TO_TICKS(1000));;
        printf("time %d seconds, timer %d calls\n", i+1, timer_cnt);
    }

    slt->detach(n);
    TEST_ASSERT_TRUE(slt->stop());

    TEST_ASSERT_INT_WITHIN(2, 100, timer_cnt);

    delete slowticker;
}
