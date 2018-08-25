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
#define RAMFUNC __attribute__ (section (".ramfunctions"))
