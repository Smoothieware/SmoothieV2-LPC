#pragma once

#include "Module.h"

class OutputStream;

class Network : public Module {
    public:
        Network();
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);
        bool start(void);

    private:
        bool handle_net_cmd( std::string& params, OutputStream& os );
};
