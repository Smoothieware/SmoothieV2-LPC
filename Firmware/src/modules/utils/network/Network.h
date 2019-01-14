#pragma once

#include "Module.h"

class OutputStream;
class Ftpd;

class Network : public Module {
    public:
        Network();
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);

    private:
        static void vSetupIFTask(void *pvParameters);
        bool start(void);

        bool handle_net_cmd( std::string& params, OutputStream& os );

        Ftpd *ftpd{nullptr};

        static bool enable_shell;
        static bool enable_httpd;
        static bool enable_ftpd;
};
