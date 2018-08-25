#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>

#include "TestRegistry.h"

#include "board.h"

// #define SCT_PWM            LPC_SCT

// #define SCT_PWM_PIN_OUT    4        /* COUT4 Generate square wave */
// #define SCT_PWM_PIN_LED    5        /* COUT5 [index 2] Controls LED */

// #define SCT_PWM_OUT        1        /* Index of OUT PWM */
// #define SCT_PWM_LED        2        /* Index of LED PWM */
// #define SCT_PWM_RATE   10000        /* PWM frequency 10 KHz */


// REGISTER_TEST(PWMTest, basic)
// {
//     /* Initialize the SCT as PWM and set frequency */
//     Chip_SCTPWM_Init(SCT_PWM);
//     Chip_SCTPWM_SetRate(SCT_PWM, SCT_PWM_RATE);

//     /* SCT_OUT5 on P2.11 mapped to FUNC1: LED2 */
//     Chip_SCU_PinMuxSet(0x2, 11, (SCU_MODE_INACT | SCU_MODE_FUNC1));
//     /* SCT_OUT4 on P2.12 mapped to FUNC1: Oscilloscope input */
//     Chip_SCU_PinMuxSet(0x2, 12, (SCU_MODE_INACT | SCU_MODE_FUNC1));

//     /* Use SCT0_OUT1 pin */
//     Chip_SCTPWM_SetOutPin(SCT_PWM, SCT_PWM_OUT, SCT_PWM_PIN_OUT);
//     Chip_SCTPWM_SetOutPin(SCT_PWM, SCT_PWM_LED, SCT_PWM_PIN_LED);

//     /* Start with 50% duty cycle */
//     Chip_SCTPWM_SetDutyCycle(SCT_PWM, SCT_PWM_OUT, Chip_SCTPWM_PercentageToTicks(SCT_PWM, 50));
//     Chip_SCTPWM_SetDutyCycle(SCT_PWM, SCT_PWM_LED, Chip_SCTPWM_PercentageToTicks(SCT_PWM, 25));
//     Chip_SCTPWM_Start(SCT_PWM);
// }

#include "Pwm.h"
REGISTER_TEST(PWMTest, from_string)
{
    TEST_ASSERT_TRUE(Pwm::setup(10000));

    Pwm pwm1;
    TEST_ASSERT_FALSE(pwm1.is_valid());
    TEST_ASSERT_FALSE(pwm1.from_string("X1.2"));
    TEST_ASSERT_FALSE(pwm1.is_valid());

    Pwm pwm2;
    TEST_ASSERT_TRUE(pwm2.from_string("P2.12"));
    TEST_ASSERT_TRUE(pwm2.is_valid());

    Pwm pwm3("P2.11");
    TEST_ASSERT_TRUE(pwm3.is_valid());
}


// current = dutycycle * 2.0625
REGISTER_TEST(PWMTest, set_current)
{
    // set X driver to 400mA
    // set Y driver to 1amp
    // set Z driver to 1.5amp
    Pwm pwmx("P7.4"); // X
    TEST_ASSERT_TRUE(pwmx.is_valid());
    // dutycycle= current/2.0625
    float dcp= 0.4F/2.0625F;
    pwmx.set(dcp);

    Pwm pwmy("PB.2"); // Y
    TEST_ASSERT_TRUE(pwmy.is_valid());

    // dutycycle= current/2.0625
    dcp= 1.0F/2.0625F;
    pwmy.set(dcp);

    Pwm pwmz("PB.3"); // Z
    TEST_ASSERT_TRUE(pwmz.is_valid());
    // dutycycle= current/2.0625
    dcp= 1.5F/2.0625F;
    pwmz.set(dcp);
}

