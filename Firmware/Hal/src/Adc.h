#pragma once

#include <stdint.h>
#include <cmath>
#include <set>

class Pin;

// define how many bits of extra resolution required
// 2 bits means the 12bit ADC is 14 bits of resolution
#define OVERSAMPLE 2

class Adc
{
public:
    static Adc *getInstance(int n) { return instances[n]; }

    Adc();
    virtual ~Adc();
    static bool setup();
    static bool start();
    static bool stop();
    static void on_tick(void);

    // specific to each instance
    Adc* from_string(const char *name);
    uint32_t read();
    float read_voltage();
    int get_channel() const { return channel; }
    bool connected() const { return enabled; }
    uint32_t get_errors() const { return not_ready_error; }
    bool is_created() const { return instance_idx >= 0; }

    // return the maximum ADC value, base is 10bits 1024.
#ifdef OVERSAMPLE
    static int get_max_value() { return 1024 << OVERSAMPLE;}
#else
    static int get_max_value() { return 1024;}
#endif

    static void sample_isr();

private:
    static const int num_channels= 8;
    static Adc* instances[num_channels];
    static int ninstances;
    static std::set<int> allocated_channels;

#ifdef OVERSAMPLE
    // we need 4^n sample to oversample and we get double that to filter out spikes
    static const int num_samples= 32; // powf(4, OVERSAMPLE)*2; // TODO FIXME
#else
    static const int num_samples= 8;
#endif

    // instance specific fields
    void new_sample(uint32_t value);

    bool enabled{false};
    static bool running;
    int channel;
    int instance_idx{-1};
    uint32_t not_ready_error{0};
    static int slowticker_n;
    // buffer storing the last num_samples readings for each channel instance
    uint16_t sample_buffer[num_samples];
    uint16_t ave_buf[4]{0};
};

