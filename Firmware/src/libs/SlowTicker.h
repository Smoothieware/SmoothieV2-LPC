#pragma once

#include <vector>
#include <tuple>
#include <functional>

#include "FreeRTOS.h"
#include "timers.h"

class SlowTicker
{
    public:
        static SlowTicker *getInstance() { return instance; }
        SlowTicker();
        virtual ~SlowTicker();

        bool start();
        bool stop();

        // call back frequency in Hz
        int attach(uint32_t frequency, std::function<void(void)> cb);
        void detach(int n);
        void tick();

    private:
        static SlowTicker *instance;

        // set frequency of timer in Hz
        bool set_frequency( int frequency );

        using callback_t = std::tuple<int, uint32_t, std::function<void(void)>>;
        std::vector<callback_t> callbacks;
        uint32_t max_frequency{0};
        uint32_t interval{0}; // period in ms between calls
        TimerHandle_t timer_handle{0};
        bool started{false};
};
