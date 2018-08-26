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

#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"

#include "Module.h"
#include "OutputStream.h"
#include "MessageQueue.h"

#include "GCode.h"
#include "GCodeProcessor.h"
#include "Dispatcher.h"
#include "Robot.h"

//set in uart thread to signal command_thread to print a query response
static bool do_query = false;
static OutputStream *query_os = nullptr;

// set to true when M28 is in effect
static bool uploading = false;
static FILE *upload_fp = nullptr;

// TODO maybe move to Dispatcher
static GCodeProcessor gp;

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
                    i.dump(upload_fp);
                }

                os.printf("ok\n");
                return true;
            }

            // if this is a multi gcode line then dispatch must not send ok unless this is the last one
            if(!THEDISPATCHER->dispatch(i, os, ngcodes == 1)) {
                // no handler processed this gcode, return ok - ignored
                if(ngcodes == 1) os.puts("ok - ignored\n");
            }

        } else {
            // if it has neither g or m then it was a blank line or comment
            os.puts("ok\n");
        }
        --ngcodes;
    }

    return true;
}

#include <functional>
static std::function<void(char)> capture_fnc;
void set_capture(std::function<void(char)> cf)
{
    capture_fnc = cf;
}

#include <vector>
static std::vector<OutputStream*> output_streams;

