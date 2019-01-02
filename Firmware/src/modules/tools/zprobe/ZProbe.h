#pragma once

#include "Module.h"
#include "Pin.h"

// defined here as they are used in multiple files
#define leveling_strategy_key "leveling-strategy"

class StepperMotor;
class GCode;
class OutputStream;
class ZProbeStrategy;
class ConfigReader;

class ZProbe: public Module
{

public:
    ZProbe();
    virtual ~ZProbe() {}

    static bool create(ConfigReader& cr);
    bool configure(ConfigReader& cr);

    bool run_probe(float& mm, float feedrate, float max_dist= -1, bool reverse= false);
    bool run_probe_return(float& mm, float feedrate, float max_dist= -1, bool reverse= false);
    bool doProbeAt(float &mm, float x, float y);

    void move_xy(float x, float y, float feedrate, bool relative=false);
    void move_x(float x, float feedrate, bool relative=false);
    void move_y(float y, float feedrate, bool relative=false);
    void move_z(float z, float feedrate, bool relative=false);
    void home();

    bool getProbeStatus() { return this->pin.get(); }
    float getSlowFeedrate() const { return slow_feedrate; }
    float getFastFeedrate() const { return fast_feedrate; }
    float getProbeHeight() const { return probe_height; }

private:
    bool handle_gcode(GCode& gcode, OutputStream& os);
    bool handle_mcode(GCode& gcode, OutputStream& os);
    void probe_XYZ(GCode& gc, OutputStream& os, uint8_t axis);
    void read_probe(void);

    float slow_feedrate;
    float fast_feedrate;
    float return_feedrate;
    float probe_height;
    float max_travel;
    float dwell_before_probing;

    Pin pin;
    ZProbeStrategy *leveling_strategy{nullptr};
    ZProbeStrategy *calibration_strategy{nullptr};

    uint16_t debounce_ms;
    uint16_t debounce{0};

    volatile struct {
        bool probing:1;
        bool reverse_z:1;
        bool invert_override:1;
        volatile bool probe_detected:1;
    };
};
