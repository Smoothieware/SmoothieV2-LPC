#include "SlowTicker.h"

#include <fcntl.h>
#include <errno.h>

// timers are specified in microseconds
#define BASE_FREQUENCY 1000000L
#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))

SlowTicker *SlowTicker::instance;


// This module uses a Timer to periodically call registered callbacks
// Modules register with a function ( callback ) and a frequency, and we then call that function at the given frequency.

SlowTicker::SlowTicker()
{
    instance= this;
}

_ramfunc_ static bool timer_handler(uint32_t *next_interval_us)
{
    SlowTicker::getInstance()->tick();
    return true;
}

#define TIMER_DEVNAME "/dev/timer2"
bool SlowTicker::start()
{
    int ret;
    #pragma message "slowticker needs to be implemented using RTOS timers"
    #if 0
    if(!started) {
        struct timer_sethandler_s handler;

        if(fd != -1) {
            printf("ERROR: Slow ticker already started\n");
            return false;
        }

        if(interval == 0) {
            interval = BASE_FREQUENCY / 1; // default to 1HZ
            max_frequency= 1;
        }

        /* Open the timer device */
        fd = open(TIMER_DEVNAME, O_RDONLY);
        if (fd < 0) {
            printf("ERROR: Failed to open %s: %d\n", TIMER_DEVNAME, errno);
            return false;
        }

        ret = ioctl(fd, TCIOC_SETTIMEOUT, interval);
        if (ret < 0) {
            printf("ERROR: Failed to set the slow ticker interval to %d: %d\n", interval, errno);
            close(fd);
            fd= -1;
            return false;
        }

        // Attach the timer handler
        handler.newhandler = timer_handler;
        handler.oldhandler = NULL;

        ret = ioctl(fd, TCIOC_SETHANDLER, (unsigned long)((uintptr_t)&handler));
        if (ret < 0) {
            printf("ERROR: Failed to set the timer handler: %d\n", errno);
            close(fd);
            fd= -1;
            return false;
        }
    }

    // Start the timer
    ret = ioctl(fd, TCIOC_START, 0);
    if (ret < 0) {
        printf("ERROR: Failed to start the timer: %d\n", errno);
        close(fd);
        fd= -1;
        return false;
    }
    started= true;
    #endif
    return true;
}

bool SlowTicker::stop()
{
    #if 0
    int ret = ioctl(fd, TCIOC_STOP, 0);
    if (ret < 0) {
        printf("ERROR: Failed to stop the timer: %d\n", errno);
    }

    return ret >= 0;
    #endif

    return false;
}

int SlowTicker::attach(uint32_t frequency, std::function<void(void)> cb)
{
    uint32_t period = BASE_FREQUENCY / frequency;
    int countdown = period;

    if( frequency > max_frequency ) {
        // reset frequency to a higher value
        if(!set_frequency(frequency)) {
            printf("WARNING: SlowTicker cannot be set to > 100Hz\n");
            return -1;
        }
        max_frequency = frequency;
    }

    // TODO need to make this thread safe
    // for now we just stop the timer
    if(started) stop();
    callbacks.push_back(std::make_tuple(period, countdown, cb));
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
// NOTE this is a slow ticker so ticks faster than 100Hz are not allowed as this uses NUTTX timers running in slow SPIFI
bool SlowTicker::set_frequency( int frequency )
{
    if(frequency > 100) return false;
    this->interval = BASE_FREQUENCY / frequency; // Timer increments in a second
    if(started) {
        stop(); // must stop timer first
        // change frequency of timer callback
        #if 0
        int ret = ioctl(fd, TCIOC_SETTIMEOUT, interval);
        if (ret < 0) {
            printf("ERROR: Failed to reset the slow ticker interval to %d: %d\n", interval, errno);
        }
        #endif
        start(); // the restart it
    }
    return true;
}

// This is an ISR
_ramfunc_ void SlowTicker::tick()
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
