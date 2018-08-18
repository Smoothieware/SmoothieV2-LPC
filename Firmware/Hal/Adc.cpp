// Uses the lpcopen ADC driver instead of the NuttX one whioch doessn't really do what we want.

#include "Adc.h"
#include "SlowTicker.h"

#include <string>
#include <cstring>
#include <cctype>
#include <algorithm>

#include "board.h"

#define _LPC_ADC_ID LPC_ADC0
const ADC_CHANNEL_T CHANNEL_LUT[] = {
    ADC_CH0,                /**< ADC channel 0 */
    ADC_CH1,                /**< ADC channel 1 */
    ADC_CH2,                /**< ADC channel 2 */
    ADC_CH3,                /**< ADC channel 3 */
    ADC_CH4,                /**< ADC channel 4 */
    ADC_CH5,                /**< ADC channel 5 */
    ADC_CH6,                /**< ADC channel 6 */
    ADC_CH7                 /**< ADC channel 7 */
};

Adc *Adc::instances[Adc::num_channels] = {nullptr};
int Adc::ninstances = 0;
bool Adc::running= false;

// TODO move ramfunc define to a utils.h
#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))

static ADC_CLOCK_SETUP_T ADCSetup;

// warning we cannot create these once ADC is running
Adc::Adc()
{
    instance_idx = ninstances++;
    instances[instance_idx] = this;
    channel = -1;
    enabled = false;
}

Adc::~Adc()
{
    Chip_ADC_Int_SetChannelCmd(_LPC_ADC_ID, CHANNEL_LUT[channel], DISABLE);
    Chip_ADC_EnableChannel(_LPC_ADC_ID, CHANNEL_LUT[channel], DISABLE);
    channel = -1;
    enabled = false;
    // remove from instances array
    irqstate_t flags = enter_critical_section();
    instances[instance_idx] = nullptr;
    for (int i = instance_idx; i < ninstances - 1; ++i) {
        instances[i] = instances[i + 1];
    }
    --ninstances;
    leave_critical_section(flags);
}

bool Adc::setup()
{
    // ADC Init
    Chip_ADC_Init(_LPC_ADC_ID, &ADCSetup);

    // ADC sample rate need to be fast enough to be able to read the enabled channels within the thermistor poll time
    // even though there maybe 32 samples we only need one new one within the polling time
    // Set sample rate to 4.5KHz (That is as slow as it will go)
    // As this is a lot of IRQ overhead we can't use interrupts in burst mode
    // so we need to sample it from a slow timer instead
    Chip_ADC_SetSampleRate(_LPC_ADC_ID, &ADCSetup, 4500);

    // We use burst mode so samples are always ready when we sample the ADC values
    // NOTE we would like to just trigger a sample after we sample, but that seemed to only work for the first channel
    // the second channel was never ready
    Chip_ADC_SetBurstCmd(_LPC_ADC_ID, ENABLE);

    // init instances array
    for (int i = 0; i < num_channels; ++i) {
        instances[i] = nullptr;
    }
    return true;
}

// As nuttx broke ADC interrupts we need to work around it
#define NO_ADC_INTERRUPTS
bool Adc::start()
{
#ifndef NO_ADC_INTERRUPTS
    // setup to interrupt, FIXME
    int ret = irq_attach(LPC43M4_IRQ_ADC0, Adc::sample_isr, NULL);
    if (ret == OK) {
        up_enable_irq(LPC43M4_IRQ_ADC0);
        // kick start it
        Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
        running= true;
        // start conversion every 10ms
        SlowTicker::getInstance()->attach(100, Adc::on_tick);
    } else {
        return false;
    }
#else
    // No need to start it as we are in BURST mode
    //Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
    running= true;
    // read conversion every 10ms
    SlowTicker::getInstance()->attach(100, Adc::on_tick);
#endif
    return true;
}

void Adc::on_tick()
{
#ifndef NO_ADC_INTERRUPTS
    // FIXME
    if(running){
        Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
    }
#else
    // we need to run the sampling irq
    if(running) {
        sample_isr(0, 0, 0);
        // No need to start it as we are in burst mode so it should always be ready
        //Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
    }
#endif
}

bool Adc::stop()
{
    running= false;
    #ifndef NO_ADC_INTERRUPTS
    up_disable_irq(LPC43M4_IRQ_ADC0);
    irq_attach(LPC43M4_IRQ_ADC0, nullptr, nullptr);
    #endif
    Chip_ADC_SetBurstCmd(_LPC_ADC_ID, DISABLE);
    Chip_ADC_DeInit(_LPC_ADC_ID);

    return true;
}

