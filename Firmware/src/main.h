#pragma once

#include <functional>
#include "MemoryPool.h"

class OutputStream;

// sets a callback for all incoming data
void set_capture(std::function<void(char)> cf);
using StartupFunc_t = std::function<void()>;
void register_startup(StartupFunc_t sf);

// TODO may move to Dispatcher
bool dispatch_line(OutputStream& os, const char *line);
bool process_command_buffer(size_t n, char *rxBuf, OutputStream *os, char *line, size_t& cnt, bool& discard, bool wait=true);

// print string to all connected consoles
extern "C" void print_to_all_consoles(const char *);
extern "C" void set_abort_comms();

// sleep for given ms, but don't block things like ?
void safe_sleep(uint32_t ms);
// get the vmotor and vfet voltages
float get_voltage_monitor(const char* name);
int get_voltage_monitor_names(const char *names[]);

#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))

extern MemoryPool *_RAM2;
extern MemoryPool *_RAM3;
extern MemoryPool *_RAM4;
extern MemoryPool *_RAM5;

// the communications task priority (lower number is lower priority)
#define COMMS_PRI 3UL
// The command thread priority
#define CMDTHRD_PRI 2UL