// this is here so we do not need duplicate this logic for USB and UART
static void process_buffer(size_t n, char *rxBuf, OutputStream *os, char *line, size_t& cnt, bool& discard)
{
    for (size_t i = 0; i < n; ++i) {
        line[cnt] = rxBuf[i];
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
            query_os = os; // we need to let it know where to send response back to TODO maybe a race condition if both USB and uart send ?
            do_query = true;

        } else if(discard) {
            // we discard long lines until we get the newline
            if(line[cnt] == '\n') discard = false;

        } else if(cnt >= MAX_LINE_LENGTH - 1) {
            // discard long lines
            discard = true;
            cnt = 0;
            os->puts("error:Discarding long line\n");

        } else if(line[cnt] == '\n') {
            line[cnt] = '\0'; // remove the \n and nul terminate
            send_message_queue(line, os);
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
    printf("USB Comms thread running\n");

    if(!setup_cdc(xTaskGetCurrentTaskHandle())) {
        printf("FATAL: CDC setup failed\n");
        return;
    }

    // on first connect we send a welcome message after getting a '\n'
    static const char *welcome_message = "Welcome to Smoothie\nok\n";
    const TickType_t waitms = pdMS_TO_TICKS( 300 );

    size_t n;
    char rxBuf[256];
    bool done = false;

    // first we wait for an initial '\n' sent from host
    while (!done) {
        // Wait to be notified that there has been a USB irq.
        ulTaskNotifyTake( pdTRUE, waitms );
        n = read_cdc(rxBuf, sizeof(rxBuf));
        if(n > 0) {
            for (size_t i = 0; i < n; ++i) {
                if(rxBuf[i] == '\n') {
                    write_cdc(welcome_message, strlen(welcome_message));
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

        n = read_cdc(rxBuf, sizeof(rxBuf));
        if(n > 0) {
            process_buffer(n, rxBuf, &os, line, cnt, discard);
        }
    }
}

#if 0
static void uart_comms(void *)
{
    printf("UART Comms thread running\n");

    // create an output stream that writes to the uart
    static OutputStream os([](const char *buf, size_t len) { return write_uart(buf, len); });
    output_streams.push_back(&os);

    char line[MAX_LINE_LENGTH];
    size_t cnt = 0;
    bool discard = false;
    while(1) {
        // Wait to be notified that there has been a UART irq.
        uint32_t ulNotificationValue = ulTaskNotifyTake( pdTRUE, waitms );

        if( ulNotificationValue != 1 ) {
            /* The call to ulTaskNotifyTake() timed out. check anyway */
        }

        size_t n = read_uart(rxBuf, sizeof(rxBuf));
        if(n > 0) {
           process_buffer(n, rxBuf, line, cnt, discard);
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

#include "Conveyor.h"
#include "Pin.h"

// Define the activity/idle indicator led
static Pin *idle_led = nullptr;

/*
 * All commands must be executed in the context of this thread. It is equivalent to the main_loop in v1.
 * Commands are sent to this thread via the message queue from things that can block (like I/O)
 * How to queue things from interupts like from Switch?
 * 1. We could have a timeout on the I/O queue of about 100-200ms and check an internal queue for commands
 * 2. we could call a on_main_loop to all registed modules.
 * Not fond of 2 and 1 requires somw form of locking so interrupts can access the queue too.
 */
static void commandthrd(void *)
{
    printf("Command thread running\n");
    // {
    //     // Manual unlocking is done before notifying, to avoid waking up
    //     // the waiting thread only to block again (see notify_one for details)
    //     std::unique_lock<std::mutex> lk(m);
    //     lk.unlock();
    //     cv.notify_one();
    // }

    for(;;) {
        char *line;
        OutputStream *os;
        bool idle = false;

        // This will timeout after 200 ms
        if(receive_message_queue(&line, &os)) {
            //printf("DEBUG: got line: %s\n", line);
            dispatch_line(*os, line);

        } else {
            // timed out or other error
            idle = true;
            if(idle_led != nullptr) {
                // toggle led to show we are alive, but idle
                idle_led->set(!idle_led->get());
            }
        }

        // set in comms thread, and executed here to avoid thread clashes
        // the trouble with this is that ? does not reply if a long command is blocking above call to dispatch_line
        // test commands for instance or a long line when the queue is full or G4 etc
        // so long as safe_sleep() is called then this will still be handled
        if(do_query) {
            std::string r;
            Robot::getInstance()->get_query_string(r);
            query_os->puts(r.c_str());
            do_query = false;
        }

        // call in_command_ctx for all modules that want it
        Module::broadcast_in_commmand_ctx(idle);

        // we check the queue to see if it is ready to run
        // we specifically deal with this in append_block, but need to check for other places
        // This used to be done in on_idle which never blocked
        Conveyor::getInstance()->check_queue();
    }
}

// called only in command thread context, it will sleep (and yield) thread but will also
// process things like query
void safe_sleep(uint32_t ms)
{
    // here we need to sleep (and yield) for 10ms then check if we need to handle the query command
    TickType_t delayms = pdMS_TO_TICKS(10); // 10 ms sleep
    while(ms > 0) {
        vTaskDelay(delayms);
        if(do_query) {
            std::string r;
            Robot::getInstance()->get_query_string(r);
            query_os->puts(r.c_str());
            do_query = false;
        }

        if(ms > 10) {
            ms -= 10;
        } else {
            break;
        }
    }
}

#include "CommandShell.h"
#include "SlowTicker.h"
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

#include "main.h"
#include <fstream>

void configureSPIFI();
//float get_pll1_clk();

//#define SD_CONFIG

#ifndef SD_CONFIG
#include STRING_CONFIG_H
static std::string str(string_config);
static std::stringstream ss(str);
#else
extern "C" bool setup_sdmmc();
#endif

static void smoothie_startup()
{
    printf("Smoothie V2.alpha Build for %s - starting up\n", BUILD_TARGET);
    //get_pll1_clk();

    // create the SlowTicker here as it is used by some modules
    SlowTicker *slow_ticker = new SlowTicker();

    // create the StepTicker, don't start it yet
    StepTicker *step_ticker = new StepTicker();
#ifdef DEBUG
    // when debug is enabled we cannot run stepticker at full speed
    step_ticker->set_frequency(10000); // 10KHz
#else
    step_ticker->set_frequency(100000); // 100KHz
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
            std::cout << "Error setting up sdmmc\n";
            break;
        }
        int ret =
        f_mount(&fatfs, "sd", 1);
        if(FR_OK != ret) {
            std::cout << "Error mounting: " << "/sd: " << ret << "\n";
            break;
        }

        std::fstream fs;
        fs.open("/sd/config.ini", std::fstream::in);
        if(!fs.is_open()) {
            std::cout << "Error opening file: " << "/sd/config.ini" << "\n";
            // unmount sdcard
            f_unmount("sd");
            break;
        }


        ConfigReader cr(fs);
        printf("Starting configuration of modules from sdcard...\n");
#else
        ConfigReader cr(ss);
        printf("Starting configuration of modules from memory...\n");
#endif

        {
            // get general system settings
            ConfigReader::section_map_t m;
            if(cr.get_section("general", m)) {
                bool f = cr.get_bool(m, "grbl_mode", false);
                THEDISPATCHER->set_grbl_mode(f);
                printf("grbl mode %s\n", f ? "set" : "not set");
            }
        }

        printf("configure the planner\n");
        Planner *planner = new Planner();
        planner->configure(cr);

        printf("configure the conveyor\n");
        Conveyor *conveyor = new Conveyor();
        conveyor->configure(cr);

        printf("configure the robot\n");
        Robot *robot = new Robot();
        if(!robot->configure(cr)) {
            printf("ERROR: Configuring robot failed\n");
            break;
        }

        ///////////////////////////////////////////////////////////
        // configure other modules here

        {
            printf("configure switches\n");
            // this creates any configured switches then we can remove it
            Switch switches("switch loader");
            if(!switches.configure(cr)) {
                printf("INFO: no switches loaded\n");
            }
        }

        {
            printf("configure extruder\n");
            // this creates any configured extruders then we can remove it
            Extruder ex("extruder loader");
            if(!ex.configure(cr)) {
                printf("INFO: no Extruders loaded\n");
            }
        }

        {
            printf("configure temperature control\n");
            if(Adc::setup()) {
                // this creates any configured temperature controls then we can remove it
                TemperatureControl tc("temperature control loader");
                if(!tc.configure(cr)) {
                    printf("INFO: no Temperature Controls loaded\n");
                }
            } else {
                printf("ERROR: ADC failed to setup\n");
            }
        }

        printf("configure endstops\n");
        Endstops *endstops = new Endstops();
        if(!endstops->configure(cr)) {
            printf("INFO: No endstops enabled\n");
            delete endstops;
            endstops = nullptr;
        }

        printf("configure kill button\n");
        KillButton *kill_button = new KillButton();
        if(!kill_button->configure(cr)) {
            printf("INFO: No kill button enabled\n");
            delete kill_button;
            kill_button = nullptr;
        }

        {
            // Pwm needs to be initialized, there can only be one frequency
            float freq = 10000; // default is 10KHz
            ConfigReader::section_map_t m;
            if(cr.get_section("pwm", m)) {
                freq = cr.get_float(m, "frequency", 10000);
            }
            Pwm::setup(freq);
            printf("INFO: PWM frequency set to %f Hz\n", freq);
        }

        printf("configure current control\n");
        CurrentControl *current_control = new CurrentControl();
        if(!current_control->configure(cr)) {
            printf("INFO: No current controls configured\n");
            delete current_control;
            current_control = nullptr;
        }

        printf("configure laser\n");
        Laser *laser = new Laser();
        if(!laser->configure(cr)) {
            printf("INFO: No laser configured\n");
            delete laser;
            laser = nullptr;
        }

        printf("configure zprobe\n");
        ZProbe *zprobe = new ZProbe();
        if(!zprobe->configure(cr)) {
            printf("INFO: No ZProbe configured\n");
            delete zprobe;
            zprobe = nullptr;
        }

        printf("configure player\n");
        Player *player = new Player();
        if(!player->configure(cr)) {
            printf("WARNING: Failed to configure Player\n");
        }

        {
            // configure system leds (if any)
            ConfigReader::section_map_t m;
            if(cr.get_section("system leds", m)) {
                std::string p = cr.get_string(m, "idle_led", "nc");
                idle_led = new Pin(p.c_str(), Pin::AS_OUTPUT);
                if(!idle_led->connected()) {
                    delete idle_led;
                    idle_led = nullptr;
                }
            }
        }


        // end of module creation and configuration
        ////////////////////////////////////////////////////////////////

#ifdef SD_CONFIG
        // close the file stream
        fs.close();

        // unmount sdcard
        //f_unmount("sd");
#endif

        // initialize planner before conveyor this is when block queue is created
        // which needs to know how many actuators there are, which it gets from robot
        if(!planner->initialize(robot->get_number_registered_motors())) {
            printf("ERROR: planner failed to initialize, out of memory?\n");
            break;
        }

        // start conveyor last
        conveyor->start();

        printf("...Ending configuration of modules\n");
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

        if(!step_ticker->start()) {
            printf("Error: failed to start StepTicker\n");
        }

        if(!Adc::start()) {
            printf("Error: failed to start ADC\n");
        }
    }

    // create queue for incoming buffers from the I/O ports
    if(!create_message_queue()) {
        // Failed to create the queue.
        printf("Error: failed to create comms i/o queue\n");
    }


    // launch the command thread that executes all incoming commands
    // 10000 Bytes stack
    xTaskCreate(commandthrd, "CommandThread", 10000/4, NULL, (tskIDLE_PRIORITY + 3UL), (TaskHandle_t *) NULL);

    // Start comms threads Higher priority than the command thread
    // fixed stack size of 4k Bytes each
    xTaskCreate(usb_comms, "USBCommsThread", 4096/4, NULL, (tskIDLE_PRIORITY + 4UL), (TaskHandle_t *) NULL);
    //xTaskCreate(uart_comms, "UARTCommsThread", 4096/4, NULL, (tskIDLE_PRIORITY + 4UL), (TaskHandle_t *) NULL);

    // wait for command thread to start
    // std::unique_lock<std::mutex> lk(m);
    // cv.wait(lk);
    // printf("Command thread started\n");

    struct mallinfo mi = mallinfo();
    printf("Initial: free malloc memory= %d, free sbrk memory= %d, Total free= %d\n", mi.fordblks, xPortGetFreeHeapSize() - mi.fordblks, xPortGetFreeHeapSize());

    /* Start the scheduler */
    vTaskStartScheduler();

}

int main(int argc, char *argv[])
{
    //HAL_NVIC_SetPriorityGrouping( NVIC_PRIORITYGROUP_4 );
    NVIC_SetPriorityGrouping( 0 );

    configureSPIFI(); // setup the winbond SPIFI to max speed

    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();

    // Set up and initialize all required blocks and
    // functions related to the board hardware
    Board_Init();
    // Set the LED to the state of "On"
    Board_LED_Set(0, true);

    printf("MCU clock rate= %lu Hz\n", SystemCoreClock);
    smoothie_startup();
    return 1;
}

// hooks from freeRTOS
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
    __asm("bkpt #0");
    for( ;; );
}

extern "C" void vApplicationIdleHook( void )
{
    /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
    to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
    task.  It is essential that code added to this hook function never attempts
    to block in any way (for example, call xQueueReceive() with a block time
    specified, or call vTaskDelay()).  If the application makes use of the
    vTaskDelete() API function (as this demo application does) then it is also
    important that vApplicationIdleHook() is permitted to return to its calling
    function, because it is the responsibility of the idle task to clean up
    memory allocated by the kernel to any task that has since been deleted. */
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
    taskDISABLE_INTERRUPTS();
    __asm("bkpt #0");
    for( ;; );
}
