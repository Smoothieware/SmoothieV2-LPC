#pragma once

#include <vector>
#include <tuple>
#include <functional>

class FastTicker
{
    public:
        static FastTicker *getInstance() { return instance; }
        FastTicker();
        virtual ~FastTicker();

        bool start();
        bool stop();

        // call back frequency in Hz
        int attach(uint32_t frequency, std::function<void(void)> cb);
        void detach(int n);
        void tick();
        bool is_running() const { return started; }
        // this depends on FreeRTOS systick rate as SlowTicker cannot go faster than that
        static uint32_t get_min_frequency() { return 1000; }

    private:
        static FastTicker *instance;

        // set frequency of timer in Hz
        bool set_frequency( int frequency );

        using callback_t = std::tuple<int, uint32_t, std::function<void(void)>>;
        std::vector<callback_t> callbacks;
        uint32_t max_frequency{0};
        uint32_t interval{0}; // period in us between calls
        bool started{false};
};
