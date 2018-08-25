#pragma once

#include "TempSensor.h"

#include <tuple>

#define QUEUE_LEN 32

class OutputStream;
class Adc;

class Thermistor : public TempSensor
{
    public:
        Thermistor();
        ~Thermistor();

        // TempSensor interface.
        bool configure(ConfigReader& cr, ConfigReader::section_map_t&);
        float get_temperature();
        bool set_optional(const sensor_options_t& options);
        bool get_optional(sensor_options_t& options);
        void get_raw(OutputStream& os);
        static std::tuple<float,float,float> calculate_steinhart_hart_coefficients(float t1, float r1, float t2, float r2, float t3, float r3);
        static void print_predefined_thermistors(OutputStream& os);

    private:
        int new_thermistor_reading();
        float adc_value_to_temperature(uint32_t adc_value);
        bool calc_jk();

        // Thermistor computation settings using beta, not used if using Steinhart-Hart
        float r0;
        float t0;

        // on board resistor settings
        int r1;
        int r2;

        union {
            // this saves memory as we only use either beta or SHH
            struct{
                float beta;
                float j;
                float k;
            };
            struct{
                float c1;
                float c2;
                float c3;
            };
        };

        Adc *thermistor_pin{nullptr};

        float min_temp, max_temp;

        bool use_steinhart_hart;
        uint8_t thermistor_number;
};
