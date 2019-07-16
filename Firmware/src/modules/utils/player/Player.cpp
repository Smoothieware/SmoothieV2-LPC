#include "Player.h"

#include "Robot.h"
#include "OutputStream.h"
#include "GCode.h"
#include "ConfigReader.h"
#include "Dispatcher.h"
#include "Conveyor.h"
#include "StringUtils.h"
#include "TemperatureControl.h"
#include "main.h"
#include "MessageQueue.h"

#include "FreeRTOS.h"
#include "task.h"

#include <cstddef>
#include <cmath>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>

#define on_boot_gcode_key "on_boot_gcode"
#define on_boot_gcode_enable_key "on_boot_gcode_enable"
#define after_suspend_gcode_key "after_suspend_gcode"
#define before_resume_gcode_key "before_resume_gcode"
#define leave_heaters_on_suspend_key "leave_heaters_on_suspend"

#define HELP(m) if(params == "-h") { os.printf("%s\n", m); return true; }

Player *Player::instance = nullptr;
// needs to be static as it may be used by a queued gcode after abort
OutputStream Player::nullos;

REGISTER_MODULE(Player, Player::create)

bool Player::create(ConfigReader& cr)
{
    printf("DEBUG: configure player\n");
    Player *player = new Player();
    if(!player->configure(cr)) {
        printf("WARNING: Failed to configure Player\n");
    }

    return true;
}

Player::Player() : Module("player")
{
    this->playing_file = false;
    this->current_file_handler = nullptr;
    this->booted = false;
    this->start_ticks = 0;
    this->reply_os = nullptr;
    this->current_os = nullptr;
    this->suspended = false;
    this->suspend_loops = 0;
    abort_thread = false;
    abort_flg= false;
    play_thread_exited = false;
    instance = this;
}

