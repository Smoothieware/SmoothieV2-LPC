#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>

#include "TestRegistry.h"

#include "board.h"

// #include "Adc.h"
// #include "SlowTicker.h"

#define _ADC_CHANNEL ADC_CH1
#define _LPC_ADC_ID LPC_ADC0

REGISTER_TEST(ADCTest, polling)
{
    ADC_CLOCK_SETUP_T ADCSetup;

    Chip_ADC_Init(_LPC_ADC_ID, &ADCSetup);
    Chip_ADC_EnableChannel(_LPC_ADC_ID, _ADC_CHANNEL, ENABLE);

    uint16_t dataADC;

    // Set sample rate to 4.5KHz
    Chip_ADC_SetSampleRate(_LPC_ADC_ID, &ADCSetup, 4500);

    // Select using burst mode
    Chip_ADC_SetBurstCmd(_LPC_ADC_ID, ENABLE);

    float acc= 0;
    uint32_t n= 0;
//    systime_t st = clock_systimer();
    for (int i = 0; i < 10000; ++i) {
        /* Start A/D conversion if not using burst mode */
        //    Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_NOW, ADC_TRIGGERMODE_RISING);

        /* Waiting for A/D conversion complete */
        while (Chip_ADC_ReadStatus(_LPC_ADC_ID, _ADC_CHANNEL, ADC_DR_DONE_STAT) != SET) {}

        /* Read ADC value */
        if(Chip_ADC_ReadValue(_LPC_ADC_ID, _ADC_CHANNEL, &dataADC) == SUCCESS) {
            acc += dataADC;
            ++n;
        } else {
            printf("Failed to read adc\n");
        }
    }
    // systime_t en = clock_systimer();

    printf("average adc= %04X, v= %10.4f\n", (int)(acc/n), 3.3F * (acc/n)/1024.0F);
    // printf("elapsed time: %dus, %10.2f us/sample\n", TICK2USEC(en-st), (float)TICK2USEC(en-st)/n);

    Chip_ADC_SetBurstCmd(_LPC_ADC_ID, DISABLE);
    Chip_ADC_EnableChannel(_LPC_ADC_ID, _ADC_CHANNEL, DISABLE);
    Chip_ADC_DeInit(_LPC_ADC_ID);
}

// REGISTER_TEST(ADCTest, non_burst_interrupt)
// {
//     ADC_CLOCK_SETUP_T ADCSetup;

//     Chip_ADC_Init(_LPC_ADC_ID, &ADCSetup);
//     Chip_ADC_EnableChannel(_LPC_ADC_ID, _ADC_CHANNEL, ENABLE);

//     uint16_t dataADC;

//     // Set sample rate to 4.5KHz
//     Chip_ADC_SetSampleRate(_LPC_ADC_ID, &ADCSetup, 4500);

//     // Select non burst mode
//     Chip_ADC_SetBurstCmd(_LPC_ADC_ID, DISABLE);

//     float acc= 0;
//     uint32_t n= 0;
//     systime_t st = clock_systimer();
//     for (int i = 0; i < 10000; ++i) {
//         /* Start A/D conversion if not using burst mode */
//         Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_NOW, ADC_TRIGGERMODE_RISING);

//         // wait 10ms
//         usleep(10000);

//          Waiting for A/D conversion complete
//         TEST_ASSERT_TRUE(Chip_ADC_ReadStatus(_LPC_ADC_ID, _ADC_CHANNEL, ADC_DR_DONE_STAT) == SET);

//         /* Read ADC value */
//         TEST_ASSERT_TRUE(Chip_ADC_ReadValue(_LPC_ADC_ID, _ADC_CHANNEL, &dataADC) == SUCCESS);

//         acc += dataADC;
//         ++n;

//         if(n == 100) {
//             printf("average adc= %04X, v= %10.4f\n", (int)(acc/n), 3.3F * (acc/n)/1024.0F);
//             acc= 0;
//             n= 0;
//         }
//     }
//     systime_t en = clock_systimer();

//     printf("elapsed time: %dus, %10.2f us/sample\n", TICK2USEC(en-st), (float)TICK2USEC(en-st)/n);

//     Chip_ADC_SetBurstCmd(_LPC_ADC_ID, DISABLE);
//     Chip_ADC_EnableChannel(_LPC_ADC_ID, _ADC_CHANNEL, DISABLE);
//     Chip_ADC_DeInit(_LPC_ADC_ID);

// }



#if 0
REGISTER_TEST(ADCTest, adc_class_interrupts)
{
    // we need to setup and start the slow ticker for Adc
    static SlowTicker *slowticker= new SlowTicker;
    TEST_ASSERT_TRUE(slowticker->start());

    TEST_ASSERT_TRUE(Adc::setup());

    Adc *adc = new Adc;
    TEST_ASSERT_FALSE(adc->connected());
    TEST_ASSERT_FALSE(adc->from_string("nc") == adc);
    TEST_ASSERT_FALSE(adc->connected());
    TEST_ASSERT_TRUE(adc->from_string("ADC0_2") == adc); // ADC0_2/T2
    TEST_ASSERT_TRUE(adc->connected());
    TEST_ASSERT_EQUAL_INT(2, adc->get_channel());
    TEST_ASSERT_TRUE(Adc::start());

    const uint32_t max_adc_value = Adc::get_max_value();
    printf("Max ADC= %d\n", max_adc_value);

    // give it time to accumulate the 32 samples
    usleep(500000);
    // fill up moving average buffer
    adc->read();
    adc->read();
    adc->read();
    adc->read();

    for (int i = 0; i < 10; ++i) {
        uint16_t v= adc->read();
        float volts= 3.3F * (v / (float)max_adc_value);

        printf("adc= %04X, volts= %10.4f\n", v, volts);
        usleep(50000);
    }

    delete adc;

    TEST_ASSERT_TRUE(Adc::stop());
}

REGISTER_TEST(ADCTest, two_adc_channels)
{
    TEST_ASSERT_TRUE(Adc::setup());

    Adc *adc1 = new Adc;
    TEST_ASSERT_TRUE(adc1->from_string("PB.6") == adc1); // ADC0_6
    TEST_ASSERT_EQUAL_INT(6, adc1->get_channel());

    Adc *adc2 = new Adc;
    TEST_ASSERT_TRUE(adc2->from_string("ADC0_1") == adc2); // ADC0_1 / T1
    TEST_ASSERT_EQUAL_INT(1, adc2->get_channel());

    TEST_ASSERT_TRUE(Adc::start());

    const uint32_t max_adc_value = Adc::get_max_value();
    printf("Max ADC= %d\n", max_adc_value);

    // give it time to accumulate the 32 samples
    usleep(500000);
    // fill up moving average buffer
    for (int i = 0; i < 4; ++i) {
        adc1->read();
        adc2->read();
    }

    for (int i = 0; i < 10; ++i) {
        uint16_t v= adc2->read();
        float volts= 3.3F * (v / (float)max_adc_value);
        printf("adc2= %04X, volts= %10.4f\n", v, volts);

        v= adc1->read();
        volts= 3.3F * (v / (float)max_adc_value);
        printf("adc1= %04X, volts= %10.4f\n", v, volts);
        usleep(50000);
    }

    delete adc1;
    delete adc2;

    TEST_ASSERT_TRUE(Adc::stop());
}
#endif
