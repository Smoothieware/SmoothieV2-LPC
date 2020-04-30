#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <malloc.h>
#include <fstream>
#include <vector>
#include <functional>

#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include "semphr.h"

#include "uart_comms.h"
#include "uart3_comms.h"
#include "stopwatch.h"

#include "Module.h"
#include "OutputStream.h"
#include "MessageQueue.h"
#include "GCode.h"
#include "GCodeProcessor.h"
#include "Dispatcher.h"
#include "Robot.h"
#include "RingBuffer.h"
#include "Conveyor.h"
#include "Pin.h"

static bool system_running= false;
static bool rpi_port_enabled= false;
static uint32_t rpi_baudrate= 115200;
static Pin *aux_play_led = nullptr;

// for ?, $I or $S queries
// for ? then query_line will be nullptr
struct query_t
{
    OutputStream *query_os;
    char *query_line;
};
static RingBuffer<struct query_t, 8> queries; // thread safe FIFO

// set to true when M28 is in effect
static bool uploading = false;
static FILE *upload_fp = nullptr;
static std::string config_error_msg;

// TODO maybe move to Dispatcher
static GCodeProcessor gp;
static bool loaded_configuration= false;
static bool config_override= false;
const char *OVERRIDE_FILE= "/sd/config-override";

MemoryPool *_RAM2;
MemoryPool *_RAM3;
MemoryPool *_RAM4;
MemoryPool *_RAM5;

// Called very early from ResetISR()
void setup_memory_pool()
{
    // MemoryPool stuff - needs to be initialised before __libc_init_array
    // so static ctors can use them
    extern uint8_t __end_bss_RAM2;
    extern uint8_t __top_RAM2;
    extern uint8_t __end_bss_RAM3;
    extern uint8_t __top_RAM3;
    extern uint8_t __end_bss_RAM4;
    extern uint8_t __top_RAM4;
    extern uint8_t __end_bss_RAM5;
    extern uint8_t __top_RAM5;

    // alocate remaining areas to HEAP for those areas
    _RAM2= new MemoryPool(&__end_bss_RAM2, &__top_RAM2 - &__end_bss_RAM2);
    _RAM3= new MemoryPool(&__end_bss_RAM3, &__top_RAM3 - &__end_bss_RAM3);
    _RAM4= new MemoryPool(&__end_bss_RAM4, &__top_RAM4 - &__end_bss_RAM4);
    _RAM5= new MemoryPool(&__end_bss_RAM5, &__top_RAM5 - &__end_bss_RAM5);
}

// load configuration from override file
static bool load_config_override(OutputStream& os)
{
    std::fstream fsin(OVERRIDE_FILE, std::fstream::in);
    if(fsin.is_open()) {
        std::string s;
        OutputStream nullos;
        // foreach line dispatch it
        while (std::getline(fsin, s)) {
            if(s[0] == ';') continue;
            // Parse the Gcode
            GCodeProcessor::GCodes_t gcodes;
            gp.parse(s.c_str(), gcodes);
            // dispatch it
            for(auto& i : gcodes) {
                if(i.get_code() >= 500 && i.get_code() <= 503) continue; // avoid recursion death
                if(!THEDISPATCHER->dispatch(i, nullos)) {
                    os.printf("WARNING: load_config_override: this line was not handled: %s\n", s.c_str());
                }
            }
        }
        loaded_configuration= true;
        fsin.close();

    }else{
        loaded_configuration= false;
        return false;
    }

    return true;
}

