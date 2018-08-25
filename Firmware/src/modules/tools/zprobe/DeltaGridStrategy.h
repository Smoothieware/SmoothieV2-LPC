#pragma once

#include "ZProbeStrategy.h"

#include <string.h>
#include <tuple>

class OutputStream;
class GCode;
class ConfigReader;

class DeltaGridStrategy : public ZProbeStrategy
{
public:
    DeltaGridStrategy(ZProbe *);
    ~DeltaGridStrategy();
    bool handleGCode(GCode& gcode, OutputStream& os);
    bool configure(ConfigReader& cr);

private:
    bool handle_mcode(GCode& gcode, OutputStream& os);
    void extrapolate_one_point(int x, int y, int xdir, int ydir);
    void extrapolate_unprobed_bed_level();
    bool doProbe(GCode& gcode, OutputStream& os);
    bool findBed(float& ht);
    void setAdjustFunction(bool on);
    void print_bed_level(OutputStream& os);
    void doCompensation(float *target, bool inverse);
    void reset_bed_level();
    void save_grid(OutputStream& os);
    bool load_grid(OutputStream& os);
    bool probe_spiral(int n, float radius, OutputStream& os);
    bool probe_grid(int n, float radius, OutputStream& os);

    float initial_height;
    float tolerance;

    float *grid;
    float grid_radius;
    std::tuple<float, float, float> probe_offsets;
    uint8_t grid_size;

    struct {
        bool save:1;
        bool do_home:1;
    };
};
