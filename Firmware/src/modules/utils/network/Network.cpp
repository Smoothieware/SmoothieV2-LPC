#include "Network.h"

#include "lwip/init.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/memp.h"
#include "lwip/tcpip.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/timers.h"
#include "netif/etharp.h"

#if LWIP_DHCP
#include "lwip/dhcp.h"
#endif

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

REGISTER_MODULE(Network, Network::create)

/* NETIF data */
static struct netif lpc_netif;
bool Network::enable_shell = false;
bool Network::enable_httpd = false;
bool Network::enable_ftpd = false;

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
}

bool Network::configure(ConfigReader& cr)
{
	ConfigReader::section_map_t m;
	if(!cr.get_section("network", m)) return false;

	bool enable = cr.get_bool(m, network_enable_key, false);
	if(!enable) {
		return false;
	}

	enable_shell = cr.get_bool(m, shell_enable_key, false);
	enable_ftpd = cr.get_bool(m, ftp_enable_key, false);

	// register command handlers
	using std::placeholders::_1;
	using std::placeholders::_2;

	THEDISPATCHER->add_handler( "net", std::bind( &Network::handle_net_cmd, this, _1, _2) );

	return true;
}

#include "lwip/tcp.h"
#include "lwip/inet.h"
extern "C" struct tcp_pcb ** const tcp_pcb_lists[];
static unsigned long netstat(char *pcOut)
{
	char *pcStart = pcOut;
	enum tcp_state eState;
	u16_t local_port, remote_port;
	ip_addr_t local_ip, remote_ip;
	int j= 1;
	struct tcp_pcb *cpcb;
	for (int i = 0; i < 4; i++) {
    	for(cpcb = *tcp_pcb_lists[i]; cpcb != NULL; cpcb = cpcb->next) {
			pcOut += sprintf(pcOut, "%2u           ", j++);
			pcOut     += sprintf(pcOut, "TCP");
			pcOut     += sprintf(pcOut, "        ");        //spacer

			//OsIrqDisable();
			//get all states during disabled preemption so that the pcb pointer can't be nulled in between
			eState = cpcb->state;
			local_port = cpcb->local_port;
			local_ip = cpcb->local_ip;
			remote_port = cpcb->remote_port;
			remote_ip = cpcb->remote_ip;
			//OsIrqEnable();


			pcOut += sprintf(pcOut, "%15s", inet_ntoa(local_ip));
			pcOut     += sprintf(pcOut, "   ");                            //spacer
			pcOut     += sprintf(pcOut, "%5u", local_port);
			pcOut     += sprintf(pcOut, "        ");         //spacer

			if(remote_ip.addr != 0) {
				pcOut += sprintf(pcOut, "%15s", inet_ntoa(remote_ip));
				pcOut   += sprintf(pcOut, "   ");                     //spacer
				pcOut += sprintf(pcOut, "%5u", remote_port);

			} else {
				pcOut += sprintf(pcOut, "---.---.---.---");                   //empty IP
				pcOut   += sprintf(pcOut, "   ");                     //spacer
				pcOut   += sprintf(pcOut, "-----");                   //emtpy Port
			}

			pcOut     += sprintf(pcOut, "          SOCKET    ");     //socket API
			switch(eState) {
				case CLOSED:
					pcOut += sprintf(pcOut, "CLOSED");
					break;

				case LISTEN:
					pcOut += sprintf(pcOut, "LISTEN");
					break;

				case ESTABLISHED:
					pcOut += sprintf(pcOut, "ESTABLISHED");
					break;

				case FIN_WAIT_1:
					pcOut += sprintf(pcOut, "FIN_WAIT_1");
					break;

				case FIN_WAIT_2:
					pcOut += sprintf(pcOut, "FIN_WAIT_2");
					break;

				case CLOSE_WAIT:
					pcOut += sprintf(pcOut, "CLOSE_WAIT");
					break;

				case TIME_WAIT:
					pcOut += sprintf(pcOut, "TIME_WAIT");
					break;

				case SYN_SENT:
				case SYN_RCVD:
					pcOut += sprintf(pcOut, "CONNECTING");
					break;

				case CLOSING:
					pcOut += sprintf(pcOut, "CLOSING");
					break;

				case LAST_ACK:
					pcOut += sprintf(pcOut, "LAST_ACK");
					break;

				default:
					pcOut += sprintf(pcOut, "???");
					break;
			}

			// 	case NETCONN_UDP:
			// 		pcOut     += sprintf(pcOut, "UDP");
			// 		pcOut     += sprintf(pcOut, "        ");                                     //spacer
			// 		pcOut += sprintf(pcOut, "%15s", sock->conn->pcb.udp->local_ip);
			// 		pcOut     += sprintf(pcOut, "   ");                                         //spacer
			// 		pcOut     += sprintf(pcOut, "%5u", sock->conn->pcb.udp->local_port);
			// 		pcOut     += sprintf(pcOut, "        ");                                     //spacer
			// 		pcOut     += sprintf(pcOut, "               ");                 //empty foreign IP (n/a in UDP)
			// 		pcOut     += sprintf(pcOut, "   ");                            //spacer
			// 		pcOut     += sprintf(pcOut, "     ");                          //empty foreign port (n/a in UDP)
			// 		pcOut     += sprintf(pcOut, "          SOCKET");         //socket API
			// 		break;

			// 	default:
			// 		pcOut     += sprintf(pcOut, "???");
			// 		break;

			// }
			pcOut += sprintf(pcOut, "\r\n");
		}
	}
	return((unsigned long)(pcOut - pcStart));
}