// can be called by modules when in command thread context
bool dispatch_line(OutputStream& os, const char *cl)
{
    // Don't like this, but we need a writable copy of the input line
    char line[strlen(cl) + 1];
    strcpy(line, cl);

    // map some special M codes to commands as they violate the gcode spec and pass a string parameter
    // M23, M32, M117, M30 => m23, m32, m117, rm and handle as a command
    if(strncmp(line, "M23 ", 4) == 0) line[0] = 'm';
    else if(strncmp(line, "M30 ", 4) == 0) { strcpy(line, "rm /sd/"); strcpy(&line[7], &cl[4]); } // make into an rm command
    else if(strncmp(line, "M32 ", 4) == 0) line[0] = 'm';
    else if(strncmp(line, "M117 ", 5) == 0) line[0] = 'm';

    // handle save to file M codes:- M28 filename, and M29
    if(strncmp(line, "M28 ", 4) == 0) {
        char *upload_filename = &line[4];
        if(strncmp(upload_filename, "/sd/", 4) != 0) {
            // prepend /sd/ luckily we have exactly 4 characters before the filename
            memcpy(line, "/sd/", 4);
            upload_filename = line;
        }
        upload_fp = fopen(upload_filename, "w");
        if(upload_fp != nullptr) {
            uploading = true;
            os.printf("Writing to file: %s\nok\n", upload_filename);
        } else {
            os.printf("open failed, File: %s.\nok\n", upload_filename);
        }
        return true;
    }

    // see if a command
    if(islower(line[0]) || line[0] == '$') {

        // we could handle this in CommandShell
        if(line[0] == '$' && strlen(line) >= 2) {
            if(line[1] == 'X') {
                // handle $X
                if(Module::is_halted()) {
                    Module::broadcast_halt(false);
                    os.puts("[Caution: Unlocked]\nok\n");
                } else {
                    os.puts("ok\n");
                }
                return true;
            }
        }

        // dispatch command
        if(!THEDISPATCHER->dispatch(line, os)) {
            if(line[0] == '$') {
                os.puts("error:Invalid statement\n");
            } else {
                os.printf("error:Unsupported command - %s\n", line);
            }

        }else if(!os.is_no_response()) {
            os.puts("ok\n");
        }else{
            os.set_no_response(false);
        }

        return true;
    }

    // Handle Gcode
    GCodeProcessor::GCodes_t gcodes;

    // Parse gcode
    if(!gp.parse(line, gcodes)) {
        // line failed checksum, send resend request
        os.printf("rs N%d\n", gp.get_line_number() + 1);
        return true;

    } else if(gcodes.empty()) {
        // if gcodes is empty then was a M110, just send ok
        os.puts("ok\n");
        return true;
    }

    // if in M28 mode then just save all incoming lines to the file until we get M29
    if(uploading && gcodes[0].has_m() && gcodes[0].get_code() == 29) {
        // done uploading, close file
        fclose(upload_fp);
        upload_fp = nullptr;
        uploading = false;
        os.printf("Done saving file.\nok\n");
        return true;
    }

    // dispatch gcodes
    // NOTE return one ok per line instead of per GCode only works for regular gcodes like G0-G3, G92 etc
    // gcodes returning data like M114 should NOT be put on multi gcode lines.
    int ngcodes = gcodes.size();
    for(auto& i : gcodes) {
        //i.dump(os);
        if(i.has_m() || i.has_g()) {

            if(uploading) {
                // just save the gcodes to the file
                if(upload_fp != nullptr) {
                    // write out gcode
                    if(!i.dump(upload_fp)) {
                        // we got an error
                        fclose(upload_fp);
                        upload_fp= nullptr;
                        os.printf("Error:error writing to file.\n");
                    }
                }

                os.printf("ok\n");
                return true;
            }

            // potentially handle M500 - M503 here
            OutputStream *pos= &os;
            std::fstream *fsout= nullptr;
            bool m500= false;

            if(i.has_m() && (i.get_code() >= 500 && i.get_code() <= 503)) {
                if(i.get_code() == 500) {
                    // we have M500 so redirect os to a config-override file
                    fsout= new std::fstream(OVERRIDE_FILE, std::fstream::out | std::fstream::trunc);
                    if(!fsout->is_open()) {
                        os.printf("ERROR: opening file: %s\n", OVERRIDE_FILE);
                        delete fsout;
                        return true;
                    }
                    pos= new OutputStream(fsout);
                    m500= true;

                } else if(i.get_code() == 501) {
                    if(load_config_override(os)) {
                        os.printf("configuration override loaded\nok\n");
                    }else{
                        os.printf("failed to load configuration override\nok\n");
                    }
                    return true;

                } else if(i.get_code() == 502) {
                    remove(OVERRIDE_FILE);
                    os.printf("configuration override file deleted\nok\n");
                    return true;

                } else if(i.get_code() == 503) {
                    if(loaded_configuration) {
                        os.printf("// NOTE: config override loaded\n");
                    }else{
                        os.printf("// NOTE: No config override loaded\n");
                    }
                    i.set_command('M', 500, 3); // change gcode to be M500.3
                }
            }

            // if this is a multi gcode line then dispatch must not send ok unless this is the last one
            if(!THEDISPATCHER->dispatch(i, *pos, ngcodes == 1 && !m500)) {
                // no handler processed this gcode, return ok - ignored
                if(ngcodes == 1) os.puts("ok - ignored\n");
            }

            // clean up after M500
            if(m500) {
                m500= false;
                fsout->close();
                delete fsout;
                delete pos; // this would be the file output stream
                if(!config_override) {
                    os.printf("WARNING: override will NOT be loaded on boot\n", OVERRIDE_FILE);
                }
                os.printf("Settings Stored to %s\nok\n", OVERRIDE_FILE);
            }

        } else {
            // if it has neither g or m then it was a blank line or comment
            os.puts("ok\n");
        }
        --ngcodes;
    }

    return true;
}

