#pragma once

#include "ConfigReader.h"

#include <map>
#include <stdint.h>

class OutputStream;

class TempSensor
{
public:
    virtual ~TempSensor() {}

    // Load config parameters using provided "base" names.
    virtual bool configure(ConfigReader& cr, ConfigReader::section_map_t& m) { return true; }

    // Return temperature in degrees Celsius.
    virtual float get_temperature() { return -1.0F; }

    typedef std::map<char, float> sensor_options_t;
    virtual bool set_optional(const sensor_options_t& options) { return false; }
    virtual bool get_optional(sensor_options_t& options) { return false; }
    virtual void get_raw(OutputStream&) {}
};