#define HELP(m) if(params == "-h") { os.printf("%s\n", m); return true; }
bool Network::handle_net_cmd( std::string& params, OutputStream& os )
{
	HELP("net - show network status, -v also shows netstat");

	if(lpc_netif.flags & NETIF_FLAG_LINK_UP) {
		os.printf("Link UP\n");
		if (lpc_netif.ip_addr.addr) {
			static char tmp_buff[16];
			os.printf("IP_ADDR    : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif.ip_addr, tmp_buff, 16));
			os.printf("NET_MASK   : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif.netmask, tmp_buff, 16));
			os.printf("GATEWAY_IP : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif.gw, tmp_buff, 16));

		} else {
			os.printf("no ip set\n");
		}
	} else {
		os.printf("Link DOWN\n");
	}

	if(!params.empty()) {
		// do netstat-like dump
		os.puts("\nNetstat...\n");
		char *buf= (char *)malloc(2048);
		if(buf != NULL) {
			unsigned long n= netstat(buf);
			buf[n]= '\0';
			os.puts(buf);
			free(buf);
		}
	}

	os.set_no_response();

	return true;
}

/* Callback for TCPIP thread to indicate TCPIP init is done */
static void tcpip_init_done_signal(void *arg)
{
	/* Tell main thread TCP/IP init is done */
	*(s32_t *) arg = 1;
}

// used by various emac drivers
extern "C" void msDelay(uint32_t ms)
{
	vTaskDelay(pdMS_TO_TICKS(ms));
}

/* LWIP kickoff and PHY link monitor thread */
void Network::vSetupIFTask(void *pvParameters)
{
	ip_addr_t ipaddr, netmask, gw;
	volatile s32_t tcpipdone = 0;
	uint32_t physts;
	static int prt_ip = 0;

	/* Wait until the TCP/IP thread is finished before
	   continuing or wierd things may happen */
	printf("Network: Waiting for TCPIP thread to initialize...\n");
	tcpip_init(tcpip_init_done_signal, (void *) &tcpipdone);
	while (!tcpipdone) {
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	printf("Network: Starting LWIP TCP echo server...\n");

	/* Static IP assignment */
#if LWIP_DHCP
	IP4_ADDR(&gw, 0, 0, 0, 0);
	IP4_ADDR(&ipaddr, 0, 0, 0, 0);
	IP4_ADDR(&netmask, 0, 0, 0, 0);
#else
	IP4_ADDR(&gw, 10, 1, 10, 1);
	IP4_ADDR(&ipaddr, 10, 1, 10, 234);
	IP4_ADDR(&netmask, 255, 255, 255, 0);
#endif

	/* Add netif interface for lpc17xx_8x */
	if (!netif_add(&lpc_netif, &ipaddr, &netmask, &gw, NULL, lpc_enetif_init,
	               tcpip_input)) {
		printf("Network: Net interface failed to initialize\n");
	}

	netif_set_default(&lpc_netif);
	netif_set_up(&lpc_netif);

	/* Enable MAC interrupts only after LWIP is ready */
	NVIC_SetPriority(ETHERNET_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);
	NVIC_EnableIRQ(ETHERNET_IRQn);

#if LWIP_DHCP
	dhcp_start(&lpc_netif);
#endif

	/* Initialize application(s) */
	if(enable_shell) {
		extern void shell_init(void);
		shell_init();
	}

	if(enable_ftpd) {
		ftpd_init();
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
					NETIF_INIT_SNMP(&lpc_netif, snmp_ifType_ethernet_csmacd, 100000000);
					printf("Network::start() - 100Mbit/s ");
				} else {
					Chip_ENET_SetSpeed(LPC_ETHERNET, 0);
					NETIF_INIT_SNMP(&lpc_netif, snmp_ifType_ethernet_csmacd, 10000000);
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
				                          (void *) &lpc_netif, 1);
				printf("Network::start() - Link connect status: UP\r\n");

			} else {
				tcpip_callback_with_block((tcpip_callback_fn) netif_set_link_down,
				                          (void *) &lpc_netif, 1);
				printf("Network::start() - Link connect status: DOWN\r\n");
			}
		}

		/* Delay for link detection (250mS) */
		vTaskDelay(pdMS_TO_TICKS(250));

		/* Print IP address info */
		if (!prt_ip) {
			if (lpc_netif.ip_addr.addr) {
				char tmp_buff[16];
				printf("IP_ADDR    : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif.ip_addr, tmp_buff, 16));
				printf("NET_MASK   : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif.netmask, tmp_buff, 16));
				printf("GATEWAY_IP : %s\r\n", ipaddr_ntoa_r((const ip_addr_t *) &lpc_netif.gw, tmp_buff, 16));
				prt_ip = 1;
			}
		}
	}
}

bool Network::start()
{
	printf("Network: running start\n");
	xTaskCreate(vSetupIFTask, "SetupIFx", 512, NULL, (tskIDLE_PRIORITY + 1UL), (xTaskHandle *) NULL);
	return true;
}