static std::function<void(char)> capture_fnc;
void set_capture(std::function<void(char)> cf)
{
    capture_fnc = cf;
}

static std::vector<OutputStream*> output_streams;

// this is here so we do not need to duplicate this logic for USB and UART
void process_command_buffer(size_t n, char *rx_buf, OutputStream *os, char *line, size_t& cnt, bool& discard)
{
    for (size_t i = 0; i < n; ++i) {
        line[cnt] = rx_buf[i];
        if(capture_fnc) {
            capture_fnc(line[cnt]);
            continue;
        }

        if(line[cnt] == 24) { // ^X
            if(!Module::is_halted()) {
                Module::broadcast_halt(true);
                os->puts("ALARM: Abort during cycle\n");
            }
            discard = false;
            cnt = 0;

        } else if(line[cnt] == '?') {
            if(!queries.full()) {
                queries.push_back({os, nullptr});
            }

        } else if(discard) {
            // we discard long lines until we get the newline
            if(line[cnt] == '\n') discard = false;

        } else if(cnt >= MAX_LINE_LENGTH - 1) {
            // discard long lines
            discard = true;
            cnt = 0;
            os->puts("error:Discarding long line\n");

        } else if(line[cnt] == '\n') {
            os->clear_flags(); // clear the done flag here to avoid race conditions
            line[cnt] = '\0'; // remove the \n and nul terminate
            if(cnt == 2 && line[0] == '$' && (line[1] == 'I' || line[1] == 'S')) {
                // Handle $I and $S as instant queries
                if(!queries.full()) {
                    queries.push_back({os, strdup(line)});
                }

            }else{
                send_message_queue(line, os);
            }
            cnt = 0;

        } else if(line[cnt] == '\r') {
            // ignore CR
            continue;

        } else if(line[cnt] == 8 || line[cnt] == 127) { // BS or DEL
            if(cnt > 0) --cnt;

        } else {
            ++cnt;
        }
    }
}

extern "C" size_t write_cdc(const char *buf, size_t len);
extern "C" size_t read_cdc(char *buf, size_t len);
extern "C" int setup_cdc(void *taskhandle);

