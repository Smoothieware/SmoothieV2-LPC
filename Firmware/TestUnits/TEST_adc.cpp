#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>

#include "TestRegistry.h"

#include "board.h"

#include "Adc.h"
#include "SlowTicker.h"

#include "FreeRTOS.h"
#include "task.h"

using systime_t= uint32_t;
#define clock_systimer() ((systime_t)Chip_RIT_GetCounter(LPC_RITIMER))
#define TICK2USEC(x) ((systime_t)(((uint64_t)(x)*1000000)/timerFreq))

#ifdef BOARD_BAMBINO
#define _ADC_CHANNEL ADC_CH1
#else
//#define _ADC_CHANNEL ADC_CH6 // board temp
#define _ADC_CHANNEL ADC_CH7 // Vbb
#endif

#define _LPC_ADC_ID LPC_ADC0
#define _LPC_ADC_IRQ ADC0_IRQn

REGISTER_TEST(ADCTest, raw_polling)
{
    /* Get RIT timer peripheral clock rate */
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_RITIMER);
    uint32_t t = Chip_Clock_GetRate(CLK_APB3_ADC0);
    printf("ADC0 clk= %lu\n", t);

    ADC_CLOCK_SETUP_T ADCSetup;

    Chip_ADC_Init(_LPC_ADC_ID, &ADCSetup);
    Chip_ADC_EnableChannel(_LPC_ADC_ID, _ADC_CHANNEL, ENABLE);

    uint16_t dataADC;

    // Set sample rate to 400KHz
    ADCSetup.burstMode= true;;
    Chip_ADC_SetSampleRate(_LPC_ADC_ID, &ADCSetup, 100000); // ADC_MAX_SAMPLE_RATE);

    // Select using burst mode
    Chip_ADC_SetBurstCmd(_LPC_ADC_ID, ENABLE);

    float acc= 0;
    uint32_t n= 0;
    systime_t st = clock_systimer();
    for (int i = 0; i < 10000; ++i) {
        /* Start A/D conversion if not using burst mode */
        //    Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_NOW, ADC_TRIGGERMODE_RISING);

        /* Waiting for A/D conversion complete */
        while (Chip_ADC_ReadStatus(_LPC_ADC_ID, _ADC_CHANNEL, ADC_DR_DONE_STAT) != SET) {
            //vTaskDelay(pdMS_TO_TICKS(1));
        }

        /* Read ADC value */
        if(Chip_ADC_ReadValue(_LPC_ADC_ID, _ADC_CHANNEL, &dataADC) == SUCCESS) {
            acc += dataADC;
            ++n;
        } else {
            printf("Failed to read adc\n");
        }
    }
    systime_t en = clock_systimer();

    printf("channel: %d, average adc= %04X, v= %10.4f\n", _ADC_CHANNEL, (int)(acc/n), 3.3F * (acc/n)/1024.0F);
    printf("elapsed time: %lu us, %10.2f us/sample\n", TICK2USEC(en-st), (float)TICK2USEC(en-st)/n);


    // calculate temp of board thermistor
    float adc_value= acc/n;
    float beta= 4334;
    float r0= 100000;
    float r2= 4700;
    float t0= 25;
    float r = r2 / (((float)1024.0F / adc_value) - 1.0F);
    float j = (1.0F / beta);
    float k = (1.0F / (t0 + 273.15F));
    float temp = (1.0F / (k + (j * logf(r / r0)))) - 273.15F;
    printf("Temp= %f °C\n", temp);

    Chip_ADC_SetBurstCmd(_LPC_ADC_ID, DISABLE);
    Chip_ADC_EnableChannel(_LPC_ADC_ID, _ADC_CHANNEL, DISABLE);
    Chip_ADC_DeInit(_LPC_ADC_ID);
}

#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))
static uint16_t intdataADC;
volatile int ADC_Interrupt_Done_Flag= 0;
extern "C" _ramfunc_ void ADC0_IRQHandler(void)
{
        TEST_ASSERT_TRUE(Chip_ADC_ReadValue(_LPC_ADC_ID, _ADC_CHANNEL, &intdataADC) == SUCCESS);
        ADC_Interrupt_Done_Flag += 1;
}

