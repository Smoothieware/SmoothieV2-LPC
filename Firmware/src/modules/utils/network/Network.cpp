#include "Network.h"

#include "lwip/init.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/memp.h"
#include "lwip/tcpip.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
//#include "lwip/timers.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"

#include "board.h"
#include "arch/lpc18xx_43xx_emac.h"
#include "arch/lpc_arch.h"
#include "arch/sys_arch.h"
#include "lpc_phy.h" /* For the PHY monitor support */

#include "main.h"
#include "ConfigReader.h"
#include "Dispatcher.h"
#include "OutputStream.h"

#include "ftpd.h"

#define network_enable_key "enable"
#define shell_enable_key "shell_enable"
#define ftp_enable_key "ftp_enable"
#define webserver_enable_key "webserver_enable"
#define ip_address_key  "ip_address"
#define ip_mask_key "ip_mask"
#define ip_gateway_key "ip_gateway"
#define hostname_key "hostname"

REGISTER_MODULE(Network, Network::create)

bool Network::create(ConfigReader& cr)
{
    printf("DEBUG: configure network\n");
    Network *network = new Network();
    if(!network->configure(cr)) {
        printf("INFO: Network not enabled\n");
        delete network;
        return false;
    }

    // register a startup function
    register_startup(std::bind(&Network::start, network));

    return true;
}

Network::Network() : Module("network")
{
    lpc_netif= (struct netif*)malloc(sizeof(struct netif));
}

Network::~Network()
{
    free(lpc_netif);
}

bool Network::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("network", m)) return false;

    bool enable = cr.get_bool(m, network_enable_key, false);
    if(!enable) {
        return false;
    }

    hostname = cr.get_string(m, hostname_key, "smoothiev2");

    std::string ip_address_str = cr.get_string(m, ip_address_key, "auto");
    if(!ip_address_str.empty() && ip_address_str != "auto") {
        std::string ip_mask_str = cr.get_string(m, ip_mask_key, "255.255.255.0");
        std::string ip_gateway_str = cr.get_string(m, ip_gateway_key, "192.168.1.254");
        ip_address= strdup(ip_address_str.c_str());
        ip_mask= strdup(ip_mask_str.c_str());
        ip_gateway= strdup(ip_gateway_str.c_str());
    }

    enable_shell = cr.get_bool(m, shell_enable_key, false);
    enable_ftpd = cr.get_bool(m, ftp_enable_key, false);
    enable_httpd = cr.get_bool(m, webserver_enable_key, false);

    // register command handlers
    using std::placeholders::_1;
    using std::placeholders::_2;

    THEDISPATCHER->add_handler( "net", std::bind( &Network::handle_net_cmd, this, _1, _2) );

    return true;
}

#include "lwip/tcp.h"
#include "lwip/inet.h"
extern "C" struct tcp_pcb ** const tcp_pcb_lists[];
static void netstat(OutputStream& os)
{
    enum tcp_state eState;
    u16_t local_port, remote_port;
    ip_addr_t local_ip, remote_ip;
    int j = 1;
    struct tcp_pcb *cpcb;
    for (int i = 0; i < 4; i++) {
        for(cpcb = *tcp_pcb_lists[i]; cpcb != NULL; cpcb = cpcb->next) {
            os.printf("%2u           ", j++);
            os.printf("TCP");
            os.printf("        ");        //spacer

            portENTER_CRITICAL();
            //get all states during disabled preemption so that the pcb pointer can't be nulled in between
            eState = cpcb->state;
            local_port = cpcb->local_port;
            local_ip = cpcb->local_ip;
            remote_port = cpcb->remote_port;
            remote_ip = cpcb->remote_ip;
            portEXIT_CRITICAL();


            os.printf("%15s", inet_ntoa(local_ip));
            os.printf("   ");                            //spacer
            os.printf("%5u", local_port);
            os.printf("        ");         //spacer

            if(remote_ip.addr != 0 && eState != LISTEN) {
                os.printf("%15s", inet_ntoa(remote_ip));
                os.printf("   ");                     //spacer
                os.printf("%5u", remote_port);

            } else {
                os.printf("---.---.---.---");         //empty IP
                os.printf("   ");                     //spacer
                os.printf("-----");                   //emtpy Port
            }

            os.printf("          SOCKET    ");     //socket API
            switch(eState) {
                case CLOSED:
                    os.printf("CLOSED");
                    break;

                case LISTEN:
                    os.printf("LISTEN");
                    break;

                case ESTABLISHED:
                    os.printf("ESTABLISHED");
                    break;

                case FIN_WAIT_1:
                    os.printf("FIN_WAIT_1");
                    break;

                case FIN_WAIT_2:
                    os.printf("FIN_WAIT_2");
                    break;

                case CLOSE_WAIT:
                    os.printf("CLOSE_WAIT");
                    break;

                case TIME_WAIT:
                    os.printf("TIME_WAIT");
                    break;

                case SYN_SENT:
                case SYN_RCVD:
                    os.printf("CONNECTING");
                    break;

                case CLOSING:
                    os.printf("CLOSING");
                    break;

                case LAST_ACK:
                    os.printf("LAST_ACK");
                    break;

                default:
                    os.printf("???");
                    break;
            }

            //  case NETCONN_UDP:
            //      os.printf("UDP");
            //      os.printf("        ");                                     //spacer
            //      os.printf("%15s", sock->conn->pcb.udp->local_ip);
            //      os.printf("   ");                                         //spacer
            //      os.printf("%5u", sock->conn->pcb.udp->local_port);
            //      os.printf("        ");                                     //spacer
            //      os.printf("               ");                 //empty foreign IP (n/a in UDP)
            //      os.printf("   ");                            //spacer
            //      os.printf("     ");                          //empty foreign port (n/a in UDP)
            //      os.printf("          SOCKET");         //socket API
            //      break;

            //  default:
            //      os.printf("???");
            //      break;

            // }
            os.printf("\r\n");
        }
    }
}

