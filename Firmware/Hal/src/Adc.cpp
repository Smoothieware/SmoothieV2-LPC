// Only thermistor inputs are supersampled
// TODO allow non thermistor instances to be created

#include "Adc.h"
#include "SlowTicker.h"

#include <string>
#include <cctype>
#include <algorithm>

#include "board.h"

#include "FreeRTOS.h"
#include "task.h"

#undef __GNU_VISIBLE
#define __GNU_VISIBLE 1 // for strcasestr
#include <string.h>

// interrupts seem to not work very well
#define NO_ADC_INTERRUPTS

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
std::set<int> Adc::allocated_channels;
int Adc::ninstances = 0;
bool Adc::running = false;
int Adc::slowticker_n = -1;

static ADC_CLOCK_SETUP_T ADCSetup;

// NOTE we cannot create these once ADC is running
Adc::Adc()
{
    if(ninstances < Adc::num_channels && !running) {
        instance_idx = ninstances++;
        instances[instance_idx] = this;
    }
    channel = -1;
    enabled = false;
}

Adc::~Adc()
{
    if(channel == -1 || instance_idx < 0) return;
    allocated_channels.erase(channel);
    Chip_ADC_Int_SetChannelCmd(_LPC_ADC_ID, CHANNEL_LUT[channel], DISABLE);
    Chip_ADC_EnableChannel(_LPC_ADC_ID, CHANNEL_LUT[channel], DISABLE);

    channel = -1;
    enabled = false;
    // remove from instances array
    taskENTER_CRITICAL();
    instances[instance_idx] = nullptr;
    for (int i = instance_idx; i < ninstances - 1; ++i) {
        instances[i] = instances[i + 1];
    }
    --ninstances;
    taskEXIT_CRITICAL();
    if(ninstances == 0) {
        if(slowticker_n >= 0) {
            SlowTicker::getInstance()->detach(slowticker_n);
            slowticker_n = -1;
        }
    }
}

bool Adc::setup()
{
    // ADC Init
    Chip_ADC_Init(_LPC_ADC_ID, &ADCSetup);

    // ADC sample rate need to be fast enough to be able to read the enabled channels within the thermistor poll time
    // even though there maybe 32 samples we only need one new one within the polling time
    // Set sample rate to 70KHz (That is as slow as it will go)
    // As this is a lot of IRQ overhead we can't use interrupts in burst mode
    // so we need to sample it from a slow timer instead
    Chip_ADC_SetSampleRate(_LPC_ADC_ID, &ADCSetup, ADC_MAX_SAMPLE_RATE);

    // NOTE we would like to just trigger a sample after we sample, but that seemed to only work for the first channel
    // the second channel was never ready

#ifndef NO_ADC_INTERRUPTS
    Chip_ADC_SetBurstCmd(_LPC_ADC_ID, DISABLE);
#else
    // We use burst mode so samples are always ready when we sample the ADC values
    Chip_ADC_SetBurstCmd(_LPC_ADC_ID, ENABLE);
#endif

    // init instances array
    for (int i = 0; i < num_channels; ++i) {
        instances[i] = nullptr;
    }
    return true;
}

