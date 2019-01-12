#pragma once

#include "Module.h"

class Network : public Module {
    public:
        Network();
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);
        bool start(void);

    private:
};
