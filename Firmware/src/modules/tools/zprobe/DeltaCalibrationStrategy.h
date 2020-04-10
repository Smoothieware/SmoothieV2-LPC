#pragma once

#include "ZProbeStrategy.h"

class OutputStream;
class GCode;
class ConfigReader;

class DeltaCalibrationStrategy : public ZProbeStrategy
{
public:
    DeltaCalibrationStrategy(ZProbe *zprb) : ZProbeStrategy(zprb){};
    ~DeltaCalibrationStrategy(){};
    bool handle_gcode(GCode& gcode, OutputStream& os);
    bool configure(ConfigReader& cr);

private:
    bool handle_mcode(GCode& gcode, OutputStream& os);
    bool set_trim(float x, float y, float z, OutputStream& os);
    bool get_trim(float& x, float& y, float& z);
    bool calibrate_delta_endstops(GCode& gcode, OutputStream& os);
    bool calibrate_delta_radius(GCode& gcode, OutputStream& os);
    bool probe_delta_points(GCode& gcode, OutputStream& os);
    bool findBed(float& ht);

    float probe_radius;
    float initial_height;
    float tolerance;
};

