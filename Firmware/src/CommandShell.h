#pragma once

#include <string>

class OutputStream;
class GCode;

class CommandShell
{
public:
    CommandShell();
    ~CommandShell(){};

    bool initialize();

private:
    bool truncate_file(const char *fn, int size, OutputStream& os);

    // commands
    bool help_cmd(std::string& params, OutputStream& os);
    bool ls_cmd(std::string& params, OutputStream& os);
    bool rm_cmd(std::string& params, OutputStream& os);
    bool mv_cmd(std::string& params, OutputStream& os);
    bool cp_cmd(std::string& params, OutputStream& os);
    bool cd_cmd(std::string& params, OutputStream& os);
    bool mkdir_cmd(std::string& params, OutputStream& os);
    bool config_set_cmd(std::string& params, OutputStream& os);
    bool config_get_cmd(std::string& params, OutputStream& os);
    bool mem_cmd(std::string& params, OutputStream& os);
    //bool mount_cmd(std::string& params, OutputStream& os);
    bool cat_cmd(std::string& params, OutputStream& os);
    bool md5sum_cmd(std::string& params, OutputStream& os);
    bool switch_cmd(std::string& params, OutputStream& os);
    bool switch_poll_cmd(std::string& params, OutputStream& os);
    bool modules_cmd(std::string& params, OutputStream& os);
    bool gpio_cmd(std::string& params, OutputStream& os);
    bool get_cmd(std::string& params, OutputStream& os);
    bool grblDP_cmd(std::string& params, OutputStream& os);
    bool grblDG_cmd(std::string& params, OutputStream& os);
    bool grblDH_cmd(std::string& params, OutputStream& os);
    bool test_cmd(std::string& params, OutputStream& os);
    bool version_cmd(std::string& params, OutputStream& os);
    bool m20_cmd(GCode& gcode, OutputStream& os);
    bool m115_cmd(GCode& gcode, OutputStream& os);
    bool ry_cmd(std::string& params, OutputStream& os);
    bool truncate_cmd(std::string& params, OutputStream& os);
    bool break_cmd(std::string& params, OutputStream& os);
    bool reset_cmd(std::string& params, OutputStream& os);
    bool flash_cmd(std::string& params, OutputStream& os);
    bool jog_cmd(std::string& params, OutputStream& os);

    bool mounted;
};