static void usb_comms(void *)
{
    printf("DEBUG: USB Comms thread running\n");

    if(!setup_cdc(xTaskGetCurrentTaskHandle())) {
        printf("FATAL: CDC setup failed\n");
        return;
    }

    // on first connect we send a welcome message after getting a '\n'
    static const char *welcome_message = "Welcome to Smoothie\nok\n";
    const TickType_t waitms = pdMS_TO_TICKS( 300 );

    size_t n;
    char rx_buf[256];
    bool done = false;

    // first we wait for an initial '\n' sent from host
    while (!done) {
        // Wait to be notified that there has been a USB irq.
        ulTaskNotifyTake( pdTRUE, waitms );
        n = read_cdc(rx_buf, sizeof(rx_buf));
        if(n > 0) {
            for (size_t i = 0; i < n; ++i) {
                if(rx_buf[i] == '\n') {
                    if(config_error_msg.empty()) {
                        write_cdc(welcome_message, strlen(welcome_message));
                    }else{
                        write_cdc(config_error_msg.c_str(), config_error_msg.size());
                    }
                    done = true;
                    break;
                }
            }
        }
    }

    // create an output stream that writes to the cdc
    static OutputStream os([](const char *buf, size_t len) { return write_cdc(buf, len); });
    output_streams.push_back(&os);

    // now read lines and dispatch them
    char line[MAX_LINE_LENGTH];
    size_t cnt = 0;
    bool discard = false;
    while(1) {
        // Wait to be notified that there has been a USB irq.
        uint32_t ulNotificationValue = ulTaskNotifyTake( pdTRUE, waitms );

        if( ulNotificationValue != 1 ) {
            /* The call to ulTaskNotifyTake() timed out. check anyway */
        }

        n = read_cdc(rx_buf, sizeof(rx_buf));
        if(n > 0) {
            process_command_buffer(n, rx_buf, &os, line, cnt, discard);
        }
    }
}

static void uart_comms(void *)
{
    printf("DEBUG: UART Comms thread running\n");
    set_notification_uart(xTaskGetCurrentTaskHandle());

    // create an output stream that writes to the uart
    static OutputStream os([](const char *buf, size_t len) { return write_uart(buf, len); });
    output_streams.push_back(&os);

    const TickType_t waitms = pdMS_TO_TICKS( 300 );

    char rx_buf[256];
    char line[MAX_LINE_LENGTH];
    size_t cnt = 0;
    bool discard = false;
    while(1) {
        // Wait to be notified that there has been a UART irq. (it may have been rx or tx so may not be anything to read)
        uint32_t ulNotificationValue = ulTaskNotifyTake( pdTRUE, waitms );

        if( ulNotificationValue != 1 ) {
            /* The call to ulTaskNotifyTake() timed out. check anyway */
        }

        size_t n = read_uart(rx_buf, sizeof(rx_buf));
        if(n > 0) {
           process_command_buffer(n, rx_buf, &os, line, cnt, discard);
        }
    }
}
#ifdef BOARD_PRIMEALPHA
static void uart3_comms(void *)
{
    printf("DEBUG: UART3 Comms thread running\n");
    set_notification_uart3(xTaskGetCurrentTaskHandle());

    // create an output stream that writes to the uart
    static OutputStream os([](const char *buf, size_t len) { return write_uart3(buf, len); });
    output_streams.push_back(&os);

    const TickType_t waitms = pdMS_TO_TICKS( 300 );

    char rx_buf[256];
    char line[MAX_LINE_LENGTH];
    size_t cnt = 0;
    bool discard = false;
    while(1) {
        // Wait to be notified that there has been a UART irq. (it may have been rx or tx so may not be anything to read)
        uint32_t ulNotificationValue = ulTaskNotifyTake( pdTRUE, waitms );

        if( ulNotificationValue != 1 ) {
            /* The call to ulTaskNotifyTake() timed out. check anyway */
        }

        size_t n = read_uart3(rx_buf, sizeof(rx_buf));
        if(n > 0) {
           process_command_buffer(n, rx_buf, &os, line, cnt, discard);
        }
    }
}
#endif

// this prints the string to all consoles that are connected and active
// must be called in command thread context
void print_to_all_consoles(const char *str)
{
    for(auto i : output_streams) {
        i->puts(str);
    }
}

static void handle_query(bool need_done)
{
    // set in comms thread, and executed in the command thread to avoid thread clashes.
    // the trouble with this is that ? does not reply if a long command is blocking call to dispatch_line
    // test commands for instance or a long line when the queue is full or G4 etc
    // so long as safe_sleep() is called then this will still be processed
    // also dispatch any instant queries we have recieved
    while(!queries.empty()) {
        struct query_t q= queries.pop_front();
        if(q.query_line == nullptr) { // it is a ? query
            std::string r;
            Robot::getInstance()->get_query_string(r);
            q.query_os->puts(r.c_str());

        } else {
            Dispatcher::getInstance()->dispatch(q.query_line, *q.query_os);
            free(q.query_line);
        }
        // on last one (Does presume they are the same os though)
        // FIXME may not work as expected when there are multiple I/O channels and output streams
        if(need_done && queries.empty()) q.query_os->set_done();
    }
}