#define HELP(m) if(params == "-h") { os.printf("%s\n", m); return true; }
bool Network::handle_net_cmd( std::string& params, OutputStream& os )
{
    HELP("net - show network status, -n also shows netstat");

    if(lpc_netif->flags & NETIF_FLAG_LINK_UP) {
        os.printf("hostname: %s\n", netif_get_hostname(lpc_netif));
        os.printf("Link UP\n");
        if (lpc_netif->ip_addr.addr) {
            static char tmp_buff[16];
            os.printf("IP_ADDR    : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif->ip_addr, tmp_buff, 16));
            os.printf("NET_MASK   : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif->netmask, tmp_buff, 16));
            os.printf("GATEWAY_IP : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif->gw, tmp_buff, 16));
            os.printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    lpc_netif->hwaddr[0], lpc_netif->hwaddr[1], lpc_netif->hwaddr[2],
                    lpc_netif->hwaddr[3], lpc_netif->hwaddr[4], lpc_netif->hwaddr[5]);

        } else {
            os.printf("no ip set\n");
        }
    } else {
        os.printf("Link DOWN\n");
    }

    if(!params.empty()) {
        // do netstat-like dump
        os.puts("\nNetstat...\n");
        netstat(os);
    }

    os.set_no_response();

    return true;
}

/* Callback for TCPIP thread to indicate TCPIP init is done */
static void tcpip_init_done_signal(void *arg)
{
    /* Tell main thread TCP/IP init is done */
    sys_sem_t *init_sem = (sys_sem_t*)arg;
    sys_sem_signal(init_sem);
}

// used by various emac drivers
extern "C" void msDelay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}


/* LWIP kickoff and PHY link monitor thread */
void Network::network_thread()
{
    ip_addr_t ipaddr, netmask, gw;
    uint32_t physts;
    static int prt_ip = 0;

    {
        // Wait until the TCP/IP thread is finished before continuing or wierd things may happen
        printf("Network: Waiting for TCPIP thread to initialize...\n");
        sys_sem_t init_sem;
        sys_sem_new(&init_sem, 0);
        tcpip_init(tcpip_init_done_signal, (void *) &init_sem);
        sys_sem_wait(&init_sem);
        sys_sem_free(&init_sem);
        printf("Network: TCPIP thread has started...\n");
    }

    // needed for LWIP_NETCONN_SEM_PER_THREAD which is needed for LWIP_NETCONN_FULLDUPLEX
    netconn_thread_init();

    if(ip_address == nullptr) {
        // dhcp
        IP4_ADDR(&gw, 0, 0, 0, 0);
        IP4_ADDR(&ipaddr, 0, 0, 0, 0);
        IP4_ADDR(&netmask, 0, 0, 0, 0);
        printf("network: using DHCP\n");

    } else {
        /* Static IP assignment */
        if(ipaddr_aton(ip_address, &ipaddr) == 0) {
            printf("Network: invalid ip address: %s\n", ip_address);
        }
        if(ipaddr_aton(ip_mask, &netmask) == 0) {
            printf("Network: invalid ip netmask: %s\n", ip_mask);
        }
        if(ipaddr_aton(ip_gateway, &gw) == 0) {
            printf("Network: invalid ip gateway: %s\n", ip_gateway);
        }
        free(ip_address);
        free(ip_mask);
        free(ip_gateway);

        // IP4_ADDR(&ipaddr, 10, 1, 10, 234);
        // IP4_ADDR(&netmask, 255, 255, 255, 0);
        // IP4_ADDR(&gw, 10, 1, 10, 1);
    }

    /* Add netif interface for lpc17xx_8x */
    if (netifapi_netif_add(lpc_netif, &ipaddr, &netmask, &gw, NULL, lpc_enetif_init, tcpip_input) != ERR_OK) {
        printf("Network: Net interface failed to initialize\n");
    }
    netif_set_hostname(lpc_netif, hostname.c_str());
    netifapi_netif_set_default(lpc_netif);
    netifapi_netif_set_up(lpc_netif);

    /* Enable MAC interrupts only after LWIP is ready */
    NVIC_SetPriority(ETHERNET_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);
    NVIC_EnableIRQ(ETHERNET_IRQn);

    if(ip_address == nullptr) {
        netifapi_dhcp_start(lpc_netif);
        //dhcp_start(lpc_netif);
    }

    /* Initialize application(s) */
    if(enable_shell) {
        extern void shell_init(void);
        shell_init();
    }

    if(enable_ftpd) {
        ftpd_init();
    }

    if(enable_httpd) {
        extern void http_server_init(void);
        http_server_init();
    }

    /* This loop monitors the PHY link and will handle cable events
       via the PHY driver. */
    while (1) {
        /* Call the PHY status update state machine once in a while
           to keep the link status up-to-date */
        physts = lpcPHYStsPoll();

        /* Only check for connection state when the PHY status has changed */
        if (physts & PHY_LINK_CHANGED) {
            if (physts & PHY_LINK_CONNECTED) {
                prt_ip = 0;

                /* Set interface speed and duplex */
                if (physts & PHY_LINK_SPEED100) {
                    Chip_ENET_SetSpeed(LPC_ETHERNET, 1);
                    //NETIF_INIT_SNMP(lpc_netif, snmp_ifType_ethernet_csmacd, 100000000);
                    printf("Network::start() - 100Mbit/s ");
                } else {
                    Chip_ENET_SetSpeed(LPC_ETHERNET, 0);
                    //NETIF_INIT_SNMP(lpc_netif, snmp_ifType_ethernet_csmacd, 10000000);
                    printf("Network::start() - 10Mbit/s ");
                }
                if (physts & PHY_LINK_FULLDUPLX) {
                    Chip_ENET_SetDuplex(LPC_ETHERNET, true);
                    printf("Full Duplex\n");
                } else {
                    Chip_ENET_SetDuplex(LPC_ETHERNET, false);
                    printf("Half Duplex\n");
                }

                tcpip_callback_with_block((tcpip_callback_fn) netif_set_link_up,
                                          (void *) lpc_netif, 1);
                printf("Network::start() - Link connect status: UP\r\n");

            } else {
                tcpip_callback_with_block((tcpip_callback_fn) netif_set_link_down,
                                          (void *) lpc_netif, 1);
                printf("Network::start() - Link connect status: DOWN\r\n");
            }
        }

        /* Print IP address info */
        if (!prt_ip) {
            if (lpc_netif->ip_addr.addr) {
                char tmp_buff[16];
                printf("IP_ADDR    : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif->ip_addr, tmp_buff, 16));
                printf("NET_MASK   : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif->netmask, tmp_buff, 16));
                printf("GATEWAY_IP : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif->gw, tmp_buff, 16));
                printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    lpc_netif->hwaddr[0], lpc_netif->hwaddr[1], lpc_netif->hwaddr[2],
                    lpc_netif->hwaddr[3], lpc_netif->hwaddr[4], lpc_netif->hwaddr[5]);

                prt_ip = 1;
            }
        }

        /* Delay for link detection (250mS) */
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    netconn_thread_cleanup();
}

void Network::vSetupIFTask(void *arg)
{
    static_cast<Network*>(arg)->network_thread();
    vTaskDelete( NULL );
}

bool Network::start()
{
    printf("Network: starting\n");
    xTaskCreate(vSetupIFTask, "SetupIFx", 256, this, (tskIDLE_PRIORITY + 1UL), (xTaskHandle *) NULL);
    return true;
}
