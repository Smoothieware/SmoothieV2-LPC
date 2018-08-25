/*
      this file is part of smoothie (http://smoothieware.org/). the motion control part is heavily based on grbl (https://github.com/simen/grbl).
      smoothie is free software: you can redistribute it and/or modify it under the terms of the gnu general public license as published by the free software foundation, either version 3 of the license, or (at your option) any later version.
      smoothie is distributed in the hope that it will be useful, but without any warranty; without even the implied warranty of merchantability or fitness for a particular purpose. see the gnu general public license for more details.
      you should have received a copy of the gnu general public license along with smoothie. if not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#ifndef max31855_h
#define max31855_h

#include "TempSensor.h"

#include "Pin.h"
#include "RingBuffer.h"

class Max31855 : public TempSensor
{
    public:

        /**
         * @brief   Initialize target instance
         * @param   Nothing
         * @return  Nothing
         * @note    The device is initialized with respective buffer
         */
        Max31855();

        /**
         * @brief   Deinitialize target instance
         * @param   Nothing
         * @return  Nothing
         * @note    The device is deinitialized with respective buffer
         */
        ~Max31855();

        /**
         * @brief   Configure the module using the parameters from the config file
         * @param   cr          : target config file object
         * @param   m           : target section map of the config file
         * @return  false       : the module failed to be configured
         * @return  true        : the module is configured and loaded successfully
         */
        bool configure(ConfigReader& cr, ConfigReader::section_map_t& m);

        /**
         * @brief   Output average temperature value of the sensor
         * @param   Nothing
         * @return  Infinity if a single bad reading occurs
         * @return  Average of the last acquired temperature values
         */
        float get_temperature();

        /**
         * @brief   Thread launcher
         * @param   Nothing
         * @return  Nothing
         * @note    This function runs only once since it works for all devices simultaneously
         */
        static void* temp_thread(void *);

        /**
         * @brief   Thread for obtaining readings from all devices
         * @param   Nothing
         * @return  Undeclared pointer
         * @note    This thread runs indefinitely and sleeps for 100 ms
         */
        void temperature_thread();
    private:
        static const int num_dev = 2; // define maximum number of max31855 sensors which may be configured
        static Max31855* instances[num_dev]; // array of device instances
        static int ninstances; // number of configured devices
        static RingBuffer<float,16> *queue[num_dev]; // buffer to store last acquired temperature values
        static bool thread_flag; // flag for launching thread only once
        pthread_t temp_thread_p;
        int instance_idx;
        int dev_path_id;
        float sum; // accumulates last acquired temperature values for moving average calculation
        struct { bool error_flag; }; // when true, a bad reading is detected and the system will halt
        int fd; // handler for opening the reading file corresponding to the selected SSP channel
};
#endif
