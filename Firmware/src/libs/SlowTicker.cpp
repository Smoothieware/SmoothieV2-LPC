#include "SlowTicker.h"


// timers are specified in milliseconds
#define BASE_FREQUENCY 1000

SlowTicker *SlowTicker::instance;

// This module uses a Timer to periodically call registered callbacks
// Modules register with a function ( callback ) and a frequency, and we then call that function at the given frequency.
// NOTE we could use TMR1 instead of s/w timer

SlowTicker::SlowTicker()
{
    instance= this;
}

SlowTicker::~SlowTicker()
{
    instance= nullptr;
    if(timer_handle != nullptr) {
        xTimerDelete(timer_handle, 1000);
    }
}

static void timer_handler(TimerHandle_t xTimer)
{
    SlowTicker::getInstance()->tick();
}

bool SlowTicker::start()
{
    if(!started) {

        if(timer_handle != nullptr) {
            printf("ERROR: Timer already created\n");
        }

        if(interval == 0) {
            max_frequency= 1; // 1 Hz
            interval = BASE_FREQUENCY/max_frequency; // default to 1HZ, 1000ms period
        }

        timer_handle= xTimerCreate("SlowTickerTimer", pdMS_TO_TICKS(interval), pdTRUE, nullptr, timer_handler);
    }

    // Start the timer
    if( xTimerStart( timer_handle, 1000 ) != pdPASS ) {
        // The timer could not be set into the Active state
        printf("ERROR: Failed to start the timer\n");
        return false;
    }
    started= true;
    return true;
}

bool SlowTicker::stop()
{
    if( xTimerStop(timer_handle, 1000) != pdPASS) {
        printf("ERROR: Failed to stop the timer\n");
        return false;
    }

    return true;
}

int SlowTicker::attach(uint32_t frequency, std::function<void(void)> cb)
{
    uint32_t period = BASE_FREQUENCY / frequency;
    int countdown = period;

    if( frequency > max_frequency ) {
        // reset frequency to a higher value
        if(!set_frequency(frequency)) {
            printf("WARNING: SlowTicker cannot be set to > %dHz\n", BASE_FREQUENCY);
            return -1;
        }
        max_frequency = frequency;
    }

    if(started) stop();
    callbacks.push_back(std::make_tuple(countdown, period, cb));
    if(started) start();

    // return the index it is in
    return callbacks.size()-1;
}

void SlowTicker::detach(int n)
{
    // TODO need to remove it but that would change all the indexes
    // For now we just zero the callback
    std::get<2>(callbacks[n])= nullptr;
}

// Set the base frequency we use for all sub-frequencies
// NOTE this is a slow ticker so ticks faster than 1000Hz are not allowed
bool SlowTicker::set_frequency( int frequency )
{
    if(frequency > BASE_FREQUENCY) return false;
    this->interval = BASE_FREQUENCY / frequency; // millisecond period
    if(started) {
        stop(); // must stop timer first
        // change frequency of timer callback
        if( xTimerChangePeriod( timer_handle, pdMS_TO_TICKS(interval), 1000 ) != pdPASS ) {
            printf("ERROR: Failed to change timer period\n");
            return false;
        }
        start(); // the restart it
    }
    return true;
}

// This is an ISR (or not actually, but must not block)
void SlowTicker::tick()
{
    // Call all callbacks that need to be called
    for(auto& i : callbacks) {
        int& countdown= std::get<0>(i);
        countdown -= this->interval;
        if (countdown < 0) {
            countdown += std::get<1>(i);
            auto& fnc= std::get<2>(i); // get the callback
            if(fnc) {
                fnc();
            }
        }
    }
}
