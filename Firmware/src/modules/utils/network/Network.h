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
        static Network *instance;
        static void vSetupIFTask(void *pvParameters);
        void network_thread();
        bool start(void);

        bool handle_net_cmd( std::string& params, OutputStream& os );

        char *ip_address{nullptr};
        char *ip_mask{nullptr};
        char *ip_gateway{nullptr};

        static bool enable_shell;
        static bool enable_httpd;
        static bool enable_ftpd;
};
