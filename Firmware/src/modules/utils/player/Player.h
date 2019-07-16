#pragma once

#include "Module.h"

#include <string>
#include <map>
#include <vector>
#include <thread>

class OutputStream;
class GCode;

class Player : public Module {
    public:
        Player();

        static bool create(ConfigReader& cr);
        bool configure(ConfigReader&);
        void in_command_ctx(bool idle);
        bool request(const char *key, void *value);
        void on_halt(bool flg);

    private:
        bool handle_gcode(GCode& gcode, OutputStream& os);
        bool handle_m23(std::string& params, OutputStream& os);
        bool handle_m32(std::string& params, OutputStream& os);

        bool play_command( std::string& parameters, OutputStream& os );
        bool progress_command( std::string& parameters, OutputStream& os );
        bool abort_command( std::string& parameters, OutputStream& os );
        bool suspend_command( std::string& parameters, OutputStream& os );
        bool resume_command( std::string& parameters, OutputStream& os );
        std::string extract_options(std::string& args);
        void suspend_part2();
        static void play_thread(void *);
        void player_thread();
        static OutputStream nullos;
        static Player *instance;
        std::string filename;
        std::string after_suspend_gcode;
        std::string before_resume_gcode;
        std::string on_boot_gcode;
        OutputStream *current_os;
        OutputStream *reply_os;

        FILE* current_file_handler;
        long file_size;
        unsigned long played_cnt;
        unsigned long start_ticks;
        float saved_position[3]; // only saves XYZ
        std::map<Module*, float> saved_temperatures;

        volatile bool abort_thread;
        volatile bool play_thread_exited;
        volatile bool abort_flg;
        volatile bool playing_file;

        struct {
            bool on_boot_gcode_enable:1;
            bool booted:1;
            bool suspended:1;
            bool was_playing_file:1;
            bool leave_heaters_on:1;
            bool override_leave_heaters_on:1;
            uint8_t suspend_loops:4;
        };
};
