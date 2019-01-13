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
        static void vSetupIFTask(void *pvParameters);

        bool handle_net_cmd( std::string& params, OutputStream& os );

        static bool enable_shell;
        static bool enable_httpd;
        static bool enable_ftpd;
};
