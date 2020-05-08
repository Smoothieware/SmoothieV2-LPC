#include "Module.h"
#include "Robot.h"

// static
std::map<const std::string, Module::modrec_t> Module::registry;
bool Module::halted= false;

Module::Module(const char* grp, const char* inst) : group_name(grp), instance_name(inst)
{
    single= false;
    added = add(grp, inst);
}

Module::Module(const char* grp) : group_name(grp)
{
    single= true;
    added = add(grp);
}

Module::~Module()
{
    // remove from the registry
    auto g = registry.find(group_name);
    if(g != registry.end()) {
        if(single) {
            registry.erase(g);
        } else {
            auto i = g->second.map->find(instance_name);
            if(i != g->second.map->end()) {
                g->second.map->erase(i);
            }
            // remove group if instances are empty as well
            if(g->second.map->empty()) {
                registry.erase(g);
            }
        }
    }
}

bool Module::add(const char* group, const char* instance)
{
    auto g = registry.find(group);
    // we have the group entry if it exists
    if(single) {
        if(g == registry.end()) {
            modrec_t m;
            m.module = this;
            m.map = nullptr;
            registry.insert(registry_t::value_type(group, m));
        } else {
            // TODO if it is a duplicate that is an error
            return false;
        }

    } else {
        if(g == registry.end()) {
            // new group entry
            modrec_t m;
            m.map = new instance_map_t;
            m.map->insert(instance_map_t::value_type(instance, this));
            m.module = nullptr;
            registry.insert(registry_t::value_type(group, m));

        } else if(g->second.map != nullptr) {
            // add instance to existing group map
            g->second.map->insert(instance_map_t::value_type(instance, this));

        } else {
            // TODO error was not a map
            return false;
        }
    }
    return true;
}

// this may be called in a Timer context so all on_halts must be Timer task safe
void Module::broadcast_halt(bool flg)
{
    halted= flg;
    for(auto& i : registry) {
        // foreach entry in the registry
        auto& x = i.second;
        if(x.map != nullptr) {
            // it is a map of modules in a group
            for(auto& j : *x.map) {
                j.second->on_halt(flg);
            }

        } else if(x.module != nullptr) {
            // it is a single module
            x.module->on_halt(flg);

        } else {
            // TODO something bad happened neither map nor module is set
        }
    }

    if(halted) {
        // if was not idle then fixup positions.
        // if we interrupted a move we need to resync the coordinates systems with the actuator position
        // TODO we may need to wait a bit to allow everything to stop
        // TODO we may need to only do this if we were not idle when we halted
        if(Robot::getInstance() != nullptr) { // so we can test it without loading robot
            Robot::getInstance()->reset_position_from_current_actuator_position();
        }
    }
}

void Module::broadcast_in_commmand_ctx(bool idle)
{
    for(auto& i : registry) {
        // foreach entry in the registry
        auto& x = i.second;
        if(x.map != nullptr) {
            // it is a map of modules in a group
            for(auto& j : *x.map) {
                if(j.second->want_command_ctx) {
                    j.second->in_command_ctx(idle);
                }
            }

        } else if(x.module != nullptr) {
            // it is a single module
            if(x.module->want_command_ctx) {
                x.module->in_command_ctx(idle);
            }

        } else {
            // TODO something bad happened neither map nor module is set
        }
    }
}

Module* Module::lookup(const char *group, const char *instance)
{
    auto g = registry.find(group);
    if(g == registry.end()) return nullptr;

    if(g->second.map != nullptr) {
        // it is a group so find the instance in that group
        if(instance != nullptr) {
            auto i = g->second.map->find(instance);
            if(i == g->second.map->end()) return nullptr;
            return i->second;
        }
        return nullptr;

    } else if(g->second.module != nullptr) {
        return g->second.module;
    }

    return nullptr;
}

std::vector<Module*> Module::lookup_group(const char *group)
{
    std::vector<Module*> results;

    auto g = registry.find(group);
    if(g != registry.end() && g->second.map != nullptr) {
        for(auto& i : *g->second.map) {
            // add each module in this group
            results.push_back(i.second);
        }
    }

    return results;
}

std::vector<std::string> Module::print_modules()
{
    std::vector<std::string> l;
    for(auto& i : registry) {
        // foreach entry in the registry
        std::string r(i.first); // group name
        auto& x = i.second;
        if(x.map != nullptr) {
            // it is a map of modules in a group
            r.append(": ");
            for(auto& j : *x.map) {
                r.append(j.first).append(",");
            }
            r.pop_back();
        }
        l.push_back(r);
    }

    return l;
}
