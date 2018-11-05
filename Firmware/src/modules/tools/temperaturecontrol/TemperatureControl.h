#pragma once

#include "Module.h"
#include "ConfigReader.h"

#include <string>

class TempSensor;
class SigmaDeltaPwm;
class GCode;
class OutputStream;
class Pin;

class TemperatureControl : public Module
{

public:
    TemperatureControl(const char *name);
    ~TemperatureControl();

    static bool load_controls(ConfigReader& cr);
    void on_halt(bool flg);
    bool request(const char *key, void *value);

    void set_desired_temperature(float desired_temperature);
    float get_temperature();
    const char *get_designator() const { return designator.c_str(); }

    using pad_temperature_t = struct pad_temperature {
        float current_temperature;
        float target_temperature;
        int pwm;
        uint8_t tool_id;
        std::string designator;
    };


    friend class PID_Autotuner;

private:
    bool configure(ConfigReader& cr, ConfigReader::section_map_t& m);

    void thermistor_read_tick(void);
    void pid_process(float);
    void setPIDp(float p);
    void setPIDi(float i);
    void setPIDd(float d);
    void check_runaway();
    bool handle_mcode(GCode& gcode, OutputStream& os);
    bool handle_M6(GCode& gcode, OutputStream& os);
    bool handle_autopid(GCode& gcode, OutputStream& os);

    float target_temperature;
    float max_temp, min_temp;

    float preset1;
    float preset2;

    TempSensor *sensor{nullptr};
    float i_max;
    int o;
    float last_reading;
    float readings_per_second;

    SigmaDeltaPwm *heater_pin{nullptr};
#ifdef BOARD_PRIMEALPHA
    static Pin *vfet_enable_pin;
#endif
    std::string designator;

    float hysteresis;
    float iTerm;
    float lastInput;
    // PID settings
    float p_factor;
    float i_factor;
    float d_factor;
    float PIDdt;

    float runaway_error_range;

    enum RUNAWAY_TYPE {NOT_HEATING, HEATING_UP, COOLING_DOWN, TARGET_TEMPERATURE_REACHED};

    uint8_t tool_id{0};

    // pack these to save memory
    struct {
        uint16_t name_checksum;
        uint16_t set_m_code: 10;
        uint16_t set_and_wait_m_code: 10;
        uint16_t get_m_code: 10;
        RUNAWAY_TYPE runaway_state: 2;
        // Temperature runaway config options
        uint8_t runaway_range: 6; // max 63
        uint16_t runaway_heating_timeout: 9; // 4088 secs
        uint16_t runaway_cooling_timeout: 9; // 4088 secs
        uint16_t runaway_timer: 9;
        uint8_t tick: 3;
        bool use_bangbang: 1;
        bool temp_violated: 1;
        bool active: 1;
        bool readonly: 1;
        bool windup: 1;
        bool sensor_settings: 1;
    };
};

