/*
      This file is part of Smoothie (http://smoothieware.org/). The motion control part is heavily based on Grbl (https://github.com/simen/grbl).
      Smoothie is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
      Smoothie is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
      You should have received a copy of the GNU General Public License along with Smoothie. If not, see <http://www.gnu.org/licenses/>.
*/

#include <math.h>
#include <fcntl.h>

#include "max31855.h"
#include "OutputStream.h"
#include "main.h"

#define dev_path_id_key                   "dev_path_id"

Max31855 *Max31855::instances[Max31855::num_dev] = {nullptr};
RingBuffer<float,16> *Max31855::queue[Max31855::num_dev] = {nullptr};
int Max31855::ninstances = 0;
bool Max31855::thread_flag = true;

/* Initialize target instance */
Max31855::Max31855()
{
    this->instance_idx = ninstances++;
    instances[this->instance_idx] = this;
    queue[this->instance_idx] = new RingBuffer<float,16>(); // buffer initialized with 16 float elements
    this->error_flag = false;
}

/* Deinitialize target instance */
Max31855::~Max31855()
{
    // remove target instance from instances array
    instances[this->instance_idx] = nullptr;
    for (int i = this->instance_idx; i < ninstances - 1; ++i) {
        instances[i] = instances[i + 1];
        queue[i] = queue[i + 1];
    }
    --ninstances;
    this->error_flag=false;
}

/* Configure the module using the parameters from the config file */
bool Max31855::configure(ConfigReader& cr, ConfigReader::section_map_t& m)
{
    /* open temperature device file from Nuttx corresponding to the selected device path ID number:
        dev_path_id 0: File /dev/temp0, Channel SSP0, Device 0
        dev_path_id 1: File /dev/temp1, Channel SSP0, Device 1
        dev_path_id 2: File /dev/temp2, Channel SSP1, Device 0
        dev_path_id 3: File /dev/temp3, Channel SSP1, Device 1   */
    std::string dev_path = "/dev/temp";
    this->dev_path_id = cr.get_int(m, dev_path_id_key, 0);
    dev_path += std::to_string(this->dev_path_id);
    this->fd = open(dev_path.c_str(), O_RDONLY);
    // negative handler if can't successfully open, positive if it is open
    if (this->fd < 0) {
        printf("ERROR: Could not open file %s\n", dev_path);
        return false;
    }

    // initialize sum of temp values in buffer for moving average calculation
    this->sum = 0;

    // we have to do this the long way as we want to set the stack size
    if (thread_flag)
    {
        // this flag is necessary since we want to launch this thread only once
        thread_flag = false;
        pthread_attr_t attr;
        struct sched_param sparam;
        int status;

        status = pthread_attr_init(&attr);
        if (status != 0) {
            printf("max31855: pthread_attr_init failed, status=%d\n", status);
        }

        status = pthread_attr_setstacksize(&attr, 2000);
        if (status != 0) {
            printf("max31855: pthread_attr_setstacksize failed, status=%d\n", status);
            return true;
        }

        status = pthread_attr_setschedpolicy(&attr, SCHED_RR);
        if (status != OK) {
            printf("max31855: pthread_attr_setschedpolicy failed, status=%d\n", status);
            return true;
        } else {
            printf("max31855: Set max31855 thread policy to SCHED_RR\n");
        }

        sparam.sched_priority = 90; // set lower than comms threads... 150; // (prio_min + prio_mid) / 2;
        status = pthread_attr_setschedparam(&attr, &sparam);
        if (status != OK) {
            printf("max31855: pthread_attr_setschedparam failed, status=%d\n", status);
            return true;
        } else {
            printf("max31855: Set max31855 thread priority to %d\n", sparam.sched_priority);
        }

        status = pthread_create(&temp_thread_p, &attr, temp_thread, NULL);
        if (status != 0) {
            printf("max31855: pthread_create failed, status=%d\n", status);
        }
    }
    return true;
}

/* Thread launcher */
void* Max31855::temp_thread(void*)
{
    // this runs indefinitely, first created instance represents the thread function
    instances[0]->temperature_thread();
    return nullptr;
}

/* Thread for obtaining readings from all devices */
void Max31855::temperature_thread()
{
    // called in thread context
    int i;
    uint16_t data;
    while (true)
    {
        usleep(100000); // sleep thread during 100 ms
        for (i = 0; i < ninstances; i++) {
            // obtain temp value from SPI
            int ret = read(instances[i]->fd, &data, 2);

            // process temp
            if (ret == -1) {
                // error
                // if enabled, debug error messages from Nuttx are provided at this point
                instances[i]->error_flag = true;
            } else {
                if (queue[i]->full()) {
                    // when buffer is full, we remove the oldest element from it
                    instances[i]->sum -= queue[i]->pop_front();
                }
                float temperature = (data & 0x1FFF) / 4.f;
                // get new element into the buffer
                queue[i]->push_back(temperature);
                instances[i]->sum += temperature;
            }
        }
    }
}

/* Output average temperature value of the sensor */
float Max31855::get_temperature()
{
    // called in ISR context
    if (this->error_flag) {
        // return infinity (error)
        return std::numeric_limits<float>::infinity();
    } else {
        if (queue[this->instance_idx]->empty()) {
            // this buffer does not contain any temp values, return infinity
            return std::numeric_limits<float>::infinity();
        } else {
            // return an average of the last readings
            return this->sum / queue[this->instance_idx]->get_size();
        }
    }
}
