#pragma once

#include "Module.h"

#include <string>

class OutputStream;
class Ftpd;

class Network : public Module {
    public:
        Network();
        virtual ~Network();
        static bool create(ConfigReader& cr);
        bool configure(ConfigReader& cr);

    private:
        static void vSetupIFTask(void *pvParameters);

        void network_thread();
        bool start(void);

        bool handle_net_cmd( std::string& params, OutputStream& os );
        struct netif *lpc_netif;
        std::string hostname;
        char *ip_address{nullptr};
        char *ip_mask{nullptr};
        char *ip_gateway{nullptr};

        bool enable_shell{false};
        bool enable_httpd{false};
        bool enable_ftpd{false};
};
