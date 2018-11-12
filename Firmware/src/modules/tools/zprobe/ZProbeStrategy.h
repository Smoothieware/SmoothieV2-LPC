/*
 * A strategy called from ZProbe to handle a strategy for leveling or calibration
 * examples are delta calibration, three point bed leveling, z height map
 */
#pragma once

class ZProbe;
class GCode;
class OutputStream;
class ConfigReader;

class ZProbeStrategy
{
public:
    ZProbeStrategy(ZProbe* zprb) : zprobe(zprb) {}
    virtual ~ZProbeStrategy(){};
    virtual bool handle_gcode(GCode& gcode, OutputStream& os)= 0;
    virtual bool configure(ConfigReader& cr)= 0;

protected:
    ZProbe *zprobe;

};