bool Adc::start()
{
#ifndef NO_ADC_INTERRUPTS
    // setup to interrupt

    NVIC_SetPriority(ADC0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(ADC0_IRQn);
    NVIC_ClearPendingIRQ(ADC0_IRQn);
    // kick start it
    Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
    running = true;
    // start conversion every 10ms
    slowticker_n = SlowTicker::getInstance()->attach(100, Adc::on_tick);
#else
    // No need to start it as we are in BURST mode
    //Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
    running = true;
    // read conversion every 10ms
    slowticker_n = SlowTicker::getInstance()->attach(100, Adc::on_tick);
#endif
    return true;
}

void Adc::on_tick()
{
#ifndef NO_ADC_INTERRUPTS
    if(running) {
        Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
    }
#else
    // we need to run the sampling irq
    if(running) {
        sample_isr();
        // No need to start it as we are in burst mode so it should always be ready
        //Chip_ADC_SetStartMode(_LPC_ADC_ID, ADC_START_NOW, ADC_TRIGGERMODE_RISING);
    }
#endif
}

bool Adc::stop()
{
    running = false;
#ifndef NO_ADC_INTERRUPTS
    NVIC_DisableIRQ(ADC0_IRQn);
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
    if(instance_idx < 0) return nullptr; // too many instances

    const char *p = strcasestr(name, "adc");
    if (p != nullptr) {
        // ADC specific pin
        p += 3;
        if(*p++ != '0') return nullptr; // must be ADC0
        if(*p++ != '_') return nullptr; // must be _
        channel = strtol(p, nullptr, 10);
        if(channel < 0 || channel >= num_channels) return nullptr;

        // make sure it is not already in use
        if(allocated_channels.count(channel) != 0) {
            // already allocated
            channel = -1;
            return nullptr;
        }

    } else if(tolower(name[0]) == 'p') {
        // pin specification
        std::string str(name);
        uint16_t port = strtol(str.substr(1).c_str(), nullptr, 16);
        size_t pos = str.find_first_of("._", 1);
        if(pos == std::string::npos) return nullptr;
        uint16_t pin = strtol(str.substr(pos + 1).c_str(), nullptr, 10);

        /* now map to an adc channel (ADC0 only)
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

        // make sure it is not already in use
        if(allocated_channels.count(channel) != 0) {
            // already allocated
            channel = -1;
            return nullptr;
        }

        // now need to set to input, disable receiver with EZI bit, disable pullup EPUN=1 and disable pulldown EPD=0
        uint16_t modefunc = 1 << 4; // disable pullup,
        Chip_SCU_PinMuxSet(port, pin, modefunc);
        Chip_SCU_ADC_Channel_Config(0, channel);

    } else {
        return nullptr;
    }

    allocated_channels.insert(channel);

    memset(sample_buffer, 0, sizeof(sample_buffer));
    memset(ave_buf, 0, sizeof(ave_buf));
    Chip_ADC_EnableChannel(_LPC_ADC_ID, CHANNEL_LUT[channel], ENABLE);
#ifndef NO_ADC_INTERRUPTS
    Chip_ADC_Int_SetChannelCmd(_LPC_ADC_ID, CHANNEL_LUT[channel], ENABLE);
#endif
    enabled = true;

    return this;
}

#ifndef NO_ADC_INTERRUPTS
//  isr call
extern "C" _ramfunc_ void ADC0_IRQHandler(void)
{
    NVIC_DisableIRQ(ADC0_IRQn);
    Adc::sample_isr();
    NVIC_EnableIRQ(ADC0_IRQn);
}
#endif

//_ramfunc_
// As this calls non ram based funcs there is no point in it being in ram
// in interrupt mode this will be called once for each active channel
void Adc::sample_isr()
{
    for (int i = 0; i < ninstances; ++i) {
        Adc *adc = Adc::getInstance(i);
        if(adc == nullptr || !adc->enabled) continue; // not setup

        int ch = adc->channel;
        if(ch < 0) continue; // no channel assigned
        uint16_t dataADC = 0;
        // NOTE these are not in RAM
        if(Chip_ADC_ReadStatus(_LPC_ADC_ID, CHANNEL_LUT[ch], ADC_DR_DONE_STAT) == SET && Chip_ADC_ReadValue(_LPC_ADC_ID, CHANNEL_LUT[ch], &dataADC) == SUCCESS) {
            adc->new_sample(dataADC);
        } else {
            // NOTE in interrupt mode this is not meaningful as we get an interrupt when each channel is ready
            adc->not_ready_error++;
        }
    }
}

// Keeps the last 32 values for each channel
// This is called in an ISR, so sample_buffer needs to be accessed atomically
//_ramfunc_
void Adc::new_sample(uint32_t value)
{
    // Shuffle down and add new value to the end
    // FIXME memmove appears to not be inline, so in an interrupt in SPIFI it is too slow
    // memmove(&sample_buffer[0], &sample_buffer[1], sizeof(sample_buffer) - sizeof(sample_buffer[0]));
    for (int i = 1; i < num_samples; ++i) {
        sample_buffer[i - 1] = sample_buffer[i];
    }
    sample_buffer[num_samples - 1] = value; // the 10 bit ADC reading
}

//#define USE_MEDIAN_FILTER
// gets called 20 times a second from a timer
uint32_t Adc::read()
{
    uint16_t median_buffer[num_samples];

    // needs atomic access TODO maybe be able to use std::atomic here or some lockless mutex
    taskENTER_CRITICAL();
    memcpy(median_buffer, sample_buffer, sizeof(median_buffer));
    taskEXIT_CRITICAL();

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

static void split(uint16_t data[], unsigned int n, uint16_t x, unsigned int& i, unsigned int& j)
{
    do {
        while (data[i] < x) i++;
        while (x < data[j]) j--;

        if (i <= j) {
            uint16_t ii = data[i];
            data[i] = data[j];
            data[j] = ii;
            i++; j--;
        }
    } while (i <= j);
}

// C.A.R. Hoare's Quick Median
static unsigned int quick_median(uint16_t data[], unsigned int n)
{
    unsigned int l = 0, r = n - 1, k = n / 2;
    while (l < r) {
        uint16_t x = data[k];
        unsigned int i = l, j = r;
        split(data, n, x, i, j);
        if (j < k) l = i;
        if (k < i) r = j;
    }
    return k;
}

float Adc::read_voltage()
{
    // just return the voltage on the pin
    uint16_t median_buffer[num_samples];

    // needs atomic access TODO maybe be able to use std::atomic here or some lockless mutex
    taskENTER_CRITICAL();
    memcpy(median_buffer, sample_buffer, sizeof(median_buffer));
    taskEXIT_CRITICAL();

    // take the median value
    unsigned int i= quick_median(median_buffer, num_samples);
    uint16_t adc= median_buffer[i];

    float v= 3.3F * (adc / 1024.0F); // 10 bit adc values
    return v;
}
