#pragma once

#include <map>
#include <functional>
#include <string>
#include <set>
#include <stdint.h>

#define THEDISPATCHER Dispatcher::getInstance()

class GCode;
class OutputStream;

class Dispatcher
{
public:
    // setup the Singleton instance
    static Dispatcher *getInstance() { return instance; }
    Dispatcher();
    // delete copy and move constructors and assign operators
    Dispatcher(Dispatcher const&) = delete;             // Copy construct
    Dispatcher(Dispatcher&&) = delete;                  // Move construct
    Dispatcher& operator=(Dispatcher const&) = delete;  // Copy assign
    Dispatcher& operator=(Dispatcher &&) = delete;      // Move assign

    using Handler_t = std::function<bool(GCode&, OutputStream&)>;
    using Handlers_t = std::multimap<uint16_t, Handler_t>;
    enum HANDLER_NAME { GCODE_HANDLER, MCODE_HANDLER };
    Handlers_t::iterator add_handler(HANDLER_NAME gcode, uint16_t code, Handler_t fnc);

    using CommandHandler_t = std::function<bool(std::string&, OutputStream&)>;
    using CommandHandlers_t = std::multimap<std::string, CommandHandler_t>;
    CommandHandlers_t::iterator add_handler(std::string cmd, CommandHandler_t fnc);
    std::set<std::string> get_commands() const;
    void remove_handler(HANDLER_NAME gcode, Handlers_t::iterator);
    bool dispatch(GCode &gc, OutputStream& os, bool need_ok= true) const;
    bool dispatch(OutputStream& os, char cmd, uint16_t code, ...) const;
    bool dispatch(const char *line, OutputStream& os) const;
    bool load_configuration() const;
    void clear_handlers();
    bool is_grbl_mode() const { return grbl_mode; }
    void set_grbl_mode(bool flg) { grbl_mode= flg; }

private:
    static Dispatcher *instance;

    // use multimap as multiple handlers may be needed per gcode
    Handlers_t gcode_handlers;
    Handlers_t mcode_handlers;
    CommandHandlers_t command_handlers;
    bool grbl_mode{false};
};