REGISTER_TEST(ADCTest, non_burst_interrupt)
{
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_RITIMER);
    ADC_CLOCK_SETUP_T ADCSetup;

    Chip_ADC_Init(_LPC_ADC_ID, &ADCSetup);
    Chip_ADC_EnableChannel(_LPC_ADC_ID, _ADC_CHANNEL, ENABLE);

    Chip_ADC_Int_SetChannelCmd(_LPC_ADC_ID, _ADC_CHANNEL, ENABLE);

    // Set sample rate to 400KHz
    Chip_ADC_SetSampleRate(_LPC_ADC_ID, &ADCSetup, ADC_MAX_SAMPLE_RATE);

    // Select non burst mode
    Chip_ADC_SetBurstCmd(_LPC_ADC_ID, DISABLE);

    NVIC_EnableIRQ(_LPC_ADC_IRQ);

    float acc= 0;
    uint32_t n= 0;
    systime_t st = clock_systimer();
    for (int i = 0; i < 10000; ++i) {
        /* Start A/D conversion */
        ADC_Interrupt_Done_Flag= 0;
        Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_NOW, ADC_TRIGGERMODE_RISING);

        // wait for IRQ
        while(ADC_Interrupt_Done_Flag == 0) {
            // wait for it
        }
        acc += intdataADC;
        ++n;
        TEST_ASSERT_TRUE(ADC_Interrupt_Done_Flag == 1);
    }
    systime_t en = clock_systimer();

    printf("channel: %d, average adc= %04X, v= %10.4f\n", _ADC_CHANNEL, (int)(acc/n), 3.3F * (acc/n)/1024.0F);
    printf("elapsed time: %lu us, %10.2f us/sample\n", TICK2USEC(en-st), (float)TICK2USEC(en-st)/n);

    /* Disable ADC interrupt */
    NVIC_DisableIRQ(_LPC_ADC_IRQ);
    Chip_ADC_Int_SetChannelCmd(_LPC_ADC_ID, _ADC_CHANNEL, DISABLE);

    Chip_ADC_EnableChannel(_LPC_ADC_ID, _ADC_CHANNEL, DISABLE);
    Chip_ADC_DeInit(_LPC_ADC_ID);
}

REGISTER_TEST(ADCTest, adc_class_interrupts)
{
    TEST_ASSERT_TRUE(Adc::setup());

    Adc *adc = new Adc;
    TEST_ASSERT_TRUE(adc->is_created());
    TEST_ASSERT_FALSE(adc->connected());
    TEST_ASSERT_FALSE(adc->from_string("nc") == adc);
    TEST_ASSERT_FALSE(adc->connected());
    TEST_ASSERT_TRUE(adc->from_string("ADC0_1") == adc); // ADC0_1/T1
    TEST_ASSERT_TRUE(adc->connected());
    TEST_ASSERT_EQUAL_INT(1, adc->get_channel());

    // check it won't let us create a duplicate
    Adc *dummy= new Adc();
    TEST_ASSERT_TRUE(dummy->from_string("ADC0_1") == nullptr);
    delete dummy;

    TEST_ASSERT_TRUE(Adc::start());

    const uint32_t max_adc_value = Adc::get_max_value();
    printf("Max ADC= %lu\n", max_adc_value);

    // give it time to accumulate the 32 samples
    vTaskDelay(pdMS_TO_TICKS(32*10*2 + 100));
    // fill up moving average buffer
    adc->read();
    adc->read();
    adc->read();
    adc->read();

    for (int i = 0; i < 10; ++i) {
        uint16_t v= adc->read();
        float volts= 3.3F * (v / (float)max_adc_value);

        printf("adc= %04X, volts= %10.4f\n", v, volts);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    printf("read_voltage() = %10.4f\n", adc->read_voltage());

    delete adc;
    TEST_ASSERT_TRUE(Adc::stop());

    // should be able to create it now
    dummy= new Adc();
    TEST_ASSERT_TRUE(dummy->from_string("ADC0_1") == dummy);
    delete dummy;
}

REGISTER_TEST(ADCTest, two_adc_channels)
{
    TEST_ASSERT_TRUE(Adc::setup());

    Adc *adc1 = new Adc;
    TEST_ASSERT_TRUE(adc1->is_created());
#ifdef BOARD_BAMBINO
    TEST_ASSERT_TRUE(adc1->from_string("PB.6") == adc1); // ADC0_6
    TEST_ASSERT_EQUAL_INT(6, adc1->get_channel());
#else
    TEST_ASSERT_TRUE(adc1->from_string("ADC0_1") == adc1); // ADC0_1 / T1
    TEST_ASSERT_EQUAL_INT(1, adc1->get_channel());
#endif

    Adc *adc2 = new Adc;
    TEST_ASSERT_TRUE(adc2->is_created());
#ifdef BOARD_BAMBINO
    TEST_ASSERT_TRUE(adc2->from_string("ADC0_1") == adc2);
    TEST_ASSERT_EQUAL_INT(1, adc2->get_channel());
#else
    TEST_ASSERT_TRUE(adc2->from_string("ADC0_2") == adc2); // ADC0_2 / T2
    TEST_ASSERT_EQUAL_INT(2, adc2->get_channel());
#endif
    TEST_ASSERT_TRUE(Adc::start());

    const uint32_t max_adc_value = Adc::get_max_value();
    printf("Max ADC= %lu\n", max_adc_value);

    // give it time to accumulate the 32 samples
    vTaskDelay(pdMS_TO_TICKS(32*10*2 + 100));

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
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    TEST_ASSERT_TRUE(adc1->get_errors() == 0);
    TEST_ASSERT_TRUE(adc2->get_errors() == 0);

    delete adc1;
    delete adc2;

    TEST_ASSERT_TRUE(Adc::stop());
}