bool Player::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("player", m)) {
        printf("WARNING:configure-player: no player section found, defaults used\n");
    }

    this->on_boot_gcode = cr.get_string(m, on_boot_gcode_key, "/sd/on_boot.gcode");
    this->on_boot_gcode_enable = cr.get_bool(m, on_boot_gcode_enable_key, true);

    this->leave_heaters_on = cr.get_bool(m, leave_heaters_on_suspend_key, false);
    this->after_suspend_gcode = cr.get_string(m, after_suspend_gcode_key, "");
    this->before_resume_gcode = cr.get_string(m, before_resume_gcode_key, "");
    std::replace( this->after_suspend_gcode.begin(), this->after_suspend_gcode.end(), '_', ' '); // replace _ with space
    std::replace( this->before_resume_gcode.begin(), this->before_resume_gcode.end(), '_', ' '); // replace _ with space

    // g/m code handlers
    using std::placeholders::_1;
    using std::placeholders::_2;

    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 28, std::bind(&Player::handle_gcode, this, _1, _2));

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 21, std::bind(&Player::handle_gcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 24, std::bind(&Player::handle_gcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 25, std::bind(&Player::handle_gcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 26, std::bind(&Player::handle_gcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 27, std::bind(&Player::handle_gcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 600, std::bind(&Player::handle_gcode, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 601, std::bind(&Player::handle_gcode, this, _1, _2));

    // These are special as they violate g code specs and pass a string parameter (filename)
    THEDISPATCHER->add_handler("m23", std::bind(&Player::handle_m23, this, _1, _2));
    THEDISPATCHER->add_handler("m32", std::bind(&Player::handle_m32, this, _1, _2));

    // command handlers
    THEDISPATCHER->add_handler( "play", std::bind( &Player::play_command, this, _1, _2) );
    THEDISPATCHER->add_handler( "progress", std::bind( &Player::progress_command, this, _1, _2) );
    THEDISPATCHER->add_handler( "abort", std::bind( &Player::abort_command, this, _1, _2) );
    THEDISPATCHER->add_handler( "suspend", std::bind( &Player::suspend_command, this, _1, _2) );
    THEDISPATCHER->add_handler( "resume", std::bind( &Player::resume_command, this, _1, _2) );

    // set this so the command ctx call back gets called
    want_command_ctx = true;

    return true;
}

// extract any options found on line, terminates args at the space before the first option (-v)
// eg this is a file.gcode -v
//    will return -v and set args to this is a file.gcode
std::string Player::extract_options(std::string& args)
{
    std::string opts;
    size_t pos = args.find(" -");
    if(pos != std::string::npos) {
        opts = args.substr(pos);
        args = args.substr(0, pos);
    }

    return opts;
}

bool Player::handle_m23(std::string& params, OutputStream& os)
{
    HELP("M23 - select file")

    std::string cmd("/sd/");
    cmd.append(params); // filename is whatever is in params including spaces
    cmd.append(" -p"); // starts paused
    play_command(cmd, nullos);

    if(this->current_file_handler == nullptr) {
        os.printf("file.open failed: %s\n", params.c_str());

    } else {
        os.printf("File opened:%s Size:%ld\n", params.c_str(), this->file_size);
        os.printf("File selected\n");
    }

    return true;
}

bool Player::handle_m32(std::string& params, OutputStream& os)
{
    HELP("M32 - select file and start print");

    std::string f("/sd/");
    f.append(params); // filename is whatever is in params including spaces

    play_command(f, nullos);

    // we need to send back different messages for M32
    if(this->current_file_handler == nullptr) {
        os.printf("file.open failed: %s\n", params.c_str());
    }

    return true;
}

bool Player::handle_gcode(GCode& gcode, OutputStream& os)
{
    if (gcode.has_m()) {
        switch(gcode.get_code()) {
            case 21: // Dummy code; makes Octoprint happy -- supposed to initialize SD card
                os.printf("SD card ok\n");
                break;

            case 24: // start print
                if (this->current_file_handler != NULL) {
                    this->playing_file = true;
                    this->reply_os = &os;
                }
                break;

            case 25:  // pause print
                this->playing_file = false;
                break;

            case 26: // Reset print. Slightly different than M26 in Marlin and the rest
                if(this->current_file_handler != nullptr) {
                    std::string currentfn = this->filename.c_str();

                    // abort the print
                    std::string cmd;
                    abort_command(cmd, nullos);

                    if(!currentfn.empty()) {
                        play_command(currentfn, nullos);

                        if(this->current_file_handler == nullptr) {
                            os.printf("file.open failed: %s\n", currentfn.c_str());
                        }
                    }

                } else {
                    os.printf("No file loaded\n");
                }
                break;

            case 27: { // report print progress, in format used by Marlin
                std::string cmd("-b");
                progress_command(cmd, os);
            }
            break;

            case 600: { // suspend print, Not entirely Marlin compliant, M600.1 will leave the heaters on
                std::string cmd((gcode.get_subcode() == 1) ? "h" : "");
                this->suspend_command(cmd, os);
            }
            break;

            case 601: { // resume print
                std::string cmd;
                this->resume_command(cmd, os);
            }
            break;

            default:
                return false;
        }

    } else if(gcode.has_g()) {
        // TODO handle grbl mode
        if(gcode.get_code() == 28 && gcode.get_subcode() == 0) { // homing cancels suspend
            if(this->suspended) {
                // clean up
                this->suspended = false;
                Robot::getInstance()->pop_state();
                this->saved_temperatures.clear();
                this->was_playing_file = false;
                this->suspend_loops = 0;
                os.printf("//Suspend cancelled due to homing cycle\n");
            }
        }
    }

    return true;
}

// This is a task
void Player::play_thread(void*)
{
    instance->player_thread();
    vTaskDelete( NULL );
}

// Play a gcode file by considering each line as if it was received on the serial console
bool Player::play_command( std::string& params, OutputStream& os )
{
    HELP("play file [-v] [-p]")

    // extract any options from the line and terminate the line there
    std::string options = extract_options(params);

    if(this->playing_file || this->suspended) {
        os.printf("Currently printing, abort print first\n");
        return true;
    }

    // Get filename which is the entire parameter line upto any options found or entire line
    this->filename = params;

    if(this->current_file_handler != nullptr) { // must have been a paused print
        fclose(this->current_file_handler);
    }

    this->current_file_handler = fopen( this->filename.c_str(), "r");
    if(this->current_file_handler == nullptr) {
        os.printf("File not found: %s\n", this->filename.c_str());
        return true;
    }

    os.printf("Playing %s\n", this->filename.c_str());

    if( options.find_first_of("Pp") == std::string::npos ) {
        this->playing_file = true;
    } else {
        this->playing_file = false; // start paused
    }

    // Output to the current stream if we were passed the -v ( verbose ) option
    if( options.find_first_of("Vv") == std::string::npos ) {
        this->current_os = nullptr;
    } else {
        // FIXME this os cannot go away, better check
        this->current_os = &os;
    }

    // get size of file
    struct stat buf;
    if (stat(filename.c_str(), &buf) >= 0) {
        file_size = buf.st_size;
        os.printf("  File size %ld\n", file_size);
    } else {
        os.printf("WARNING - Could not get file size\n");
        file_size = 0;
    }

    this->played_cnt = 0;

    // start play thread
    play_thread_exited = false;

    BaseType_t status = xTaskCreate(play_thread, "PlayThread", 4000/4, NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);
    if (status != pdPASS) {
        printf("Player: xTaskCreate failed, status=%ld\n", status);
    }

    return true;
}

bool Player::progress_command( std::string& params, OutputStream& os )
{
    HELP("Display progress of sdcard print")

    // get options
    std::string options = stringutils::shift_parameter( params );
    bool sdprinting = options.find_first_of("Bb") != std::string::npos;

    if(!playing_file && current_file_handler != nullptr) {
        if(sdprinting)
            os.printf("SD printing byte %lu/%lu\n", played_cnt, file_size);
        else
            os.printf("SD print is paused at %lu/%lu\n", played_cnt, file_size);
        return true;

    } else if(!playing_file) {
        os.printf("Not currently playing\n");
        return true;
    }

    if(file_size > 0) {
        unsigned long est = 0;
        unsigned long elapsed_secs = ((xTaskGetTickCount() - start_ticks) * 1000 / configTICK_RATE_HZ) / 1000;
        if(elapsed_secs > 10) {
            unsigned long bytespersec = played_cnt / elapsed_secs;
            if(bytespersec > 0)
                est = (file_size - played_cnt) / bytespersec;
        }

        float pcnt = ((float)file_size - (file_size - played_cnt)) * 100.0F / file_size;
        // If -b or -B is passed, report in the format used by Marlin and the others.
        if (!sdprinting) {
            os.printf("file: %s, %5.1f %% complete, elapsed time: %02lu:%02lu:%02lu", this->filename.c_str(), roundf(pcnt), elapsed_secs / 3600, (elapsed_secs % 3600) / 60, elapsed_secs % 60);
            if(est > 0) {
                os.printf(", est time: %02lu:%02lu:%02lu",  est / 3600, (est % 3600) / 60, est % 60);
            }
            os.printf("\n");
        } else {
            os.printf("SD printing byte %lu/%lu\n", played_cnt, file_size);
        }

    } else {
        os.printf("File size is unknown\n");
    }

    return true;
}

bool Player::abort_command( std::string& params, OutputStream& os )
{
    HELP("abort playing file");

    if(!playing_file && current_file_handler == nullptr) {
        os.printf("Not currently playing\n");
        return true;
    }

    printf("DEBUG: aborting play, waiting for thread to exit..\n");

    abort_thread = true;
    suspended = false;

    // there could be several gcodes queued after thread exits, we need to flush the message queue as well
    // so we need to complete this in command thread context when it is idle
    if(params.empty()) {
        abort_flg = true;
        os.printf("Please wait for abort to complete. Turn any heaters off manually\n");
    }

    return true;
}

// called when in command thread context, we can issue commands here
// NOTE when idle only called once every 200ms
void Player::in_command_ctx(bool idle)
{
    if( !this->booted ) {
        this->booted = true;
        if( this->on_boot_gcode_enable ) {
            this->play_command(this->on_boot_gcode, nullos);

        } else {
            printf("player: On boot gcode disabled.\n");
        }
    }

    if(abort_flg && idle) {
        // we need to abort but there will be gcodes in the message queue so wait until idle then clean up
        abort_flg= false;
        // clear out the block queue, will wait until queue is empty
        // MUST be called in command thread context when idle to make sure there are no blocked messages waiting to put something on the queue
        Conveyor::getInstance()->flush_queue();

        // now the position will think it is at the last received pos, so we need to do FK to get the actuator position and reset the current position
        Robot::getInstance()->reset_position_from_current_actuator_position();
        printf("DEBUG: Abort completed\n");
        return;
    }

    if(suspended && suspend_loops > 0) {
        // if we are suspended we need to allow the command thread to cycle a few times to flush the queus,
        // then finish off the suspend processing
        if(idle && --suspend_loops == 0) {
            suspend_part2();
            return;
        }
    }

    // clean up the play thread once it has finished normally
    if(play_thread_exited) {
        play_thread_exited = false;
    }
}

void Player::player_thread()
{
    printf("DEBUG: Player thread starting\n");

    start_ticks = xTaskGetTickCount();
    char buf[130]; // lines upto 128 characters are allowed, anything longer is discarded
    bool discard = false;
    uint32_t linecnt= 0;

    while(fgets(buf, sizeof(buf), this->current_file_handler) != NULL) {
        while(!playing_file && !abort_thread && !Module::is_halted()) {
            // we must be paused
            vTaskDelay(pdMS_TO_TICKS(200)); // sleep and yield
        }

        // allows us to abort the thread
        if(abort_thread || Module::is_halted()) {
            abort_thread = false;
            break;
        }

        int len = strlen(buf);
        if(len == 0) continue; // empty line? should not be possible
        if(buf[len - 1] == '\n' || feof(this->current_file_handler)) {
            if(discard) { // we are discarding a long line
                discard = false;
                continue;
            }
            if(len == 1) continue; // empty line

            if(current_os != nullptr) {
                current_os->puts(buf);
            }

            buf[len - 1] = '\0'; // remove the \n and nul terminate

            // we do not want to fill the message queue, so leave some space in it
            //while(get_message_queue_space() < 2) vTaskDelay(pdMS_TO_TICKS(1));

            // don't fill block queue so don't let planner stall on a full queue
            Conveyor::getInstance()->wait_for_room();

            send_message_queue(buf, &nullos);

            played_cnt += len;

            if((++linecnt % 100) == 0) {
                // yield to some other threads every 100 lines or so
                vTaskDelay(pdMS_TO_TICKS(1));
            }

        } else {
            // discard long line
            if(this->current_os != nullptr) { this->current_os->printf("Warning: Discarded long line\n"); }
            discard = true;
        }
    }

    // finished file, clean up
    this->playing_file = false;
    this->filename = "";
    played_cnt = 0;
    file_size = 0;
    fclose(this->current_file_handler);
    current_file_handler = nullptr;
    this->current_os = nullptr;

    if(this->reply_os != nullptr) {
        // if we were printing from an M command from pronterface we need to send this back
        this->reply_os->printf("Done printing file\n");
        this->reply_os = nullptr;
    }

    printf("DEBUG: Player thread exiting\n");

    // indicates that the thread has finished, used to clean up
    play_thread_exited = true;
}

bool Player::request(const char *key, void *value)
{
    if(strcmp("is_playing", key) == 0) {
        *(bool*)value = this->playing_file;
        return true;

    } else if(strcmp("is_suspended", key) == 0) {
        *(bool*)value = this->suspended;
        return true;

    } else if(strcmp("get_progress", key) == 0) {
        if(file_size > 0 && playing_file) {
            // TODO implement
            // struct pad_progress p;
            // p.elapsed_secs = this->elapsed_secs;
            // p.percent_complete = (this->file_size - (this->file_size - this->played_cnt)) * 100 / this->file_size;
            // p.filename = this->filename;
            // *(struct pad_progress *)value = p;
            return true;
        }

    } else if(strcmp("abort_play", key) == 0) {
        OutputStream os;
        std::string cmd;
        abort_command(cmd, os);
        return true;
    }

    return false;
}

/**
Suspend a print in progress
1. send pause to upstream host, or pause if printing from sd
1a. loop on_main_loop several times to clear any buffered commmands TODO will need to change this as there is no main loop
2. wait for empty queue
3. save the current position, extruder position, temperatures - any state that would need to be restored
4. retract by specifed amount either on command line or in config
5. turn off heaters.
6. optionally run after_suspend gcode (either in config or on command line)

User may jog or remove and insert filament at this point, extruding or retracting as needed

*/
bool Player::suspend_command(std::string& params, OutputStream& os )
{
    HELP("suspend operation l parameter will leave heaters on")

    if(suspended) {
        os.printf("Already suspended\n");
        return true;
    }

    os.printf("Suspending print, waiting for queue to empty...\n");

    // override the leave_heaters_on setting
    this->override_leave_heaters_on = (params == "l");

    suspended = true;
    if( this->playing_file ) {
        // pause an sd print
        this->playing_file = false;
        this->was_playing_file = true;
    } else {
        // send pause to upstream host
        os.printf("// action:pause\n");
        this->was_playing_file = false;
    }

    // there are queues that may be full of commands so we need to allow command thread to cycle a few times
    // to clear any buffered commands in the comms streams etc
    // as we also check for the command thread to be idle we only need 2 iterations
    suspend_loops = 2;

    return true;
}

// this completes the suspend
void Player::suspend_part2()
{
    //  need to use streams here as the original stream may have changed
    print_to_all_consoles("// Waiting for queue to empty (Host must stop sending)...\n");
    // wait for queue to empty
    Conveyor::getInstance()->wait_for_idle();

    if(Module::is_halted()) {
        print_to_all_consoles("Suspend aborted by kill\n");
        suspended= false;
        return;
    }

    print_to_all_consoles("// Saving current state...\n");

    // save current XYZ position
    Robot::getInstance()->get_axis_position(this->saved_position);

    // save current extruder state for all extruders
    std::vector<Module*> extruders = Module::lookup_group("extruder");
    for(auto m : extruders) {
        m->request("save_state", nullptr);
    }

    // save state use M120
    Robot::getInstance()->push_state();

    // TODO retract by optional amount...

    this->saved_temperatures.clear();
    if(!this->leave_heaters_on && !this->override_leave_heaters_on) {
        // save current temperatures

        // scan all temperature controls
        std::vector<Module*> controllers = Module::lookup_group("temperature control");
        for(auto m : controllers) {
            // query each heater and save the target temperature if on
            TemperatureControl::pad_temperature_t temp;
            if(m->request("get_current_temperature", &temp)) {
                //m->get_instance_name(), temp.designator.c_str(), temp.tool_id, temp.current_temperature, temp.target_temperature, temp.pwm);
                // TODO see if in exclude list
                if(temp.target_temperature > 0) {
                    this->saved_temperatures[m] = temp.target_temperature;
                    // turn off heaters that were on
                    float t = 0;
                    m->request("set_temperature", &t);
                }
            }
        }
    }

    // execute optional gcode if defined
    if(!after_suspend_gcode.empty()) {
        dispatch_line(nullos, after_suspend_gcode.c_str());

    }

    print_to_all_consoles("// Print Suspended, enter resume to continue printing\n");
}

/**
resume the suspended print
1. restore the temperatures and wait for them to get up to temp
2. optionally run before_resume gcode if specified
3. restore the position it was at and E and any other saved state
4. resume sd print or send resume upstream
*/
bool Player::resume_command(std::string& params, OutputStream& os )
{
    HELP("Resume a suspended operation")

    if(!suspended) {
        os.printf("Not suspended\n");
        return true;
    }

    os.printf("resuming print...\n");

    // wait for them to reach temp
    if(!this->saved_temperatures.empty()) {
        // set heaters to saved temps
        for(auto& h : this->saved_temperatures) {
            float t = h.second;
            h.first->request("set_temperature", &t);
        }
        os.printf("Waiting for heaters...\n");
        bool wait = true;
        int cnt = 0;
        while(wait) {
            safe_sleep(100); // sleep for 100ms
            wait = false;

            bool timeup = (cnt++ % 10) == 0; // every second

            for(auto& h : this->saved_temperatures) {
                struct TemperatureControl::pad_temperature temp;
                if(h.first->request("get_current_temperature", &temp)) {
                    if(timeup) {
                        os.printf("%s:%3.1f /%3.1f @%d ", temp.designator.c_str(), temp.current_temperature, ((temp.target_temperature == -1) ? 0.0 : temp.target_temperature), temp.pwm);
                    }
                    wait = wait || (temp.current_temperature < h.second);
                }
            }
            if(timeup) os.printf("\n");

            if(Module::is_halted()) {
                // abort temp wait and rest of resume
                os.printf("Resume aborted by kill\n");
                Robot::getInstance()->pop_state();
                this->saved_temperatures.clear();
                suspended = false;
                return true;
            }
        }
    }

    // clean up
    this->saved_temperatures.clear();
    suspended = false;

    if(Module::is_halted()) {
        // abort temp wait and rest of resume
        os.printf("Resume aborted by kill\n");
        Robot::getInstance()->pop_state();
        suspended = false;
        return true;
    }

    // execute optional gcode if defined
    if(!before_resume_gcode.empty()) {
        os.printf("Executing before resume gcode...\n");
        dispatch_line(nullos, before_resume_gcode.c_str());
        Conveyor::getInstance()->wait_for_idle();
    }

    // Restore position
    os.printf("Restoring saved XYZ positions and state...\n");
    Robot::getInstance()->pop_state();
    bool abs_mode = Robot::getInstance()->absolute_mode; // what mode we were in
    // force absolute mode for restoring position, then set to the saved relative/absolute mode
    Robot::getInstance()->absolute_mode = true;

    // NOTE position was saved in MCS so must use G53 to restore position
    Robot::getInstance()->next_command_is_MCS = true; // must use machine coordinates in case G92 or WCS is in effect
    THEDISPATCHER->dispatch(nullos, 'G', 0, 'X', saved_position[0], 'Y', saved_position[1], 'Z', saved_position[2], 0);
    Conveyor::getInstance()->wait_for_idle();

    Robot::getInstance()->absolute_mode = abs_mode;

    // restore extruder state
    std::vector<Module*> extruders = Module::lookup_group("extruder");
    for(auto m : extruders) {
        m->request("restore_state", nullptr);
    }

    if(Module::is_halted()) {
        os.printf("Resume aborted by kill\n");
        suspended = false;
        return true;
    }

    os.printf("Resuming print\n");

    if(this->was_playing_file) {
        this->playing_file = true;
        this->was_playing_file = false;

    } else {
        // Send resume to host
        os.printf("// action:resume\n");
    }

    return true;
}

void Player::on_halt(bool flg)
{
    if(flg && suspended) {
       // clean up from suspend
       suspended= false;
       Robot::getInstance()->pop_state();
       saved_temperatures.clear();
       was_playing_file= false;
       suspend_loops= 0;
       print_to_all_consoles("// Suspend cleared\n");
    }
}