// TODO only ADC0_0 to ADC0_7 handled at the moment
// figure out channel from name (ADC0_1, ADC0_4, ...)
// or Pin P4_3
Adc* Adc::from_string(const char *name)
{
    if(enabled) return nullptr; // aready setup

    const char *p = strcasestr(name, "adc");
    if (p != nullptr) {
        // ADC specific pin
        p += 3;
        if(*p++ != '0') return nullptr; // must be ADC0
        if(*p++ != '_') return nullptr; // must be _
        channel = strtol(p, nullptr, 10);
        if(channel < 0 || channel >= num_channels) return nullptr;

    } else if(tolower(name[0]) == 'p') {
        // pin specification
        std::string str(name);
        uint16_t port = strtol(str.substr(1).c_str(), nullptr, 16);
        size_t pos = str.find_first_of("._", 1);
        if(pos == std::string::npos) return nullptr;
        uint16_t pin = strtol(str.substr(pos + 1).c_str(), nullptr, 10);

        /* now map to an adc channel
            P4_3 ADC0_0
            P4_1 ADC0_1
            PF_8 ADC0_2
            P7_5 ADC0_3
            P7_4 ADC0_4
            PF_10 ADC0_5
            PB_6 ADC0_6
        */
        if     (port ==  4 && pin ==  3) channel = 0;
        else if(port ==  4 && pin ==  1) channel = 1;
        else if(port == 15 && pin ==  8) channel = 2;
        else if(port ==  7 && pin ==  5) channel = 3;
        else if(port ==  7 && pin ==  4) channel = 4;
        else if(port == 15 && pin == 10) channel = 5;
        else if(port == 11 && pin ==  6) channel = 6;
        else return nullptr; // not a valid ADC pin

        // now need to set to input, disable receiver with EZI bit, disable pullup EPUN=1 and disable pulldown EPD=0
        uint16_t modefunc = 1 << 4; // disable pullup,
        Chip_SCU_PinMuxSet(port, pin, modefunc);

    } else {
        return nullptr;
    }

    // TODO do not hard code ADC0
    Chip_SCU_ADC_Channel_Config(0, channel);

    memset(sample_buffer, 0, sizeof(sample_buffer));
    memset(ave_buf, 0, sizeof(ave_buf));
    Chip_ADC_EnableChannel(_LPC_ADC_ID, CHANNEL_LUT[channel], ENABLE);
    #ifndef NO_ADC_INTERRUPTS
    Chip_ADC_Int_SetChannelCmd(_LPC_ADC_ID, CHANNEL_LUT[channel], ENABLE);
    #endif
    enabled = true;

    return this;
}

//  isr call
_ramfunc_ int Adc::sample_isr(int irq, void *context, FAR void *arg)
{
    for (int i = 0; i < ninstances; ++i) {
        Adc *adc = Adc::getInstance(i);
        if(adc == nullptr || !adc->enabled) continue; // not setup

        int ch = adc->channel;
        if(ch < 0) continue; // no channel assigned
        uint16_t dataADC = 0;
        if(Chip_ADC_ReadStatus(_LPC_ADC_ID, CHANNEL_LUT[ch], ADC_DR_DONE_STAT) == SET && Chip_ADC_ReadValue(_LPC_ADC_ID, CHANNEL_LUT[ch], &dataADC) == SUCCESS) {
            adc->new_sample(dataADC);
        }else{
            adc->not_ready_error++;
        }
    }

    return OK;
}

// Keeps the last 32 values for each channel
// This is called in an ISR, so sample_buffer needs to be accessed atomically
_ramfunc_ void Adc::new_sample(uint32_t value)
{
    // Shuffle down and add new value to the end
    memmove(&sample_buffer[0], &sample_buffer[1], sizeof(sample_buffer) - sizeof(sample_buffer[0]));
    sample_buffer[num_samples - 1] = value; // the 12 bit ADC reading
}

//#define USE_MEDIAN_FILTER
// Read the filtered value ( burst mode ) on a given pin
uint32_t Adc::read()
{
    uint16_t median_buffer[num_samples];

    // needs atomic access TODO maybe be able to use std::atomic here or some lockless mutex
    irqstate_t flags = enter_critical_section();
    memcpy(median_buffer, sample_buffer, sizeof(median_buffer));
    leave_critical_section(flags);

#ifdef USE_MEDIAN_FILTER
    // returns the median value of the last 8 samples
    return median_buffer[quick_median(median_buffer, num_samples)];

#elif defined(OVERSAMPLE)
    // Oversample to get 2 extra bits of resolution
    // weed out top and bottom worst values then oversample the rest
    std::sort(median_buffer, median_buffer + num_samples);
    uint32_t sum = 0;
    for (int i = num_samples / 4; i < (num_samples - (num_samples / 4)); ++i) {
        sum += median_buffer[i];
    }

    // put into a 4 element moving average and return the average of the last 4 oversampled readings
    // this slows down the rate of change a little bit
    ave_buf[3] = ave_buf[2];
    ave_buf[2] = ave_buf[1];
    ave_buf[1] = ave_buf[0];
    ave_buf[0] = sum >> OVERSAMPLE;
    return roundf((ave_buf[0] + ave_buf[1] + ave_buf[2] + ave_buf[3]) / 4.0F);

#else
    // sort the 8 readings and return the average of the middle 4
    std::sort(median_buffer, median_buffer + num_samples);
    int sum = 0;
    for (int i = num_samples / 4; i < (num_samples - (num_samples / 4)); ++i) {
        sum += median_buffer[i];
    }
    return sum / (num_samples / 2);

#endif
}
