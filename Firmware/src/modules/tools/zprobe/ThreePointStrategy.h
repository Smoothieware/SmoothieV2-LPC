#pragma once

#include "ZProbeStrategy.h"

#include <string.h>
#include <tuple>

class OutputStream;
class Plane3D;
class GCode;
class ConfigReader;

class ThreePointStrategy : public ZProbeStrategy
{
public:
    ThreePointStrategy(ZProbe *zprobe);
    ~ThreePointStrategy();
    bool handle_gcode(GCode& gcode, OutputStream& os);
    bool configure(ConfigReader& cr);
    float getZOffset(float x, float y);

private:
    bool handle_mcode(GCode& gcode, OutputStream& os);
    void homeXY();
    bool doProbing(OutputStream& os);
    std::tuple<float, float> parseXY(const char *str);
    std::tuple<float, float, float> parseXYZ(const char *str);
    void setAdjustFunction(bool);
    bool test_probe_points(OutputStream& os);

    std::tuple<float, float, float> probe_offsets;
    std::tuple<float, float> probe_points[3];
    Plane3D *plane;
    struct {
        bool home:1;
        bool save:1;
    };
    float tolerance;
};
