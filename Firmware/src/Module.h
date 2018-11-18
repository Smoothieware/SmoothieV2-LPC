#pragma once

#include <map>
#include <string>
#include <vector>
#include <atomic>

class ConfigReader;

class Module
{
public:
    Module(const char* group, const char* instance);
    Module(const char* group);
    virtual ~Module();

    // called to allow the module to read its configuration data
    virtual bool configure(ConfigReader& cr){ return false; };

    // the system is entering or leaving halt/alarm mode flg == true if entering
    // may be called in an ISR context
    virtual void on_halt(bool flg) {};

    // request public data from module instance, tpy eof data requested is in key,
    // and the address of an appropriate returned data type is provided by the caller
    virtual bool request(const char *key, void *value) { return false; }

    // sent in command thread context about every 200ms, or after every gcode processed
    virtual void in_command_ctx(bool idle) {};

    // module registry function, to look up an instance of a module
    // returns nullptr if not found, otherwise returns a pointer to the module
    static Module* lookup(const char *group, const char *instance= nullptr);

    // lookup and return an array of modules that belong to group, returns empty if not found of if not a group
    static std::vector<Module*> lookup_group(const char *group);

    using instance_map_t = std::map<const std::string, Module*>; // map of module instances
    // either a map of module instances OR a pointer to a module, one will be nullptr the other won't be
    using modrec_t = struct { instance_map_t *map; Module *module; };

    // sends the on_halt event to all modules, flg is true if halt, false if cleared
    // may be called in an ISR context
    static void broadcast_halt(bool flg);
    static bool is_halted() { return halted; }

    static std::vector<std::string> print_modules();
    static void broadcast_in_commmand_ctx(bool idle);

    bool was_added() const { return added; }

    const char *get_group_name() const { return group_name.c_str(); }
    const char *get_instance_name() const { return instance_name.c_str(); }

protected:
    // TODO do we really want to store these here? currently needed for destructor
    std::string group_name, instance_name;

    // set if module wants the command_ctx callback
    std::atomic_bool want_command_ctx{false};

private:
    using registry_t = std::map<const std::string, modrec_t>;
    static registry_t registry;
    bool add(const char* group, const char* instance= nullptr);

    // specifies whether this is registered as a single module or a group of modules
    bool single{false};

    // set if sucessfully registered
    bool added{false};

    static bool halted;
};

// this puts the address of the create function in a known area for main to call to create the module(s)
#define REGISTER_MODULE(n, m) __attribute__ ((used,section(".registered_modules"))) static uint32_t RM_##n= (uint32_t)m;