/*
 * All commands must be executed in the context of this thread. It is equivalent to the main_loop in v1.
 * Commands are sent to this thread via the message queue from things that can block (like I/O)
 * Other things can call dispatch_line direct from the in_command_ctx call.
 */
static void command_handler()
{
    printf("DEBUG: Command thread running\n");

    for(;;) {
        char *line;
        OutputStream *os= nullptr;
        bool idle = false;

        // This will timeout after 100 ms
        if(receive_message_queue(&line, &os)) {
            //printf("DEBUG: got line: %s\n", line);
            dispatch_line(*os, line);
            handle_query(false);
            os->set_done(); // set after all possible output

        } else {
            // timed out or other error
            idle = true;
            if(config_error_msg.empty()) {
                // toggle led to show we are alive, but idle
                Board_LED_Toggle(0);
            }
            handle_query(true);
        }

        // call in_command_ctx for all modules that want it
        // dispatch_line can be called from that
        Module::broadcast_in_commmand_ctx(idle);

        // we check the queue to see if it is ready to run
        // we specifically deal with this in append_block, but need to check for other places
        if(Conveyor::getInstance() != nullptr) {
            Conveyor::getInstance()->check_queue();
        }
    }
}

// called only in command thread context, it will sleep (and yield) thread but will also
// process things like instant query
void safe_sleep(uint32_t ms)
{
    // here we need to sleep (and yield) for 10ms then check if we need to handle the query command
    TickType_t delayms = pdMS_TO_TICKS(10); // 10 ms sleep
    while(ms > 0) {
        vTaskDelay(delayms);
        // presumably there is a long running command that
        // may need Outputstream which will set done flag when it is done
        handle_query(false);

        if(ms > 10) {
            ms -= 10;
        } else {
            break;
        }
    }
}

#include "CommandShell.h"
#include "SlowTicker.h"
#include "FastTicker.h"
#include "StepTicker.h"
#include "ConfigReader.h"
#include "Switch.h"
#include "Planner.h"
#include "Robot.h"
#include "KillButton.h"
#include "Extruder.h"
#include "TemperatureControl.h"
#include "Adc.h"
#include "Pwm.h"
#include "CurrentControl.h"
#include "Laser.h"
#include "Endstops.h"
#include "ZProbe.h"
#include "Player.h"

extern void configureSPIFI();
//float get_pll1_clk();

#define SD_CONFIG

#ifndef SD_CONFIG
#include STRING_CONFIG_H
static std::string str(string_config);
static std::stringstream ss(str);
#else
extern "C" bool setup_sdmmc();
#endif

// voltage monitors
static std::map<std::string, Adc*> voltage_monitors;

float get_voltage_monitor(const char* name)
{
    auto p= voltage_monitors.find(name);
    if(p == voltage_monitors.end()) return 0;
    return p->second->read_voltage();
}

int get_voltage_monitor_names(const char *names[])
{
    int i= 0;
    for(auto& p : voltage_monitors) {
        if(names != nullptr)
            names[i]= p.first.c_str();
        ++i;
    }
    return i;
}

// this is used to add callback functions to be called once the system is running
static std::vector<StartupFunc_t> startup_fncs;
void register_startup(StartupFunc_t sf)
{
    startup_fncs.push_back(sf);
}

