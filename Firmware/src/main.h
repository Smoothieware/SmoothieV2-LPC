#pragma once

#include <functional>

class OutputStream;

// sets a callback for all incoming data
void set_capture(std::function<void(char)> cf);

// TODO may move to Dispatcher
bool dispatch_line(OutputStream& os, const char *line);
// print string to all connected consoles
void print_to_all_consoles(const char *);
// sleep for given ms, but don't block things like ?
void safe_sleep(uint32_t ms);
// get the vmotor and vfet voltages
float get_voltage_monitor(const char* name);
void print_voltage_monitors(OutputStream& os, float scale=1.0F);

#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))
