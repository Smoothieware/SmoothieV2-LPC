#include "CommandShell.h"
#include "OutputStream.h"
#include "Dispatcher.h"
#include "Module.h"
#include "StringUtils.h"
#include "Robot.h"
#include "AutoPushPop.h"
#include "StepperMotor.h"
#include "main.h"
#include "TemperatureControl.h"
#include "ConfigWriter.h"
#include "Conveyor.h"
#include "version.h"
#include "ymodem.h"
#include "Adc.h"
#include "FastTicker.h"
#include "StepTicker.h"
#include "Adc.h"
#include "GCodeProcessor.h"

#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include "stopwatch.h"

#include <functional>
#include <set>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <malloc.h>

#define HELP(m) if(params == "-h") { os.printf("%s\n", m); return true; }

CommandShell::CommandShell()
{
    mounted = false;
}

bool CommandShell::initialize()
{
    // register command handlers
    using std::placeholders::_1;
    using std::placeholders::_2;

    THEDISPATCHER->add_handler( "help", std::bind( &CommandShell::help_cmd, this, _1, _2) );

    //THEDISPATCHER->add_handler( "mount", std::bind( &CommandShell::mount_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "ls", std::bind( &CommandShell::ls_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "rm", std::bind( &CommandShell::rm_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "mv", std::bind( &CommandShell::mv_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "cp", std::bind( &CommandShell::cp_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "cd", std::bind( &CommandShell::cd_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "mkdir", std::bind( &CommandShell::mkdir_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "cat", std::bind( &CommandShell::cat_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "md5sum", std::bind( &CommandShell::md5sum_cmd, this, _1, _2) );

    THEDISPATCHER->add_handler( "config-set", std::bind( &CommandShell::config_set_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "config-get", std::bind( &CommandShell::config_get_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "ry", std::bind( &CommandShell::ry_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "truncate", std::bind( &CommandShell::truncate_cmd, this, _1, _2) );

    THEDISPATCHER->add_handler( "mem", std::bind( &CommandShell::mem_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "switch", std::bind( &CommandShell::switch_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "gpio", std::bind( &CommandShell::gpio_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "modules", std::bind( &CommandShell::modules_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "get", std::bind( &CommandShell::get_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$#", std::bind( &CommandShell::grblDP_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$G", std::bind( &CommandShell::grblDG_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$I", std::bind( &CommandShell::grblDG_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$H", std::bind( &CommandShell::grblDH_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$S", std::bind( &CommandShell::switch_poll_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "$J", std::bind( &CommandShell::jog_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "test", std::bind( &CommandShell::test_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "version", std::bind( &CommandShell::version_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "break", std::bind( &CommandShell::break_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "reset", std::bind( &CommandShell::reset_cmd, this, _1, _2) );
    THEDISPATCHER->add_handler( "flash", std::bind( &CommandShell::flash_cmd, this, _1, _2) );

    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 20, std::bind(&CommandShell::m20_cmd, this, _1, _2));
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 115, std::bind(&CommandShell::m115_cmd, this, _1, _2));

    return true;
}

// lists all the registered commands
bool CommandShell::help_cmd(std::string& params, OutputStream& os)
{
    HELP("Show available commands");
    auto cmds = THEDISPATCHER->get_commands();
    for(auto& i : cmds) {
        os.printf("%s\n", i.c_str());
        // Display the help string for each command
        //    std::string cmd(i);
        //    cmd.append(" -h");
        //    THEDISPATCHER->dispatch(cmd.c_str(), os);
    }
    os.puts("\nuse cmd -h to get help on that command\n");

    return true;
}


bool CommandShell::m20_cmd(GCode& gcode, OutputStream& os)
{
    if(THEDISPATCHER->is_grbl_mode()) return false;

    os.printf("Begin file list\n");
    std::string params("-1 /sd");
    ls_cmd(params, os);
    os.printf("End file list\n");
    return true;
}

bool CommandShell::ls_cmd(std::string& params, OutputStream& os)
{
    HELP("list files: dir [-1] [folder]");
    std::string path;
    std::string opts;
    while(!params.empty()) {
        std::string s = stringutils::shift_parameter( params );
        if(s.front() == '-') {
            opts.append(s);
        } else {
            path = s;
            if(!params.empty()) {
                path.append(" ");
                path.append(params);
            }
            break;
        }
    }

#if 0
    DIR *d;
    struct dirent *p;
    d = opendir(path.c_str());
    if (d != NULL) {
        while ((p = readdir(d)) != NULL) {
            if(Module::is_halted()) break;
            os.printf("%s", p->d_name);
            struct stat buf;
            std::string sp = path + "/" + p->d_name;
            if (stat(sp.c_str(), &buf) >= 0) {
                if (S_ISDIR(buf.st_mode)) {
                    os.printf("/");

                } else if(opts.find("-s", 0, 2) != std::string::npos) {
                    os.printf(" %d", buf.st_size);
                }
            } else {
                os.printf(" - Could not stat: %s", sp.c_str());
            }
            os.printf("\n");
        }
        closedir(d);
    } else {
        os.printf("Could not open directory %s\n", path.c_str());
    }
#else
    // newlib does not support dirent so use ff lib directly
    DIR dir;
    FILINFO finfo;
    FATFS *fs;
    FRESULT res = f_opendir(&dir, path.c_str());
    if(FR_OK != res) {
        os.printf("Could not open directory %s\n", path.c_str());
        return true;
    }

    DWORD p1, s1, s2;
    p1 = s1 = s2 = 0;
    bool simple = false;
    if(opts.find("-1", 0, 2) != std::string::npos) {
        simple = true;
    }

    for(;;) {
        //if(Module::is_halted()) {f_closedir(&dir); return true; }
        res = f_readdir(&dir, &finfo);
        if ((res != FR_OK) || !finfo.fname[0]) break;
        if(simple) {
            if(finfo.fattrib & AM_DIR) {
                os.printf("%s/\n", finfo.fname);
            } else {
                os.printf("%s\n", finfo.fname);
            }
        } else {
            if (finfo.fattrib & AM_DIR) {
                s2++;
            } else {
                s1++; p1 += finfo.fsize;
            }
            os.printf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  %s\n",
                      (finfo.fattrib & AM_DIR) ? 'D' : '-',
                      (finfo.fattrib & AM_RDO) ? 'R' : '-',
                      (finfo.fattrib & AM_HID) ? 'H' : '-',
                      (finfo.fattrib & AM_SYS) ? 'S' : '-',
                      (finfo.fattrib & AM_ARC) ? 'A' : '-',
                      (finfo.fdate >> 9) + 1980, (finfo.fdate >> 5) & 15, finfo.fdate & 31,
                      (finfo.ftime >> 11), (finfo.ftime >> 5) & 63,
                      (DWORD)finfo.fsize, finfo.fname);
        }
    }
    if(!simple) {
        os.printf("%4lu File(s),%10lu bytes total\n%4lu Dir(s)", s1, p1, s2);
        res = f_getfree("/sd", (DWORD*)&p1, &fs);
        if(FR_OK == res) {
            os.printf(", %10lu bytes free\n", p1 * fs->csize * 512);
        } else {
            os.printf("\n");
        }
        os.set_no_response();
    }
    f_closedir(&dir);
#endif
    return true;
}

bool CommandShell::rm_cmd(std::string& params, OutputStream& os)
{
    HELP("delete: file(s) or directory. quote names with spaces");
    std::string fn = stringutils::shift_parameter( params );
    while(!fn.empty()) {
        int s = remove(fn.c_str());
        if (s != 0){
            os.printf("Could not delete %s\n", fn.c_str());
        } else {
            os.printf("deleted %s\n", fn.c_str());
        }
        fn = stringutils::shift_parameter( params );
    }

    return true;
}

bool CommandShell::cd_cmd(std::string& params, OutputStream& os)
{
    HELP("change directory");
    std::string fn = stringutils::shift_parameter( params );
    if(fn.empty()) {
        fn= "/";
    }
    if(FR_OK != f_chdir(fn.c_str())) {
        os.puts("failed to change to directory\n");
    }
    return true;
}

bool CommandShell::mkdir_cmd(std::string& params, OutputStream& os)
{
    HELP("make directory");
    std::string fn = stringutils::shift_parameter( params );
    if(fn.empty()) {
        os.puts("directory name required\n");
        return true;
    }
    if(FR_OK != f_mkdir(fn.c_str())) {
        os.puts("failed to make directory\n");
    }
    return true;
}

bool CommandShell::mv_cmd(std::string& params, OutputStream& os)
{
    HELP("rename: from to");
    std::string fn1 = stringutils::shift_parameter( params );
    std::string fn2 = stringutils::shift_parameter( params );
    if(fn1.empty() || fn2.empty()) {
        os.puts("from and to files required\n");
        return true;
    }
    int s = rename(fn1.c_str(), fn2.c_str());
    if (s != 0) os.printf("Could not rename %s to %s\n", fn1.c_str(), fn2.c_str());
    return true;
}

bool CommandShell::cp_cmd(std::string& params, OutputStream& os)
{
    HELP("copy a file: from to");
    std::string fn1 = stringutils::shift_parameter( params );
    std::string fn2 = stringutils::shift_parameter( params );

    if(fn1.empty() || fn2.empty()) {
        os.puts("from and to files required\n");
        return true;
    }

    std::fstream fsin;
    std::fstream fsout;

    fsin.rdbuf()->pubsetbuf(0, 0); // set unbuffered
    fsout.rdbuf()->pubsetbuf(0, 0); // set unbuffered
    fsin.open(fn1, std::fstream::in | std::fstream::binary);
    if(!fsin.is_open()) {
        os.printf("File %s does not exist\n", fn1.c_str());
        return true;
    }

    fsout.open(fn2, std::fstream::out | std::fstream::binary | std::fstream::trunc);
    if(!fsout.is_open()) {
        os.printf("Could not open File %s for write\n", fn2.c_str());
        return true;
    }

    // allocate from heap rather than the limited stack
    char *buffer = (char *)malloc(4096);
    if(buffer != nullptr) {
        /* Copy source to destination */
        while (!fsin.eof()) {
            fsin.read(buffer, sizeof(buffer));
            int br = fsin.gcount();
            if(br > 0) {
                fsout.write(buffer, br);
                if(!fsout.good()) {
                    os.printf("Write failed to File %s\n", fn2.c_str());
                    break;
                }
            }
        }
        free(buffer);

    } else {
        os.printf("Not enough memory for operation\n");
    }

    /* Close open files */
    fsin.close();
    fsout.close();

    return true;
}

static void printTaskList(OutputStream& os)
{
    TaskStatus_t *pxTaskStatusArray;
    char cStatus;

    //vTaskSuspendAll();
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();

    /* Allocate an array index for each task. */
    pxTaskStatusArray = (TaskStatus_t *)malloc( uxArraySize * sizeof( TaskStatus_t ) );

    if( pxTaskStatusArray != NULL ) {
        /* Generate the (binary) data. */
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, NULL );

        /* Create a human readable table from the binary data. */
        for(UBaseType_t x = 0; x < uxArraySize; x++ ) {
            switch( pxTaskStatusArray[ x ].eCurrentState ) {
                case eRunning:      cStatus = 'X'; break;
                case eReady:        cStatus = 'R'; break;
                case eBlocked:      cStatus = 'B'; break;
                case eSuspended:    cStatus = 'S'; break;
                case eDeleted:      cStatus = 'D'; break;
                default:            cStatus = '?'; break;
            }

            /* Write the task name */
            os.printf("%12s ", pxTaskStatusArray[x].pcTaskName);

            /* Write the rest of the string. */
            os.printf(" %c %2u %6u %2u\n", cStatus, ( unsigned int ) pxTaskStatusArray[ x ].uxCurrentPriority, ( unsigned int ) pxTaskStatusArray[ x ].usStackHighWaterMark, ( unsigned int ) pxTaskStatusArray[ x ].xTaskNumber );
        }

        free( pxTaskStatusArray );
    }
    //xTaskResumeAll();
}

bool CommandShell::mem_cmd(std::string& params, OutputStream& os)
{
    HELP("show memory allocation and threads");

    printTaskList(os);
    // os->puts("\n\n");
    // vTaskGetRunTimeStats(pcWriteBuffer);
    // os->puts(pcWriteBuffer);

    struct mallinfo mem = mallinfo();
    os.printf("\n\nfree sbrk memory= %d, Total free= %d\n", xPortGetFreeHeapSize() - mem.fordblks, xPortGetFreeHeapSize());
    os.printf("malloc:      total       used       free    largest\n");
    os.printf("Mem:   %11d%11d%11d%11d\n", mem.arena, mem.uordblks, mem.fordblks, mem.ordblks);

    os.printf("RAM2: %lu used, %lu bytes free\n", _RAM2->get_size() - _RAM2->available(), _RAM2->available());
    os.printf("RAM3: %lu used, %lu bytes free\n", _RAM3->get_size() - _RAM3->available(), _RAM3->available());
    os.printf("RAM4: %lu used, %lu bytes free\n", _RAM4->get_size() - _RAM4->available(), _RAM4->available());
    os.printf("RAM5: %lu used, %lu bytes free\n", _RAM5->get_size() - _RAM5->available(), _RAM5->available());
    os.printf("Total available RAM: %lu\n", xPortGetFreeHeapSize() +
              _RAM2->available() + _RAM3->available() + _RAM4->available() + _RAM5->available());

    if(!params.empty()) {
        os.printf("-- RAM2 --\n"); _RAM2->debug(os);
        os.printf("-- RAM3 --\n"); _RAM3->debug(os);
        os.printf("-- RAM4 --\n"); _RAM4->debug(os);
        os.printf("-- RAM5 --\n"); _RAM5->debug(os);
    }

    os.set_no_response();
    return true;
}

#if 0
bool CommandShell::mount_cmd(std::string& params, OutputStream& os)
{
    HELP("mount sdcard on /sd (or unmount if already mounted)");

    const char g_target[] = "sd";
    if(mounted) {
        os.printf("Already mounted, unmounting\n");
        int ret = f_unmount("g_target");
        if(ret != FR_OK) {
            os.printf("Error unmounting: %d", ret);
            return true;
        }
        mounted = false;
        return true;
    }

    int ret = f_mount(f_mount(&fatfs, g_target, 1);
    if(FR_OK == ret) {
    mounted = true;
    os.printf("Mounted /%s\n", g_target);

    } else {
        os.printf("Failed to mount sdcard: %d\n", ret);
    }

    return true;
}
#endif

bool CommandShell::cat_cmd(std::string& params, OutputStream& os)
{
    HELP("display file: nnn option will show first nnn lines, -d will delay output and send ^D at end to terminate");
    // Get params ( filename and line limit )
    std::string filename          = stringutils::shift_parameter( params );
    std::string limit_parameter   = stringutils::shift_parameter( params );

    if(filename.empty()) {
        os.puts("file name required\n");
        return true;
    }

    bool delay = false;
    int limit = -1;

    if(!limit_parameter.empty() && limit_parameter.substr(0, 2) == "-d") {
        delay = true;
        limit_parameter = stringutils::shift_parameter( params );
    }

    if (!limit_parameter.empty()) {
        char *e = NULL;
        limit = strtol(limit_parameter.c_str(), &e, 10);
        if (e <= limit_parameter.c_str())
            limit = -1;
    }

    // Open file
    FILE *lp = fopen(filename.c_str(), "r");
    if (lp != NULL) {
        if(delay) {
            os.puts("you have 5 seconds to initiate the upload command on the host...\n");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
        char buffer[132];
        int newlines = 0;
        // Print each line of the file
        while (fgets (buffer, sizeof(buffer) - 1, lp) != nullptr) {
            os.puts(buffer);
            if ( limit > 0 && ++newlines >= limit ) {
                break;
            }
        };
        fclose(lp);
        if(delay) {
            char c = 26;
            os.write(&c, 1);
        }

    } else {
        os.printf("File not found: %s\n", filename.c_str());
    }

    os.set_no_response();
    return true;
}

#include "md5.h"
bool CommandShell::md5sum_cmd(std::string& params, OutputStream& os)
{
    HELP("calculate the md5sum of given filename");
    std::string filename          = stringutils::shift_parameter( params );

    if(filename.empty()) {
        os.puts("file name required\n");
        return true;
    }

    // Open file
    FILE *lp = fopen(filename.c_str(), "r");
    if (lp != NULL) {
        MD5 md5;
        uint8_t buf[64];
        do {
            size_t n = fread(buf, 1, sizeof buf, lp);
            if(n > 0) md5.update(buf, n);
        } while(!feof(lp));

        os.printf("%s %s\n", md5.finalize().hexdigest().c_str(), params.c_str());
        fclose(lp);

    } else {
        os.printf("File not found: %s\n", params.c_str());
    }

    os.set_no_response();
    return true;
}

#include "Switch.h"
// set or get switch state for a named switch
bool CommandShell::switch_cmd(std::string& params, OutputStream& os)
{
    HELP("list switches or get/set named switch. if 2nd parameter is on/off it sets state if it is numeric it sets value");

    std::string name = stringutils::shift_parameter( params );
    std::string value = stringutils::shift_parameter( params );

    if(name.empty()) {
        // just list all the switches
        std::vector<Module*> mv = Module::lookup_group("switch");
        if(mv.size() > 0) {
            for(auto m : mv) {
                Switch *s = static_cast<Switch*>(m);
                os.printf("%s:\n", m->get_instance_name());
                os.printf(" %s\n", s->get_info().c_str());
            }
        } else {
            os.printf("No switches found\n");
        }

        return true;
    }

    Module *m = Module::lookup("switch", name.c_str());
    if(m == nullptr) {
        os.printf("no such switch: %s\n", name.c_str());
        return true;
    }

    bool ok = false;
    if(value.empty()) {
        // get switch state
        bool state;
        ok = m->request("state", &state);
        if (!ok) {
            os.printf("unknown command %s.\n", "state");
            return true;
        }
        os.printf("switch %s is %d\n", name.c_str(), state);

    } else {
        const char *cmd;
        // set switch state
        if(value == "on" || value == "off") {
            bool b = value == "on";
            cmd = "set-state";
            ok =  m->request(cmd, &b);

        } else {
            float v = strtof(value.c_str(), NULL);
            cmd = "set-value";
            ok = m->request(cmd, &v);
        }

        if (ok) {
            os.printf("switch %s set to: %s\n", name.c_str(), value.c_str());
        } else {
            os.printf("unknown command %s.\n", cmd);
        }
    }

    return true;
}

bool CommandShell::switch_poll_cmd(std::string& params, OutputStream& os)
{
    HELP("returns switch poll query")
    std::string name = stringutils::shift_parameter( params );

    while(!name.empty()) {
        Module *m = Module::lookup("switch", name.c_str());
        if(m != nullptr) {
            // get switch state
            bool state;
            bool ok = m->request("state", &state);
            if (ok) {
                os.printf("switch %s is %d\n", name.c_str(), state);
            }
        }
        name = stringutils::shift_parameter( params );
    }

    os.set_no_response();

    return true;
}

static bool get_spindle_state()
{
    // get spindle switch state
    Module *m = Module::lookup("switch", "spindle");
    if(m != nullptr) {
        // get switch state
        bool state;
        if(m->request("state", &state)) {
            return state;
        }
    }
    return false;
}

// set or get gpio
bool CommandShell::gpio_cmd(std::string& params, OutputStream& os)
{
    HELP("set and get gpio pins: use GPIO5[14] | gpio5_14 | gpio5.14 | P4_10 | p4.10 out/in [on/off]");

    std::string gpio = stringutils::shift_parameter( params );
    std::string dir = stringutils::shift_parameter( params );

    if(gpio.empty()) {
        os.printf("incorrect usage\n");
        return true;
    }

    if(dir.empty() || dir == "in") {
        // read pin
        Pin pin(gpio.c_str(), Pin::AS_INPUT);
        if(!pin.connected()) {
            os.printf("Not a valid GPIO\n");
            return true;
        }

        os.printf("%s: %d\n", pin.to_string().c_str(), pin.get());
        return true;
    }

    if(dir == "out") {
        std::string v = stringutils::shift_parameter( params );
        if(v.empty()) {
            os.printf("on|off required\n");
            return true;
        }
        Pin pin(gpio.c_str(), Pin::AS_OUTPUT);
        if(!pin.connected()) {
            os.printf("Not a valid GPIO\n");
            return true;
        }
        bool b = (v == "on");
        pin.set(b);
        os.printf("%s: set to %d\n", pin.to_string().c_str(), pin.get());
        return true;
    }

    os.printf("incorrect usage\n");
    return true;
}

bool CommandShell::modules_cmd(std::string& params, OutputStream& os)
{
    HELP("List all registered modules\n");

    std::vector<std::string> l = Module::print_modules();

    if(l.empty()) {
        os.printf("No modules found\n");
        return true;
    }

    for(auto& i : l) {
        os.printf("%s\n", i.c_str());
    }

    os.set_no_response();
    return true;
}

bool CommandShell::get_cmd(std::string& params, OutputStream& os)
{
    HELP("get pos|wcs|state|status|temp|volts")
    std::string what = stringutils::shift_parameter( params );
    bool handled = true;
    if (what == "temp") {
        std::string type = stringutils::shift_parameter( params );
        if(type.empty()) {
            // scan all temperature controls
            std::vector<Module*> controllers = Module::lookup_group("temperature control");
            for(auto m : controllers) {
                TemperatureControl::pad_temperature_t temp;
                if(m->request("get_current_temperature", &temp)) {
                    os.printf("%s: %s (%d) temp: %f/%f @%d\n", m->get_instance_name(), temp.designator.c_str(), temp.tool_id, temp.current_temperature, temp.target_temperature, temp.pwm);
                } else {
                    os.printf("temp request failed\n");
                }
            }

        } else {
            Module *m = Module::lookup("temperature control", type.c_str());
            if(m == nullptr) {
                os.printf("%s is not a known temperature control", type.c_str());

            } else {
                TemperatureControl::pad_temperature_t temp;
                m->request("get_current_temperature", &temp);
                os.printf("%s temp: %f/%f @%d\n", type.c_str(), temp.current_temperature, temp.target_temperature, temp.pwm);
            }
        }

    } else if (what == "fk" || what == "ik") {
        // string p= shift_parameter( params );
        // bool move= false;
        // if(p == "-m") {
        //     move= true;
        //     p= shift_parameter( params );
        // }

        // std::vector<float> v= parse_number_list(p.c_str());
        // if(p.empty() || v.size() < 1) {
        //     os.printf("error:usage: get [fk|ik] [-m] x[,y,z]\n");
        //     return;
        // }

        // float x= v[0];
        // float y= (v.size() > 1) ? v[1] : x;
        // float z= (v.size() > 2) ? v[2] : y;

        // if(what == "fk") {
        //     // do forward kinematics on the given actuator position and display the cartesian coordinates
        //     ActuatorCoordinates apos{x, y, z};
        //     float pos[3];
        //     Robot::getInstance()->arm_solution->actuator_to_cartesian(apos, pos);
        //     os.printf("cartesian= X %f, Y %f, Z %f\n", pos[0], pos[1], pos[2]);
        //     x= pos[0];
        //     y= pos[1];
        //     z= pos[2];

        // }else{
        //     // do inverse kinematics on the given cartesian position and display the actuator coordinates
        //     float pos[3]{x, y, z};
        //     ActuatorCoordinates apos;
        //     Robot::getInstance()->arm_solution->cartesian_to_actuator(pos, apos);
        //     os.printf("actuator= X %f, Y %f, Z %f\n", apos[0], apos[1], apos[2]);
        // }

        // if(move) {
        //     // move to the calculated, or given, XYZ
        //     char cmd[64];
        //     snprintf(cmd, sizeof(cmd), "G53 G0 X%f Y%f Z%f", x, y, z);
        //     struct SerialMessage message;
        //     message.message = cmd;
        //     message.stream = &(StreamOutput::NullStream);
        //     THEKERNEL->call_event(ON_CONSOLE_LINE_RECEIVED, &message );
        //     THECONVEYOR->wait_for_idle();
        // }

    } else if (what == "pos") {
        // convenience to call all the various M114 variants, shows ABC axis where relevant
        std::string buf;
        Robot::getInstance()->print_position(0, buf); os.printf("last %s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(1, buf); os.printf("realtime %s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(2, buf); os.printf("%s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(3, buf); os.printf("%s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(4, buf); os.printf("%s\n", buf.c_str()); buf.clear();
        Robot::getInstance()->print_position(5, buf); os.printf("%s\n", buf.c_str()); buf.clear();

    } else if (what == "wcs") {
        // print the wcs state
        std::string cmd("-v");
        grblDP_cmd(cmd, os);

    } else if (what == "state") {
        // also $G and $I
        // [GC:G0 G54 G17 G21 G90 G94 M0 M5 M9 T0 F0.]
        os.printf("[GC:G%d %s G%d G%d G%d G94 M0 M%c M9 T%d F%1.4f S%1.4f]\n",
                  GCodeProcessor::get_group1_modal_code(),
                  stringutils::wcs2gcode(Robot::getInstance()->get_current_wcs()).c_str(),
                  Robot::getInstance()->plane_axis_0 == X_AXIS && Robot::getInstance()->plane_axis_1 == Y_AXIS && Robot::getInstance()->plane_axis_2 == Z_AXIS ? 17 :
                  Robot::getInstance()->plane_axis_0 == X_AXIS && Robot::getInstance()->plane_axis_1 == Z_AXIS && Robot::getInstance()->plane_axis_2 == Y_AXIS ? 18 :
                  Robot::getInstance()->plane_axis_0 == Y_AXIS && Robot::getInstance()->plane_axis_1 == Z_AXIS && Robot::getInstance()->plane_axis_2 == X_AXIS ? 19 : 17,
                  Robot::getInstance()->inch_mode ? 20 : 21,
                  Robot::getInstance()->absolute_mode ? 90 : 91,
                  get_spindle_state()?'3':'5',
                  0, // TODO get_active_tool(),
                  Robot::getInstance()->from_millimeters(Robot::getInstance()->get_feed_rate()),
                  Robot::getInstance()->get_s_value());

    } else if (what == "status") {
        // also ? on serial and usb
        std::string str;
        Robot::getInstance()->get_query_string(str);
        os.printf("%s\n", str.c_str());

    } else if (what == "volts") {
        std::string type = stringutils::shift_parameter( params );
        if(type.empty()) {
            int n = get_voltage_monitor_names(nullptr);
            if(n > 0) {
                const char *names[n];
                get_voltage_monitor_names(names);
                for (int i = 0; i < n; ++i) {
                    os.printf("%s: %f v\n", names[i], get_voltage_monitor(names[i]) * 11);
                }
            } else {
                os.printf("No voltage monitors configured\n");
            }
        } else {
            os.printf("%s: %f v\n", type.c_str(), get_voltage_monitor(type.c_str()) * 11);
        }

    } else {

        handled = false;
    }

    return handled;
}

bool CommandShell::grblDG_cmd(std::string& params, OutputStream& os)
{
    std::string cmd("state");
    return get_cmd(cmd, os);
}

bool CommandShell::grblDH_cmd(std::string& params, OutputStream& os)
{
    if(THEDISPATCHER->is_grbl_mode()) {
        return THEDISPATCHER->dispatch(os, 'G', 28, 2, 0); // G28.2 to home
    } else {
        return THEDISPATCHER->dispatch(os, 'G', 28, 0); // G28 to home
    }
}

bool CommandShell::grblDP_cmd(std::string& params, OutputStream& os)
{
    /*
    [G54:95.000,40.000,-23.600]
    [G55:0.000,0.000,0.000]
    [G56:0.000,0.000,0.000]
    [G57:0.000,0.000,0.000]
    [G58:0.000,0.000,0.000]
    [G59:0.000,0.000,0.000]
    [G28:0.000,0.000,0.000]
    [G30:0.000,0.000,0.000]
    [G92:0.000,0.000,0.000]
    [TLO:0.000]
    [PRB:0.000,0.000,0.000:0]
    */

    HELP("show grbl $ command")

    bool verbose = stringutils::shift_parameter( params ).find_first_of("Vv") != std::string::npos;

    std::vector<Robot::wcs_t> v = Robot::getInstance()->get_wcs_state();
    if(verbose) {
        char current_wcs = std::get<0>(v[0]);
        os.printf("[current WCS: %s]\n", stringutils::wcs2gcode(current_wcs).c_str());
    }

    int n = std::get<1>(v[0]);
    for (int i = 1; i <= n; ++i) {
        os.printf("[%s:%1.4f,%1.4f,%1.4f]\n", stringutils::wcs2gcode(i - 1).c_str(),
                  Robot::getInstance()->from_millimeters(std::get<0>(v[i])),
                  Robot::getInstance()->from_millimeters(std::get<1>(v[i])),
                  Robot::getInstance()->from_millimeters(std::get<2>(v[i])));
    }

    float rd[] {0, 0, 0};
    //PublicData::get_value( endstops_checksum, saved_position_checksum, &rd ); TODO use request
    os.printf("[G28:%1.4f,%1.4f,%1.4f]\n",
              Robot::getInstance()->from_millimeters(rd[0]),
              Robot::getInstance()->from_millimeters(rd[1]),
              Robot::getInstance()->from_millimeters(rd[2]));

    os.printf("[G30:%1.4f,%1.4f,%1.4f]\n",  0.0F, 0.0F, 0.0F); // not implemented

    os.printf("[G92:%1.4f,%1.4f,%1.4f]\n",
              Robot::getInstance()->from_millimeters(std::get<0>(v[n + 1])),
              Robot::getInstance()->from_millimeters(std::get<1>(v[n + 1])),
              Robot::getInstance()->from_millimeters(std::get<2>(v[n + 1])));

    if(verbose) {
        os.printf("[Tool Offset:%1.4f,%1.4f,%1.4f]\n",
                  Robot::getInstance()->from_millimeters(std::get<0>(v[n + 2])),
                  Robot::getInstance()->from_millimeters(std::get<1>(v[n + 2])),
                  Robot::getInstance()->from_millimeters(std::get<2>(v[n + 2])));
    } else {
        os.printf("[TLO:%1.4f]\n", Robot::getInstance()->from_millimeters(std::get<2>(v[n + 2])));
    }

    // this is the last probe position, updated when a probe completes, also stores the number of steps moved after a homing cycle
    float px, py, pz;
    uint8_t ps;
    std::tie(px, py, pz, ps) = Robot::getInstance()->get_last_probe_position();
    os.printf("[PRB:%1.4f,%1.4f,%1.4f:%d]\n", Robot::getInstance()->from_millimeters(px), Robot::getInstance()->from_millimeters(py), Robot::getInstance()->from_millimeters(pz), ps);

    return true;
}

// runs several types of test on the mechanisms
// TODO this will block the command thread, and queries will stop,
// may want to run the long running commands in a thread
bool CommandShell::test_cmd(std::string& params, OutputStream& os)
{
    HELP("test [jog|circle|square|raw]");

    AutoPushPop app; // this will save the state and restore it on exit
    std::string what = stringutils::shift_parameter( params );
    OutputStream nullos;

    if (what == "jog") {
        // jogs back and forth usage: axis distance iterations [feedrate]
        std::string axis = stringutils::shift_parameter( params );
        std::string dist = stringutils::shift_parameter( params );
        std::string iters = stringutils::shift_parameter( params );
        std::string speed = stringutils::shift_parameter( params );
        if(axis.empty() || dist.empty() || iters.empty()) {
            os.printf("usage: jog axis distance iterations [feedrate]\n");
            return true;
        }
        float d = strtof(dist.c_str(), NULL);
        float f = speed.empty() ? Robot::getInstance()->get_feed_rate() : strtof(speed.c_str(), NULL);
        uint32_t n = strtol(iters.c_str(), NULL, 10);

        bool toggle = false;
        Robot::getInstance()->absolute_mode = false;
        for (uint32_t i = 0; i < n; ++i) {
            THEDISPATCHER->dispatch(nullos, 'G', 0, 'F', f, toupper(axis[0]), toggle ? -d : d, 0);
            if(Module::is_halted()) break;
            toggle = !toggle;
        }

    } else if (what == "circle") {
        // draws a circle around origin. usage: radius iterations [feedrate]
        std::string radius = stringutils::shift_parameter( params );
        std::string iters = stringutils::shift_parameter( params );
        std::string speed = stringutils::shift_parameter( params );
        if(radius.empty() || iters.empty()) {
            os.printf("usage: circle radius iterations [feedrate]\n");
            return true;
        }

        float r = strtof(radius.c_str(), NULL);
        uint32_t n = strtol(iters.c_str(), NULL, 10);
        float f = speed.empty() ? Robot::getInstance()->get_feed_rate() : strtof(speed.c_str(), NULL);

        Robot::getInstance()->absolute_mode = false;
        THEDISPATCHER->dispatch(nullos, 'G', 0, 'X', -r, 'F', f, 0);
        Robot::getInstance()->absolute_mode = true;

        for (uint32_t i = 0; i < n; ++i) {
            if(Module::is_halted()) break;
            THEDISPATCHER->dispatch(nullos, 'G', 2, 'I', r, 'J', 0.0F, 'F', f, 0);
        }

        // leave it where it started
        if(!Module::is_halted()) {
            Robot::getInstance()->absolute_mode = false;
            THEDISPATCHER->dispatch(nullos, 'G', 0, 'X', r, 'F', f, 0);
            Robot::getInstance()->absolute_mode = true;
        }

    } else if (what == "square") {
        // draws a square usage: size iterations [feedrate]
        std::string size = stringutils::shift_parameter( params );
        std::string iters = stringutils::shift_parameter( params );
        std::string speed = stringutils::shift_parameter( params );
        if(size.empty() || iters.empty()) {
            os.printf("usage: square size iterations [fedrate]\n");
            return true;
        }
        float d = strtof(size.c_str(), NULL);
        float f = speed.empty() ? Robot::getInstance()->get_feed_rate() : strtof(speed.c_str(), NULL);
        uint32_t n = strtol(iters.c_str(), NULL, 10);

        Robot::getInstance()->absolute_mode = false;

        for (uint32_t i = 0; i < n; ++i) {
            THEDISPATCHER->dispatch(nullos, 'G', 0, 'X', d, 'F', f, 0);
            THEDISPATCHER->dispatch(nullos, 'G', 0, 'Y', d, 0);
            THEDISPATCHER->dispatch(nullos, 'G', 0, 'X', -d, 0);
            THEDISPATCHER->dispatch(nullos, 'G', 0, 'Y', -d, 0);
            if(Module::is_halted()) break;
        }

    } else if (what == "raw") {

        // issues raw steps to the specified axis usage: axis steps steps/sec
        std::string axis = stringutils::shift_parameter( params );
        std::string stepstr = stringutils::shift_parameter( params );
        std::string stepspersec = stringutils::shift_parameter( params );
        if(axis.empty() || stepstr.empty() || stepspersec.empty()) {
            os.printf("usage: raw axis steps steps/sec\n");
            return true;
        }

        char ax = toupper(axis[0]);
        uint8_t a = ax >= 'X' ? ax - 'X' : ax - 'A' + 3;
        int steps = strtol(stepstr.c_str(), NULL, 10);
        bool dir = steps >= 0;
        steps = std::abs(steps);

        if(a > C_AXIS) {
            os.printf("error: axis must be x, y, z, a, b, c\n");
            return true;
        }

        if(a >= Robot::getInstance()->get_number_registered_motors()) {
            os.printf("error: axis is out of range\n");
            return true;
        }

        uint32_t sps = strtol(stepspersec.c_str(), NULL, 10);
        sps = std::max(sps, (uint32_t)1);

        os.printf("issuing %d steps at a rate of %d steps/sec on the %c axis\n", steps, sps, ax);
        uint32_t delayus = 1000000.0F / sps;
        for(int s = 0; s < steps; s++) {
            if(Module::is_halted()) break;
            Robot::getInstance()->actuators[a]->manual_step(dir);
            // delay
            StopWatch_DelayUs(delayus);
        }

        // reset the position based on current actuator position
        Robot::getInstance()->reset_position_from_current_actuator_position();

    } else {
        os.printf("usage:\n test jog axis distance iterations [feedrate]\n");
        os.printf(" test square size iterations [feedrate]\n");
        os.printf(" test circle radius iterations [feedrate]\n");
        os.printf(" test raw axis steps steps/sec\n");
    }

    return true;
}

bool CommandShell::version_cmd(std::string& params, OutputStream& os)
{
    HELP("version - print version");

    Version vers;
    const char *mcu = "LPC4330 on " BUILD_TARGET;
    os.printf("Build version: %s, Build date: %s, MCU: %s, System Clock: %ldMHz\r\n", vers.get_build(), vers.get_build_date(), mcu, SystemCoreClock / 1000000);
    os.printf("%d axis\n", MAX_ROBOT_ACTUATORS);

    os.set_no_response();

    return true;
}

bool CommandShell::m115_cmd(GCode& gcode, OutputStream& os)
{
    Version vers;

    os.printf("FIRMWARE_NAME:Smoothieware2, FIRMWARE_URL:http%%3A//smoothieware.org, X-SOURCE_CODE_URL:https://github.com/Smoothieware/SmoothieV2, FIRMWARE_VERSION:%s, X-FIRMWARE_BUILD_DATE:%s, X-SYSTEM_CLOCK:%ldMHz, X-AXES:%d, X-GRBL_MODE:%d\n", vers.get_build(), vers.get_build_date(), SystemCoreClock / 1000000, MAX_ROBOT_ACTUATORS, Dispatcher::getInstance()->is_grbl_mode() ? 1 : 0);

    return true;
}

bool CommandShell::config_get_cmd(std::string& params, OutputStream& os)
{
    HELP("config-get \"section name\"");
    std::string sectionstr = stringutils::shift_parameter( params );
    if(sectionstr.empty()) {
        os.printf("Usage: config-get section\n");
        return true;
    }

    std::fstream fsin;
    fsin.open("/sd/config.ini", std::fstream::in);
    if(!fsin.is_open()) {
        os.printf("Error opening file /sd/config.ini\n");
        return true;
    }

    ConfigReader cr(fsin);
    ConfigReader::section_map_t m;
    bool b = cr.get_section(sectionstr.c_str(), m);
    if(b) {
        for(auto& s : m) {
            std::string k = s.first;
            std::string v = s.second;
            os.printf("%s = %s\n", k.c_str(), v.c_str());
        }
    } else {
        os.printf("No section named %s\n", sectionstr.c_str());
    }

    fsin.close();

    os.set_no_response();

    return true;
}

bool CommandShell::config_set_cmd(std::string& params, OutputStream& os)
{
    HELP("config-set \"section name\" key [=] value");
    os.set_no_response();

    std::string sectionstr = stringutils::shift_parameter( params );
    std::string keystr = stringutils::shift_parameter( params );
    std::string valuestr = stringutils::shift_parameter( params );
    if(valuestr == "=") {
        // ignore optional =
        valuestr = stringutils::shift_parameter( params );
    }

    if(sectionstr.empty() || keystr.empty() || valuestr.empty()) {
        os.printf("Usage: config-set section key value\n");
        return true;
    }

    std::fstream fsin;
    std::fstream fsout;
    fsin.open("/sd/config.ini", std::fstream::in);
    if(!fsin.is_open()) {
        os.printf("Error opening file /sd/config.ini\n");
        return true;
    }

    fsout.open("/sd/config.tmp", std::fstream::out);
    if(!fsout.is_open()) {
        os.printf("Error opening file /sd/config.tmp\n");
        fsin.close();
        return true;
    }

    ConfigWriter cw(fsin, fsout);

    const char *section = sectionstr.c_str();
    const char *key = keystr.c_str();
    const char *value = valuestr.c_str();

    if(cw.write(section, key, value)) {
        os.printf("config changed ok\n");

    } else {
        os.printf("failed to change config\n");
        return true;
    }

    fsin.close();
    fsout.close();

    // now rename the config.ini file to config.bak and config.tmp file to config.ini
    // remove old backup file first
    remove("/sd/config.bak");
    int s = rename("/sd/config.ini", "/sd/config.bak");
    if(s == 0) {
        s = rename("/sd/config.tmp", "/sd/config.ini");
        if(s != 0) os.printf("Failed to rename config.tmp to config.ini\n");

    } else {
        os.printf("Failed to rename config.ini to config.bak - aborting\n");
    }
    return true;
}

bool CommandShell::ry_cmd(std::string& params, OutputStream& os)
{
    HELP("ymodem recieve");

    if(!Conveyor::getInstance()->is_idle()) {
        os.printf("ry not allowed while printing or busy\n");
        return true;
    }

    if(params.empty()) {
        os.printf("start ymodem transfer\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }


    YModem ymodem([&os](char c){os.write(&c, 1);});
    // check we did not run out of memory
    if(!ymodem.is_ok()) {
        os.printf("error: not enough memory\n");
        return true;
    }

    set_capture([&ymodem](char c) { ymodem.add(c); });
    int ret = ymodem.receive();
    set_capture(nullptr);

    if(params.empty()) {
        if(ret > 0) {
            os.printf("downloaded %d file(s) ok\n", ret);
        } else {
            os.printf("download failed with error %d\n", ret);
        }
    } else {
        os.set_no_response(true);
    }

    return true;
}

bool CommandShell::truncate_file(const char *fn, int size, OutputStream& os)
{
    FIL fp;  /* File object */
    // Open file
    int ret = f_open(&fp, fn, FA_WRITE);
    if(FR_OK != ret) {
        os.printf("file %s does not exist\n", fn);
        return false;
    }

    ret = f_lseek(&fp, size);
    if(FR_OK != ret) {
        f_close(&fp);
        os.printf("error %d seeking to %d bytes\n", ret, size);
        return false;
    }

    ret = f_truncate(&fp);
    f_close(&fp);
    if(FR_OK != ret) {
        os.printf("error %d truncating file\n", ret);
        return false;
    }

    return true;
}

bool CommandShell::truncate_cmd(std::string& params, OutputStream& os)
{
    HELP("truncate file to size: truncate filename size-in-bytes");
    std::string fn = stringutils::shift_parameter( params );
    std::string sizestr = stringutils::shift_parameter( params );

    if(fn.empty() || sizestr.empty()) {
        os.printf("Usage: truncate filename size\n");
        return true;
    }

    char *e = NULL;
    int size = strtol(sizestr.c_str(), &e, 10);
    if (e <= sizestr.c_str() || size <= 0) {
        os.printf("size must be > 0\n");
        return true;
    }

    if(truncate_file(fn.c_str(), size, os)) {
        os.printf("File %s truncated to %d bytes\n", fn.c_str(), size);
    }

    return true;
}

bool CommandShell::break_cmd(std::string& params, OutputStream& os)
{
    HELP("force s/w break point");
    //*(volatile int*)0xa5a5a5a4 = 1; // force hardware fault
    __asm("bkpt #0");
    return true;
}

bool CommandShell::reset_cmd(std::string& params, OutputStream& os)
{
    HELP("reset board");
    os.printf("Reset will occur in 5 seconds, make sure to disconnect before that\n");
    vTaskDelay(pdMS_TO_TICKS(5000));
    *(volatile int*)0x40053100 = 1; // reset core
    return true;
}

#include "uart_comms.h"
extern "C" void shutdown_sdmmc();
extern "C" void shutdown_cdc();
bool CommandShell::flash_cmd(std::string& params, OutputStream& os)
{
    HELP("flash image - flash flashme.bin");
    uint32_t magic = *(uint32_t*)0x14700000;
    if(magic != 0x5555AAAAUL) {
        os.printf("No magic flashloader: %08X\n", magic);
        return true;
    }

    // check the flashme.bin is on the disk first
    FILE *fp = fopen("/sd/flashme.bin", "r");
    if(fp == NULL) {
        os.printf("No flashme.bin file found\n");
        return true;
    }
    fclose(fp);

    // stop stuff
    f_unmount("sd");
    FastTicker::getInstance()->stop();
    StepTicker::getInstance()->stop();
    Adc::stop();
    shutdown_sdmmc();
    shutdown_cdc();
    vTaskSuspendAll();
    __disable_irq();
    //NVIC_DisableIRQ(USB0_IRQn);
    //NVIC_DisableIRQ(SysTick_IRQn);

    // get start address of the flash loader
    uint32_t p = *(uint32_t*)0x14700004;
    void (*runat)(void) = *(void (*)())p;
    os.printf("Executing at %p\n", runat);
    stop_uart();

    runat();

    // should never get here
    __asm("bkpt #0");
    return true;
}

bool CommandShell::jog_cmd(std::string& params, OutputStream& os)
{
    HELP("instant jog: $J X0.01 [F0.5] - axis can be one of XYZABC, optional speed is scale of max_rate");
    // $J X0.1 F0.5
    int n_motors = Robot::getInstance()->get_number_registered_motors();

    // get axis to move and amount (X0.1)
    // for now always 1 axis
    size_t npos = params.find_first_of("XYZABC");
    if(npos == std::string::npos) {
        os.printf("usage: $J X0.01 [F0.5]\n");
        return true;
    }

    std::string s = params.substr(npos);
    if(s.empty() || s.size() < 2) {
        os.printf("usage: $J X0.01 [F0.5]\n");
        return true;
    }
    char ax = toupper(s[0]);
    uint8_t a = ax >= 'X' ? ax - 'X' : ax - 'A' + 3;
    if(a >= n_motors) {
        os.printf("error:bad axis\n");
        return true;
    }

    float d = strtof(s.substr(1).c_str(), NULL);

    float delta[n_motors];
    for (int i = 0; i < n_motors; ++i) {
        delta[i] = 0;
    }
    delta[a] = d;

    // get speed scale
    float scale = 1.0F;
    npos = params.find_first_of("F");
    if(npos != std::string::npos && npos + 1 < params.size()) {
        scale = strtof(params.substr(npos + 1).c_str(), NULL);
    }

    Robot::getInstance()->push_state();
    float rate_mm_s = Robot::getInstance()->actuators[a]->get_max_rate() * scale;
    Robot::getInstance()->delta_move(delta, rate_mm_s, n_motors);

    // turn off queue delay and run it now
    Conveyor::getInstance()->force_queue();
    Robot::getInstance()->pop_state();
    //os.printf("Jog: %c%f F%f\n", ax, d, scale);

    return true;
}
