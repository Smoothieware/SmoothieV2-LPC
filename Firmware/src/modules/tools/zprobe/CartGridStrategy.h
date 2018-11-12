#pragma once

#include "ZProbeStrategy.h"

#include <string.h>
#include <tuple>

class OutputStream;
class GCode;
class ConfigReader;

class CartGridStrategy : public ZProbeStrategy
{
public:
    CartGridStrategy(ZProbe *zprobe);
    ~CartGridStrategy();
    bool configure(ConfigReader& cr);
    bool handle_gcode(GCode& gcode, OutputStream& os);

private:
    bool handle_mcode(GCode& gcode, OutputStream& os);

    bool doProbe(GCode& gcode, OutputStream& os);
    bool findBed();
    void setAdjustFunction(bool on);
    void print_bed_level(OutputStream& os);
    void doCompensation(float *target, bool inverse);
    void reset_bed_level();
    void save_grid(OutputStream& os);
    bool load_grid(OutputStream& os);
    bool probe_grid(int n, int m, float _x_start, float _y_start, float _x_size, float _y_size, OutputStream& os);

    float initial_height;
    float tolerance;

    float height_limit;
    float dampening_start;
    float damping_interval;

    float *grid;
    std::tuple<float, float, float> probe_offsets;
    float x_start,y_start;
    float x_size,y_size;

    struct {
        uint8_t configured_grid_x_size:8;
        uint8_t configured_grid_y_size:8;
        uint8_t current_grid_x_size:8;
        uint8_t current_grid_y_size:8;
        bool save:1;
        bool do_home:1;
        bool only_by_two_corners:1;
        bool human_readable:1;
    };
};