static void smoothie_startup(void *)
{
    printf("INFO: Smoothie V2.alpha Build for %s - starting up\n", BUILD_TARGET);
    //get_pll1_clk();

    // led 4 indicates boot phase 2 starts
    Board_LED_Set(3, true);

    // create the SlowTicker here as it is used by some modules
    SlowTicker *slow_ticker = new SlowTicker();

    // create the FastTicker here as it is used by some modules
    FastTicker *fast_ticker = new FastTicker();

    // create the StepTicker, don't start it yet
    StepTicker *step_ticker = new StepTicker();
#ifdef DEBUG
    // when debug is enabled we cannot run stepticker at full speed
    step_ticker->set_frequency(10000); // 10KHz
#else
    step_ticker->set_frequency(150000); // 150KHz
#endif
    step_ticker->set_unstep_time(1); // 1us step pulse by default

    // configure the Dispatcher
    new Dispatcher();

    bool ok = false;

    // open the config file
    do {
#ifdef SD_CONFIG
        static FATFS fatfs; /* File system object */
        if(!setup_sdmmc()) {
            std::cout << "Error: setting up sdmmc\n";
            break;
        }

        // TODO check the card is inserted

        int ret = f_mount(&fatfs, "sd", 1);
        if(FR_OK != ret) {
            std::cout << "Error: mounting: " << "/sd: " << ret << "\n";
            break;
        }

        std::fstream fs;
        fs.open("/sd/config.ini", std::fstream::in);
        if(!fs.is_open()) {
            std::cout << "Error: opening file: " << "/sd/config.ini" << "\n";
            // unmount sdcard
            //f_unmount("sd");
            break;
        }


        ConfigReader cr(fs);
        printf("DEBUG: Starting configuration of modules from sdcard...\n");
#else
        ConfigReader cr(ss);
        printf("DEBUG: Starting configuration of modules from memory...\n");
#endif

        {
            // get general system settings
            ConfigReader::section_map_t m;
            if(cr.get_section("general", m)) {
                bool f = cr.get_bool(m, "grbl_mode", false);
                THEDISPATCHER->set_grbl_mode(f);
                printf("INFO: grbl mode %s\n", f ? "set" : "not set");
                config_override= cr.get_bool(m, "config-override", false);
                printf("INFO: use config override is %s\n", config_override ? "set" : "not set");
                rpi_port_enabled= cr.get_bool(m, "rpi_port_enable", false);
                rpi_baudrate= cr.get_int(m, "rpi_baudrate", 115200);
                printf("INFO: rpi port is %senabled, at baudrate: %lu\n", rpi_port_enabled ? "" : "not ", rpi_baudrate);
                std::string p = cr.get_string(m, "aux_play_led", "nc");
                aux_play_led = new Pin(p.c_str(), Pin::AS_OUTPUT);
                if(!aux_play_led->connected()) {
                    delete aux_play_led;
                    aux_play_led = nullptr;
                }else{
                    printf("INFO: auxilliary play led set to %s\n", aux_play_led->to_string().c_str());
                }
            }
        }

        printf("DEBUG: configure the planner\n");
        Planner *planner = new Planner();
        planner->configure(cr);

        printf("DEBUG: configure the conveyor\n");
        Conveyor *conveyor = new Conveyor();
        conveyor->configure(cr);

        printf("DEBUG: configure the robot\n");
        Robot *robot = new Robot();
        if(!robot->configure(cr)) {
            printf("ERROR: Configuring robot failed\n");
            break;
        }

        ///////////////////////////////////////////////////////////
        // configure core modules here
        {
            // Pwm needs to be initialized, there can only be one frequency
            // needs to be done before any module that could use it
            uint32_t freq = 10000; // default is 10KHz
            ConfigReader::section_map_t m;
            if(cr.get_section("pwm", m)) {
                freq = cr.get_int(m, "frequency", freq);
            }
            Pwm::setup(freq);
            printf("INFO: PWM frequency set to %lu Hz\n", freq);
        }

        {
            printf("DEBUG: configure extruder\n");
            // this creates any configured extruders then we can remove it
            Extruder ex("extruder loader");
            if(!ex.configure(cr)) {
                printf("INFO: no Extruders loaded\n");
            }
        }

        {
            printf("DEBUG: configure temperature control\n");
            if(Adc::setup()) {
                // this creates any configured temperature controls
                if(!TemperatureControl::load_controls(cr)) {
                    printf("INFO: no Temperature Controls loaded\n");
                }
            } else {
                printf("ERROR: ADC failed to setup\n");
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////
        // create all registered modules, the addresses are stored in a known location in flash
        extern uint32_t __registered_modules_start;
        extern uint32_t __registered_modules_end;
        uint32_t *g_pfnModules= &__registered_modules_start;
        while (g_pfnModules < &__registered_modules_end) {
            uint32_t *addr= g_pfnModules++;
            bool (*pfnModule)(ConfigReader& cr)= (bool (*)(ConfigReader& cr))*addr;
            // this calls the registered create function for the module
            pfnModule(cr);
        }

        // end of module creation and configuration
        ////////////////////////////////////////////////////////////////

        {
            // configure voltage monitors if any
            ConfigReader::section_map_t m;
            if(cr.get_section("voltage monitor", m)) {
                for(auto& s : m) {
                    std::string k = s.first;
                    std::string v = s.second;

                    Adc *padc= new Adc;
                    if(padc->from_string(v.c_str()) == nullptr) {
                        printf("WARNING: Failed to create %s voltage monitor\n", k.c_str());
                        delete padc;
                    }else{
                        voltage_monitors[k]= padc;
                        printf("DEBUG: added voltage monitor %s: %s\n", k.c_str(), v.c_str());
                    }
                }
            }
        }
#ifdef SD_CONFIG
        // close the file stream
        fs.close();

        // unmount sdcard
        //f_unmount("sd");
#endif

        // initialize planner before conveyor this is when block queue is created
        // which needs to know how many actuators there are, which it gets from robot
        if(!planner->initialize(robot->get_number_registered_motors())) {
            printf("FATAL: planner failed to initialize, out of memory?\n");
            break;
        }

        // start conveyor last
        conveyor->start();

        printf("DEBUG: ...Ending configuration of modules\n");
        ok = true;
    } while(0);

    // create the commandshell, it is dependent on some of the above
    CommandShell *shell = new CommandShell();
    shell->initialize();

    if(ok) {
        // start the timers
        if(!slow_ticker->start()) {
            printf("Error: failed to start SlowTicker\n");
        }

        if(!fast_ticker->start()) {
            printf("WARNING: failed to start FastTicker (maybe nothing is using it?)\n");
        }

        if(!step_ticker->start()) {
            printf("Error: failed to start StepTicker\n");
        }

        if(!Adc::start()) {
            printf("Error: failed to start ADC\n");
        }

    } else {
        puts("ERROR: Configure failed\n");
        config_error_msg= "There was a fatal error in the config.ini this must be fixed to continue\nOnly some shell commands are allowed and sdcard access\n";
        Module::broadcast_halt(true);
        puts(config_error_msg.c_str());
    }

    // create queue for incoming buffers from the I/O ports
    if(!create_message_queue()) {
        // Failed to create the queue.
        printf("Error: failed to create comms i/o queue\n");
    }

    // Start comms threads Higher priority than the command thread
    // fixed stack size of 4k Bytes each
    xTaskCreate(usb_comms, "USBCommsThread", 1500/4, NULL, (tskIDLE_PRIORITY + 3UL), (TaskHandle_t *) NULL);
    xTaskCreate(uart_comms, "UARTCommsThread", 1500/4, NULL, (tskIDLE_PRIORITY + 3UL), (TaskHandle_t *) NULL);

    // run any startup functions that have been registered
    for(auto f : startup_fncs) {
        f();
    }
    startup_fncs.clear();
    startup_fncs.shrink_to_fit();

#ifdef BOARD_PRIMEALPHA
    if(rpi_port_enabled) {
        if(setup_uart3(rpi_baudrate) < 0) {
            printf("ERROR: UART3/RPI setup failed\n");
        } else {
            xTaskCreate(uart3_comms, "UART3CommsThread", 1500/4, NULL, (tskIDLE_PRIORITY + 3UL), (TaskHandle_t *) NULL
                );
        }
    }
#endif

    struct mallinfo mi = mallinfo();
    printf("DEBUG: Initial: free malloc memory= %d, free sbrk memory= %d, Total free= %d\n", mi.fordblks, xPortGetFreeHeapSize() - mi.fordblks, xPortGetFreeHeapSize());

    // indicate we are up and running
    system_running= true;

    // load config override if set
    if(ok && config_override) {
        OutputStream os(&std::cout);
        if(load_config_override(os)) {
            os.printf("INFO: configuration override loaded\n");

        }else{
            os.printf("INFO: No saved configuration override\n");
        }
    }

    // led 3,4 off indicates boot phase 2 complete
    Board_LED_Set(2, false);
    Board_LED_Set(3, false);

    // run the command handler in this thread
    command_handler();

    // does not return from above
}

int main(int argc, char *argv[])
{
    NVIC_SetPriorityGrouping( 0 );

    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();

    // Set up and initialize all required blocks and
    // functions related to the board hardware
    Board_Init();

    if(setup_uart() < 0) {
        printf("FATAL: UART setup failed\n");
    }

#ifndef FLASH16BIT
    configureSPIFI(); // setup the winbond SPIFI to max speed
#endif

    printf("MCU clock rate= %lu Hz\n", SystemCoreClock);

    StopWatch_Init();
    printf("StopWatch clock rate= %lu Hz\n", StopWatch_TicksPerSecond());

    // led 4 indicates boot phase 1 complete
    Board_LED_Set(3, true);

    // launch the startup thread which will become the command thread that executes all incoming commands
    // 10000 Bytes stack
    xTaskCreate(smoothie_startup, "CommandThread", 10000/4, NULL, (tskIDLE_PRIORITY + 2UL), (TaskHandle_t *) NULL);

    /* Start the scheduler */
    vTaskStartScheduler();

    // never gets here
    return 1;
}

#define TICKS2MS( xTicks ) ( ((xTicks) * 1000.0F) / configTICK_RATE_HZ )

// hooks from freeRTOS
extern "C" void vApplicationIdleHook( void )
{
    static TickType_t last_time_check = xTaskGetTickCount();
    if(TICKS2MS(xTaskGetTickCount() - last_time_check) >= 300) {
        last_time_check = xTaskGetTickCount();
        if(!config_error_msg.empty()) {
            // handle config error
            // flash both leds
            Board_LED_Toggle(0);
            Board_LED_Toggle(1);
        } else {
            // handle play led 1 and aux play led
            if(system_running) {
                if(Module::is_halted()) {
                    Board_LED_Toggle(1);
                    if(aux_play_led != nullptr) {
                        aux_play_led->set(!aux_play_led->get());
                    }
                }else{
                    Board_LED_Set(1, !Conveyor::getInstance()->is_idle());
                    if(aux_play_led != nullptr) {
                        aux_play_led->set(!Conveyor::getInstance()->is_idle());
                    }
                }
            }
        }
    }
}

extern "C" void vApplicationTickHook( void )
{
    /* This function will be called by each tick interrupt if
    configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
    added here, but the tick hook is called from an interrupt context, so
    code must not attempt to block, and only the interrupt safe FreeRTOS API
    functions can be used (those that end in FromISR()). */
}

extern "C" void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    taskDISABLE_INTERRUPTS();
    Board_LED_Set(0, true);
    Board_LED_Set(1, false);
    Board_LED_Set(2, true);
    Board_LED_Set(3, true);
   __asm("bkpt #0");
    for( ;; );
}


extern "C" void vApplicationMallocFailedHook( void )
{
    /* vApplicationMallocFailedHook() will only be called if
    configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
    function that will get called if a call to pvPortMalloc() fails.
    pvPortMalloc() is called internally by the kernel whenever a task, queue,
    timer or semaphore is created.  It is also called by various parts of the
    demo application.  If heap_1.c or heap_2.c are used, then the size of the
    heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
    FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
    to query the size of free heap space that remains (although it does not
    provide information on how the remaining heap might be fragmented). */
    #if 0
    taskDISABLE_INTERRUPTS();
    __asm("bkpt #0");
    for( ;; );
    #else
    // we don't want to use any memory for this
    // returns NULL to the caller
    write_uart("FATAL: malloc/sbrk out of memory\n", 33);
    return;
    #endif
}

extern "C" void HardFault_Handler(void) {
    Board_LED_Set(0, true);
    Board_LED_Set(1, true);
    Board_LED_Set(2, false);
    Board_LED_Set(3, false);
    __asm("bkpt #0");
    for( ;; );
}
