#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "Module.h"
#include "TemperatureSwitch.h"
#include "Dispatcher.h"
#include "OutputStream.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <memory>
#include <string.h>
#include <sstream>
#include "prettyprint.hpp"
#include <iostream>

#if 0
// this declares any global variables the test needs
TEST_DECLARE(TemperatureSwitch)

END_DECLARE

// called before each test
TEST_SETUP(TemperatureSwitch)
{
    printf("...Setup TemperatureSwitch\n");
}

// called after each test
TEST_TEARDOWN(TemperatureSwitch)
{
    // have kernel reset to a clean state
    //test_kernel_teardown();

    printf("...Teardown TemperatureSwitch\n");
}
#endif

// define various configs here, these are in the same format they would appear in the config.ini file
const static char edge_low_config[]= "\
[temperature switch]\n\
psu_off.enable = true \n\
psu_off.designator = T \n\
psu_off.switch = fan \n\
psu_off.threshold_temp = 50.0 \n\
psu_off.heatup_poll = 2 \n\
psu_off.cooldown_poll = 2 \n\
psu_off.arm_mcode = 1100 \n\
psu_off.trigger = falling \n\
psu_off.inverted = false \n\
";

const static char level_config[]= "\
[temperature switch]\n\
psu_off.enable = true \n\
psu_off.designator = T \n\
psu_off.switch = fan \n\
psu_off.threshold_temp = 50.0 \n\
psu_off.heatup_poll = 2 \n\
psu_off.cooldown_poll = 2 \n\
";

// Handle temperature request and switch status request
static bool switch_state;
static bool switch_get_hit= false;
static bool switch_set_hit= false;
static int hitcnt= 0;
static float return_current_temp= 0;

#define TICK2MS( xTicks ) ( (uint32_t) ( (xTicks * 1000) / configTICK_RATE_HZ ) )

using pad_temperature_t = struct pad_temperature {
    float current_temperature;
    float target_temperature;
    int pwm;
    uint8_t tool_id;
    std::string designator;
};

static void set_temp(float t)
{
    uint32_t last_time= xTaskGetTickCount();
    uint32_t msec;

    return_current_temp= t;
    // we need to call this for at least 2 seconds
    do {
        uint32_t now= xTaskGetTickCount();
        msec= TICK2MS(now-last_time);
        // stimulates the in_command_ctx
        Module::broadcast_in_commmand_ctx(true);
    } while (msec < 3000);

}
// Mock temperature control module
class MockTemperatureControl : public Module
{
public:
    MockTemperatureControl(const char *group, const char *name) : Module(group, name) {};
    bool request(const char *key, void *value)
    {
        if(strcmp(key, "get_current_temperature") == 0) {
            // we are passed a pad_temperature
            pad_temperature_t *t = static_cast<pad_temperature_t *>(value);

            // setup data
            t->current_temperature = return_current_temp;
            t->target_temperature = 185;
            t->pwm = 255;
            t->designator = "T";
            t->tool_id = 0;
            hitcnt++;
            return true;
        }

        return false;
    }
};

// Mock switch module
class MockSwitch : public Module
{
public:
    MockSwitch(const char *group, const char *name) : Module(group, name) {};
    bool request(const char *key, void *value)
    {
        if(strcmp(key, "state") == 0) {
            *(bool *)value = switch_state;
            switch_get_hit= true;

        } else if(strcmp(key, "set-state") == 0) {
            switch_state = *(bool*)value;
            switch_set_hit= true;

        } else {
            return false;
        }

        return true;
    }
};


REGISTER_TEST(TemperatureSwitch,level_low_high)
{
    // load config with required settings for this test
    std::stringstream ss(level_config);
    ConfigReader cr(ss);
    TEST_ASSERT_TRUE(TemperatureSwitch::load(cr));
    Module *m= Module::lookup("temperature switch", "psu_off");
    TEST_ASSERT_NOT_NULL(m);

    // so it gets destroyed when we go out of scope
    std::unique_ptr<TemperatureSwitch> nts(static_cast<TemperatureSwitch*>(m));
    TEST_ASSERT_NOT_NULL(nts.get());
    TEST_ASSERT_TRUE(nts->is_armed());

    // capture temperature requests
    MockTemperatureControl mtc("temperature control", "hotend");
    // capture any call to the switch to turn it on or off
    MockSwitch ms("switch", "fan");
    switch_state= false;
    switch_set_hit= false;
    switch_get_hit= false;
    hitcnt= 0;

    // set the first low temperature
    set_temp(25);

    TEST_ASSERT_TRUE(hitcnt > 0);
    TEST_ASSERT_TRUE(switch_get_hit);
    TEST_ASSERT_FALSE(switch_set_hit);
    TEST_ASSERT_FALSE(switch_state);

    // increase temp low -> high
    set_temp(60);

    // make sure switch was set
    TEST_ASSERT_TRUE(switch_set_hit);

    // and make sure it was turned on
    TEST_ASSERT_TRUE(switch_state);

    // now make sure it turns off when temp drops
    set_temp(25);

    // and make sure it was turned off
    TEST_ASSERT_FALSE(switch_state);
}

REGISTER_TEST(TemperatureSwitch,edge_high_low)
{
    Dispatcher *dispatcher= new Dispatcher;
    dispatcher->clear_handlers();

    // load config with required settings for this test
    std::stringstream ss(edge_low_config);
    ConfigReader cr(ss);
    TEST_ASSERT_TRUE(TemperatureSwitch::load(cr));
    Module *m= Module::lookup("temperature switch", "psu_off");
    TEST_ASSERT_NOT_NULL(m);

    // so it gets destroyed when we go out of scope
    std::unique_ptr<TemperatureSwitch> nts(static_cast<TemperatureSwitch*>(m));
    TEST_ASSERT_NOT_NULL(nts.get());
    TEST_ASSERT_FALSE(nts->is_armed());


    // capture temperature requests
    MockTemperatureControl mtc("temperature control", "hotend");
    // capture any call to the switch to turn it on or off
    MockSwitch ms("switch", "fan");

    // set initial temp low
    set_temp(25);

    // switch is on
    switch_state= true;

    // increase temp low -> high
    set_temp(60);

    // make sure it was not turned off
    TEST_ASSERT_TRUE(switch_state);

    // drop temp
    set_temp(30);

    // and make sure it was still on
    TEST_ASSERT_TRUE(switch_state);

    // now arm it
    std::ostringstream oss;
    OutputStream os(&oss);
    dispatcher->dispatch(os, 'M', 1100, 'S', 1.0F, 0);
    printf(oss.str().c_str());

    TEST_ASSERT_TRUE(nts->is_armed());

    // increase temp low -> high
    set_temp(60);

    // make sure it was not turned off
    TEST_ASSERT_TRUE(switch_state);

    // drop temp
    set_temp(30);

    // and make sure it was turned off
    TEST_ASSERT_FALSE(switch_state);

    // make sure it is not armed anymore
    TEST_ASSERT_FALSE(nts->is_armed());
}

