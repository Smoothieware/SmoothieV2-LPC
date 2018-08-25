#pragma once

#include <vector>
#include <tuple>
#include <functional>

class SlowTicker
{
    public:
        static SlowTicker *getInstance() { return instance; }
        SlowTicker();
        ~SlowTicker() {};

        bool start();
        bool stop();

        int attach(uint32_t frequency, std::function<void(void)> cb);
        void detach(int n);
        void tick();

    private:
        static SlowTicker *instance;

        bool set_frequency( int frequency );

        using callback_t = std::tuple<int, uint32_t, std::function<void(void)>>;
        std::vector<callback_t> callbacks;
        uint32_t max_frequency{0};
        uint32_t interval{0};

        int fd{-1}; // timer device
        bool started{false};
};
