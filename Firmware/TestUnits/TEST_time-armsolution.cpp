#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include <cmath>

#include "TestRegistry.h"

#include "LinearDeltaSolution.h"
#include "ActuatorCoordinates.h"

#include "ConfigReader.h"

#include "board.h"

using systime_t= uint32_t;
#define clock_systimer() ((systime_t)Chip_RIT_GetCounter(LPC_RITIMER))
#define TICK2USEC(x) ((systime_t)(((uint64_t)(x)*1000000)/timerFreq))

static std::string str("[linear delta]\narm_length = 367.3\narm_radius = 199.6014\n");

REGISTER_TEST(ArmSolution, delta_ik)
{
    /* Get RIT timer peripheral clock rate */
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_RITIMER);

    std::stringstream ss1(str);
    ConfigReader cr(ss1);

    float millimeters[3]= {100.0, 200.0, 300.0};
    ActuatorCoordinates ac;
    BaseSolution* k= new LinearDeltaSolution(cr);

    uint32_t n= 100000;
    systime_t st = clock_systimer();

    for(uint32_t i=0;i<n;i++) k->cartesian_to_actuator( millimeters, ac);

    systime_t en = clock_systimer();
    printf("elapsed time %lu us over %lu iterations %1.4f us per iteration\n", TICK2USEC(en-st), n, TICK2USEC(en-st)/(float)n);

    delete k;

    TEST_PASS();
}

REGISTER_TEST(ArmSolution, delta_ik_vs_fk)
{
    std::stringstream ss1(str);
    ConfigReader cr(ss1);

    float millimeters[3]= {0.0, 0.0, 100.0};
    ActuatorCoordinates ac;
    BaseSolution* k= new LinearDeltaSolution(cr);

    for(float x=0.0F;x<100.0F;x+=0.1F) {
        millimeters[0]= x;
        k->cartesian_to_actuator( millimeters, ac);
        float mm[3];
        k->actuator_to_cartesian(ac, mm);
        // printf("A: X%f, Y%f, Z%f - ", ac[0], ac[1], ac[2]);
        // printf("M: X%f, Y%f, Z%f\n", mm[0], mm[1], mm[2]);
        TEST_ASSERT_FLOAT_WITHIN(0.005F, millimeters[0], mm[0]);
        TEST_ASSERT_FLOAT_WITHIN(0.005F, millimeters[1], mm[1]);
        TEST_ASSERT_FLOAT_WITHIN(0.005F, millimeters[2], mm[2]);
    }

    delete k;
}
