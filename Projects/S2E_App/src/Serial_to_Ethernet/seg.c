#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "W7500x_wztoe.h"
#include "W7500x_gpio.h"
#include "W7500x_board.h"
#include "socket.h"
#include "seg.h"
#include "timerHandler.h"
#include "uartHandler.h"
#include "gpioHandler.h"

/* Private define ------------------------------------------------------------*/
// Ring Buffer
BUFFER_DECLARATION(data_rx_0);
BUFFER_DECLARATION(data_rx_1);
/* Private variables ---------------------------------------------------------*/
uint8_t flag_s2e_application_running = 0;

uint8_t opmode = DEVICE_GW_MODE;
static uint8_t mixed_state[1] = {MIXED_SERVER,};
static uint8_t sw_modeswitch_at_mode_on = SEG_DISABLE;
static uint16_t client_any_port[1] = {0,};

// Timer Enable flags / Time
uint8_t enable_inactivity_timer[1] = {SEG_DISABLE,};
volatile uint16_t inactivity_time[1] = {0,};

uint8_t enable_keepalive_timer[1] = {SEG_DISABLE,};
volatile uint16_t keepalive_time[1] = {0,};

uint8_t enable_modeswitch_timer = SEG_DISABLE;
volatile uint16_t modeswitch_time = 0;
volatile uint16_t modeswitch_gap_time = DEFAULT_MODESWITCH_INTER_GAP;

uint8_t enable_reconnection_timer[1] = {SEG_DISABLE,};
volatile uint16_t reconnection_time[1] = {0,};

uint8_t enable_serial_input_timer[1] = {SEG_DISABLE,};
volatile uint16_t serial_input_time[1] = {0,};
uint8_t flag_serial_input_time_elapse[1] = {SEG_DISABLE,}; // for Time delimiter

// added for auth timeout
uint8_t enable_connection_auth_timer[1] = {SEG_DISABLE,};
volatile uint16_t connection_auth_time[1] = {0,};

// flags
uint8_t flag_connect_pw_auth[1] = {SEG_DISABLE,}; // TCP_SERVER_MODE only
uint8_t flag_sent_keepalive[1] = {SEG_DISABLE,};
uint8_t flag_sent_first_keepalive[1] = {SEG_DISABLE,};

// static variables for function: check_modeswitch_trigger()
static uint8_t triggercode_idx;
static uint8_t ch_tmp[3];

// User's buffer / size idx
extern uint8_t g_send_buf[1][DATA_BUF_SIZE];
extern uint8_t g_recv_buf[1][DATA_BUF_SIZE];
uint16_t u2e_size[1] = {0,};
uint16_t e2u_size[1] = {0,};

// S2E Data byte count variables
volatile uint32_t s2e_uart_rx_bytecount[1] = {0,};
volatile uint32_t s2e_uart_tx_bytecount[1] = {0,};
volatile uint32_t s2e_ether_rx_bytecount[1] = {0,};
volatile uint32_t s2e_ether_tx_bytecount[1] = {0,};

// UDP: Peer netinfo
uint8_t peerip[4] = {0, };
uint8_t peerip_tmp[4] = {0xff, };
uint16_t peerport = 0;

// XON/XOFF (Software flow control) flag, Serial data can be transmitted to peer when XON enabled. 
uint8_t isXON = SEG_ENABLE;

char * str_working[] = {"TCP_CLIENT_MODE", "TCP_SERVER_MODE", "TCP_MIXED_MODE", "UDP_MODE"};

uint8_t flag_process_dhcp_success = OFF;
uint8_t flag_process_dns_success = OFF;

uint8_t isSocketOpen_TCPclient[1] = {OFF,};

// ## timeflag for debugging
uint8_t tmp_timeflag_for_debug = 0;

/* Private functions prototypes ----------------------------------------------*/
void proc_SEG_tcp_client(uint8_t sock);
void proc_SEG_tcp_server(uint8_t sock);
void proc_SEG_tcp_mixed(uint8_t sock);
void proc_SEG_udp(uint8_t sock);

void uart_to_ether(uint8_t sock);
void ether_to_uart(uint8_t sock);
uint16_t get_serial_data(uint8_t socket);
void reset_SEG_timeflags(uint8_t socket);
uint8_t check_connect_pw_auth(uint8_t * buf, uint16_t len);
void restore_serial_data(uint8_t idx);

uint8_t check_tcp_connect_exception(uint8_t socket);

void set_device_status(uint8_t socket, teDEVSTATUS);
uint16_t get_tcp_any_port(uint8_t sock);

// UART tx/rx and Ethernet tx/rx data transfer bytes counter
void add_data_transfer_bytecount(uint8_t socket, teDATADIR dir, uint16_t len);

/* Public & Private functions ------------------------------------------------*/

void do_seg(uint8_t sock)
{
	//DevConfig *s2e = get_DevConfig_pointer();
	struct __network_connection *network_connection = (struct __network_connection *)get_DevConfig_pointer()->network_connection;
	struct __serial_option *serial_option = (struct __serial_option *)get_DevConfig_pointer()->serial_option;
	struct __firmware_update *firmware_update = (struct __firmware_update *)&(get_DevConfig_pointer()->firmware_update);
	
	uint8_t i;
	
//#ifdef _SEG_DEBUG_
#if 1
	if(tmp_timeflag_for_debug) // every 1 sec
	{
		//if(opmode == DEVICE_GW_MODE) 	printf("working mode: %s, mixed: %s\r\n", str_working[net->working_mode], (net->working_mode == 2)?(mixed_state ? "CLIENT":"SERVER"):("NONE"));
		//else 							printf("opmode: DEVICE_AT_MODE\r\n");
		//printf("UART2Ether - ringbuf: %d, rd: %d, wr: %d, u2e_size: %d\r\n", BUFFER_USED_SIZE(data_rx), data_rx_rd, data_rx_wr, u2e_size);
		//printf("UART2Ether - ringbuf: %d, rd: %d, wr: %d, u2e_size: %d peer xon/off: %s\r\n", BUFFER_USED_SIZE(data_rx), data_rx_rd, data_rx_wr, u2e_size, isXON?"XON":"XOFF");
		//printf("modeswitch_time [%d] : modeswitch_gap_time [%d]\r\n", modeswitch_time, modeswitch_gap_time);
		//printf("[%d]: [%d] ", modeswitch_time, modeswitch_gap_time);
		//printf("idx = %d\r\n", triggercode_idx);
		//printf("opmode: %d\r\n", opmode);
		//printf("flag_connect_pw_auth: %d\r\n", flag_connect_pw_auth);
		//printf("UART2Ether - ringbuf: %d, u2e_size: %d\r\n", BUFFER_USED_SIZE(data_rx), u2e_size);
		//printf("Ether2UART - RX_RSR: %d, e2u_size: %d\r\n", getSn_RX_RSR(sock), e2u_size);
		//printf("sock_state: %x\r\n", getSn_SR(sock));
		//printf("\r\nringbuf_usedlen = %d\r\n", BUFFER_USED_SIZE(data_rx));
		//printf(" >> UART: [Rx] %u / [Tx] %u\r\n", get_data_transfer_bytecount(SEG_UART_RX), get_data_transfer_bytecount(SEG_UART_TX));
		//printf(" >> ETHER: [Rx] %u / [Tx] %u\r\n", get_data_transfer_bytecount(SEG_ETHER_RX), get_data_transfer_bytecount(SEG_ETHER_TX));
		//printf(" >> RINGBUFFER_USED_SIZE: [Rx] %d\r\n", BUFFER_USED_SIZE(data_rx));
		tmp_timeflag_for_debug = 0;
	}
#endif
	
	// Firmware update: Do not run SEG process
	if(firmware_update->fwup_flag == SEG_ENABLE) 
    {
        return;
    }
	
	// Serial AT command mode enabled, initial settings
	if((opmode == DEVICE_GW_MODE) && (sw_modeswitch_at_mode_on == SEG_ENABLE))
	{
		for(i=0; i<2; i++)
		{
			// Socket disconnect (TCP only) / clos
			process_socket_termination(i);
		}
		// Mode switch
		init_trigger_modeswitch(DEVICE_AT_MODE);
		// Mode switch flag disabled
		sw_modeswitch_at_mode_on = SEG_DISABLE;
	}
	
	if(opmode == DEVICE_GW_MODE) 
	{
		switch(network_connection[sock].working_mode)
		{
			case TCP_CLIENT_MODE:
				proc_SEG_tcp_client(sock);
				break;
			
			case TCP_SERVER_MODE:
				proc_SEG_tcp_server(sock);
				break;
			
			case TCP_MIXED_MODE:
				proc_SEG_tcp_mixed(sock);
				break;
			
			case UDP_MODE:
				proc_SEG_udp(sock);
				break;
			
			default:
				break;
		}
		
		// XON/XOFF Software flow control: Check the Buffer usage and Send the start/stop commands
		// [WIZnet Device] -> [Peer]
		if((serial_option[sock].flow_control == flow_xon_xoff) || serial_option[sock].flow_control == flow_rts_cts) 
        {
            check_uart_flow_control(sock, serial_option[sock].flow_control);
        }
	}
}

void set_device_status(uint8_t socket, teDEVSTATUS status)
{
	struct __network_connection *network_connection = (struct __network_connection *)get_DevConfig_pointer()->network_connection;
	
	switch(status)
	{
		case ST_OPEN:		// TCP connection: disconnected (or UDP mode)
			network_connection[socket].working_state = ST_OPEN;
			break;
		
		case ST_CONNECT:	// TCP connection: connected
			network_connection[socket].working_state = ST_CONNECT;
			break;
		
		case ST_UPGRADE:	// TCP connection: disconnected
			network_connection[socket].working_state = ST_UPGRADE;
			break;
		
		case ST_ATMODE:		// TCP connection: disconnected
			network_connection[socket].working_state = ST_ATMODE;
			break;
		
		case ST_UDP:		// UDP mode
			network_connection[socket].working_state = ST_UDP;
		default:
			break;
	}
	
	// Status indicator pins
	if(network_connection[socket].working_state == ST_CONNECT)
	{
		set_connection_status_io(STATUS_TCPCONNECT_PIN, ON); // Status I/O pin to low
	}
	else
	{
		set_connection_status_io(STATUS_TCPCONNECT_PIN, OFF); // Status I/O pin to high
	}
}

uint8_t get_device_status(void)
{
	struct __network_connection *network_connection = (struct __network_connection *)get_DevConfig_pointer()->network_connection;
    
	return network_connection[0].working_state;
}


void proc_SEG_udp(uint8_t sock)
{
	//DevConfig *s2e = get_DevConfig_pointer();
	struct __network_connection *network_connection = (struct __network_connection *)get_DevConfig_pointer()->network_connection;
	struct __serial_common *serial_common = (struct __serial_common *)&get_DevConfig_pointer()->serial_common;
    struct __serial_data_packing *serial_data_packing = (struct __serial_data_packing *)get_DevConfig_pointer()->serial_data_packing;
	
	uint8_t state = getSn_SR(sock);
    
	switch(state)
	{
		case SOCK_UDP:
			if(M_BUFFER_USED_SIZE(sock) || u2e_size[sock])	
            {
                uart_to_ether(sock);
            }
			if(getSn_RX_RSR(sock) 	|| e2u_size[sock])		
            {
                ether_to_uart(sock);
            }
			break;
			
		case SOCK_CLOSED:
			//reset_SEG_timeflags(sock);
			BUFFER_CLEAR(data_rx_0);
		
			u2e_size[sock] = 0;
			e2u_size[sock] = 0;
			
			if(socket(sock, Sn_MR_UDP, network_connection[sock].local_port, 0) == sock)
			{
				set_device_status(sock, ST_UDP);
				
				if(serial_data_packing[sock].packing_time) 
                {
                    modeswitch_gap_time = serial_data_packing[sock].packing_time; // replace the GAP time (default: 500ms)
                }
				
				if(serial_common->serial_debug_en == SEG_ENABLE)
				{
					printf(" > SEG:UDP_MODE:SOCKOPEN\r\n");
				}
			}
			break;
		default:
			break;
	}
}

void proc_SEG_tcp_client(uint8_t sock)
{
	//DevConfig *s2e = get_DevConfig_pointer();
	struct __tcp_option *tcp_option = (struct __tcp_option *)get_DevConfig_pointer()->tcp_option;
	struct __network_connection *network_connection = (struct __network_connection *)get_DevConfig_pointer()->network_connection;
	struct __serial_command *serial_command = (struct __serial_command *)&get_DevConfig_pointer()->serial_command;
	struct __serial_common *serial_common = (struct __serial_common *)&get_DevConfig_pointer()->serial_common;
	struct __serial_data_packing *serial_data_packing = (struct __serial_data_packing *)(get_DevConfig_pointer()->serial_data_packing);
	
	uint16_t source_port;
	uint8_t destip[4] = {0, };
	uint16_t destport = 0;
	
	uint8_t state = getSn_SR(sock);
    
	switch(state)
	{
		case SOCK_INIT:
			if(reconnection_time[sock] >= tcp_option[sock].reconnection)
			{
				reconnection_time[sock] = 0; // reconnection time variable clear
				
				// TCP connect exception checker; e.g., dns failed / zero srcip ... and etc.
				if(check_tcp_connect_exception(sock) == ON) 
                {
                    return;
                }
				
				// TCP connect
				connect(sock, network_connection[sock].remote_ip, network_connection[sock].remote_port);
#ifdef _SEG_DEBUG_
				printf(" > SEG:TCP_CLIENT_MODE:CLIENT_CONNECTION\r\n");
#endif
			}
			break;
		
		case SOCK_ESTABLISHED:
			if(getSn_IR(sock) & Sn_IR_CON)
			{
				///////////////////////////////////////////////////////////////////////////////////////////////////
				// S2E: TCP client mode initialize after connection established (only once)
				///////////////////////////////////////////////////////////////////////////////////////////////////
				//net->state = ST_CONNECT;
				set_device_status(sock, ST_CONNECT);
				
				if(!inactivity_time[sock] && tcp_option[sock].inactivity)		
                {
                    enable_inactivity_timer[sock] = SEG_ENABLE;
                }
				if(!keepalive_time[sock] && tcp_option[sock].keepalive_en)	
                {
                    enable_keepalive_timer[sock] = SEG_ENABLE;
                }
				
				// TCP server mode only, This flag have to be enabled always at TCP client mode
				//if(option->pw_connect_en == SEG_ENABLE)		flag_connect_pw_auth = SEG_ENABLE;
				flag_connect_pw_auth[sock] = SEG_ENABLE;
				
				// Reconnection timer disable
				if(enable_reconnection_timer[sock] == SEG_ENABLE)
				{
					enable_reconnection_timer[sock] = SEG_DISABLE;
					reconnection_time[sock] = 0;
				}
				
				// Serial debug message printout
				if(serial_common->serial_debug_en == SEG_ENABLE)
				{
					getsockopt(sock, SO_DESTIP, &destip);
					getsockopt(sock, SO_DESTPORT, &destport);
					printf(" > SEG:CONNECTED TO - %d.%d.%d.%d : %d\r\n",destip[0], destip[1], destip[2], destip[3], destport);
				}
				
				// UART Ring buffer clear
				BUFFER_CLEAR(data_rx_0);
				
				// Debug message enable flag: TCP client sokect open 
				isSocketOpen_TCPclient[sock] = OFF;
				
				setSn_IR(sock, Sn_IR_CON);
			}
			
			// Serial to Ethernet process
			if(M_BUFFER_USED_SIZE(sock) || u2e_size[sock])	
			{
					uart_to_ether(sock);
			}
			if(getSn_RX_RSR(sock) 	|| e2u_size)		
			{
					ether_to_uart(sock);
			}
			
			// Check the inactivity timer
			if((enable_inactivity_timer[sock] == SEG_ENABLE) && (inactivity_time[sock] >= tcp_option[sock].inactivity))
			{
				//disconnect(sock);
				process_socket_termination(sock);
				
				// Keep-alive timer disabled
				enable_keepalive_timer[sock] = DISABLE;
				keepalive_time[sock] = 0;
#ifdef _SEG_DEBUG_
				printf(" > INACTIVITY TIMER: TIMEOUT\r\n");
#endif
			}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  
			
			// Check the keee-alive timer
			if((tcp_option[sock].keepalive_en == SEG_ENABLE) && (enable_keepalive_timer[sock] == SEG_ENABLE))
			{
				// Send the first keee-alive packet
				if((flag_sent_first_keepalive[sock] == SEG_DISABLE) 
                    && (keepalive_time[sock] >= tcp_option[sock].keepalive_wait_time) 
                    && (tcp_option[sock].keepalive_wait_time != 0))
				{
#ifdef _SEG_DEBUG_
					printf(" >> send_keepalive_packet_first [%d]\r\n", keepalive_time);
#endif
					send_keepalive_packet_manual(sock); // <-> send_keepalive_packet_auto()
					keepalive_time[sock] = 0;
					
					flag_sent_first_keepalive[sock] = SEG_ENABLE;
				}
				// Send the keee-alive packet periodically
				if((flag_sent_first_keepalive[sock] == SEG_ENABLE) 
                    && (keepalive_time[sock] >= tcp_option[sock].keepalive_retry_time) 
                    && (tcp_option[sock].keepalive_retry_time != 0))
				{
#ifdef _SEG_DEBUG_
					printf(" >> send_keepalive_packet_manual [%d]\r\n", keepalive_time[sock]);
#endif
					send_keepalive_packet_manual(sock);
					keepalive_time[sock] = 0;
				}
			}
			
			break;
		
		case SOCK_CLOSE_WAIT:
			while(getSn_RX_RSR(sock) || e2u_size) 
            {
                ether_to_uart(sock); // receive remaining packets
            }
			disconnect(sock);
			break;
		
		case SOCK_FIN_WAIT:
		case SOCK_CLOSED:
			set_device_status(sock, ST_OPEN);
			reset_SEG_timeflags(sock);
			
			u2e_size[sock] = 0;
			e2u_size[sock] = 0;
			
			source_port = get_tcp_any_port(sock);
#ifdef _SEG_DEBUG_
			printf(" > TCP CLIENT: client_any_port = %d\r\n", client_any_port[sock]);
#endif		
			if(socket(sock, Sn_MR_TCP, source_port, Sn_MR_ND) == sock)
			{
				// Replace the command mode switch code GAP time (default: 500ms)
				if((serial_command->serial_command == SEG_ENABLE) && serial_data_packing[0].packing_time) 
                {
                    modeswitch_gap_time = serial_data_packing[0].packing_time;
                }
				
				// Enable the reconnection Timer
				if((enable_reconnection_timer[sock] == SEG_DISABLE) && tcp_option[sock].reconnection) 
                {
                    enable_reconnection_timer[sock] = SEG_ENABLE;
                }
				
				if(serial_common->serial_debug_en == SEG_ENABLE)
				{
					if(isSocketOpen_TCPclient[sock] == OFF)
					{
						printf(" > SEG:TCP_CLIENT_MODE:SOCKOPEN\r\n");
						isSocketOpen_TCPclient[sock] = ON;
					}
				}
			}
		
			break;
			
		default:
			break;
	}
}

void proc_SEG_tcp_server(uint8_t sock)
{
	//DevConfig *s2e = get_DevConfig_pointer();
	struct __tcp_option *tcp_option = (struct __tcp_option *)get_DevConfig_pointer()->tcp_option;
	struct __serial_common *serial_common = (struct __serial_common *)&get_DevConfig_pointer()->serial_common;
	struct __network_connection *network_connection = (struct __network_connection *)(get_DevConfig_pointer()->network_connection);
	struct __serial_command *serial_command = (struct __serial_command *)&get_DevConfig_pointer()->serial_command;
    struct __serial_data_packing *serial_data_packing = (struct __serial_data_packing *)get_DevConfig_pointer()->serial_data_packing;
    
	uint8_t destip[4] = {0, };
	uint16_t destport = 0;
	
	uint8_t state = getSn_SR(sock);
	switch(state)
	{
		case SOCK_INIT:
			//listen(sock); Function call Immediately after socket open operation
			break;
		
		case SOCK_LISTEN:
			break;
		
		case SOCK_ESTABLISHED:
			if(getSn_IR(sock) & Sn_IR_CON)
			{
				///////////////////////////////////////////////////////////////////////////////////////////////////
				// S2E: TCP server mode initialize after connection established (only once)
				///////////////////////////////////////////////////////////////////////////////////////////////////
				//net->state = ST_CONNECT;
				set_device_status(sock, ST_CONNECT);
				
				if(!inactivity_time[sock] && tcp_option[0].inactivity)		
                {
                    enable_inactivity_timer[sock] = SEG_ENABLE;
                }
				//if(!keepalive_time && net->keepalive_en)	enable_keepalive_timer = SEG_ENABLE;
				
				if(tcp_option[0].pw_connect_en == SEG_DISABLE)	
                {
                    flag_connect_pw_auth[sock] = SEG_ENABLE;		// TCP server mode only (+ mixed_server)
                }
				else
				{
					// Connection password auth timer initialize
					enable_connection_auth_timer[sock] = SEG_ENABLE;
					connection_auth_time[sock]  = 0;
				}
				
				// Serial debug message printout
				if(serial_common->serial_debug_en == SEG_ENABLE)
				{
					getsockopt(sock, SO_DESTIP, &destip);
					getsockopt(sock, SO_DESTPORT, &destport);
					printf(" > SEG:CONNECTED FROM - %d.%d.%d.%d : %d\r\n",destip[0], destip[1], destip[2], destip[3], destport);
				}
				
				// UART Ring buffer clear
				BUFFER_CLEAR(data_rx_0);
				
				setSn_IR(sock, Sn_IR_CON);
			}
			
			// Serial to Ethernet process
			if(M_BUFFER_USED_SIZE(sock) || u2e_size[sock])	
            {
                uart_to_ether(sock);
            }
			if(getSn_RX_RSR(sock) || e2u_size[sock])	
            {
                ether_to_uart(sock);
            }
			
			// Check the inactivity timer
			if((enable_inactivity_timer[sock] == SEG_ENABLE) && (inactivity_time[sock] >= tcp_option[0].inactivity))
			{
				//disconnect(sock);
				process_socket_termination(sock);
				
				// Keep-alive timer disabled
				enable_keepalive_timer[sock] = DISABLE;
				keepalive_time[sock] = 0;
#ifdef _SEG_DEBUG_
				printf(" > INACTIVITY TIMER: TIMEOUT\r\n");
#endif
			}
			
			// Check the keee-alive timer
			if((tcp_option[0].keepalive_en == SEG_ENABLE) && (enable_keepalive_timer[sock] == SEG_ENABLE))
			{
				// Send the first keee-alive packet
				if((flag_sent_first_keepalive[sock] == SEG_DISABLE) 
                    && (keepalive_time[sock] >= tcp_option[0].keepalive_wait_time) 
                    && (tcp_option[0].keepalive_wait_time != 0))
				{
#ifdef _SEG_DEBUG_
					printf(" >> send_keepalive_packet_first [%d]\r\n", keepalive_time[sock]);
#endif
					send_keepalive_packet_manual(sock); // <-> send_keepalive_packet_auto()
					keepalive_time[sock] = 0;
					
					flag_sent_first_keepalive[sock] = SEG_ENABLE;
				}
				// Send the keee-alive packet periodically
				if((flag_sent_first_keepalive[sock] == SEG_ENABLE) 
                    && (keepalive_time[sock] >= tcp_option[0].keepalive_retry_time) 
                    && (tcp_option[0].keepalive_retry_time != 0))
				{
#ifdef _SEG_DEBUG_
					printf(" >> send_keepalive_packet_manual [%d]\r\n", keepalive_time[sock]);
#endif
					send_keepalive_packet_manual(sock);
					keepalive_time[sock] = 0;
				}
			}
			
			// Check the connection password auth timer
			if(tcp_option[0].pw_connect_en == SEG_ENABLE)
			{
				if((flag_connect_pw_auth[sock] == SEG_DISABLE) && (connection_auth_time[sock] >= MAX_CONNECTION_AUTH_TIME)) // timeout default: 5000ms (5 sec)
				{
					//disconnect(sock);
					process_socket_termination(sock);
					
					enable_connection_auth_timer[sock] = DISABLE;
					connection_auth_time[sock] = 0;
#ifdef _SEG_DEBUG_
					printf(" > CONNECTION PW: AUTH TIMEOUT\r\n");
#endif
				}
			}
			break;
		
		case SOCK_CLOSE_WAIT:
			while(getSn_RX_RSR(sock) || e2u_size) 
            {
                ether_to_uart(sock); // receive remaining packets
            }
			disconnect(sock);
			break;
		
		case SOCK_FIN_WAIT:
		case SOCK_CLOSED:
			set_device_status(sock, ST_OPEN);
			reset_SEG_timeflags(sock);
			
			u2e_size[sock] = 0;
			e2u_size[sock] = 0;

			if(socket(sock, Sn_MR_TCP, network_connection[sock].local_port, Sn_MR_ND) == sock)
			{
				// Replace the command mode switch code GAP time (default: 500ms)
				if((serial_command->serial_command == SEG_ENABLE) && serial_data_packing[sock].packing_time) 
                {
                    modeswitch_gap_time = serial_data_packing[sock].packing_time;
                }
				
				// TCP Server listen
				listen(sock);
				
				if(serial_common->serial_debug_en == SEG_ENABLE)
				{
					printf(" > SEG:TCP_SERVER_MODE:SOCKOPEN\r\n");
				}
			}
			break;
			
		default:
			break;
	}
}


void proc_SEG_tcp_mixed(uint8_t sock)
{
	//DevConfig *s2e = get_DevConfig_pointer();
	struct __tcp_option *tcp_option = (struct __tcp_option *)get_DevConfig_pointer()->tcp_option;
    struct __network_connection *network_connection = (struct __network_connection *)get_DevConfig_pointer()->network_connection;
    struct __serial_common *serial_common = (struct __serial_common *)&get_DevConfig_pointer()->serial_common;
    struct __serial_command *serial_command = (struct __serial_command *)&get_DevConfig_pointer()->serial_command;
    struct __serial_data_packing *serial_data_packing = (struct __serial_data_packing *)&get_DevConfig_pointer()->serial_data_packing;
    
	uint16_t source_port = 0;
	uint8_t destip[4] = {0, };
	uint16_t destport = 0;
	
#ifdef MIXED_CLIENT_LIMITED_CONNECT
	static uint8_t reconnection_count;
#endif
	
	uint8_t state = getSn_SR(sock);
	switch(state)
	{
		case SOCK_INIT:
			if(mixed_state[sock] == MIXED_CLIENT)
			{
				if(reconnection_time[sock] >= tcp_option[0].reconnection)
				{
					reconnection_time[sock] = 0; // reconnection time variable clear
					
					// TCP connect exception checker; e.g., dns failed / zero srcip ... and etc.
					if(check_tcp_connect_exception(sock) == ON)
					{
#ifdef MIXED_CLIENT_LIMITED_CONNECT
						process_socket_termination(sock);
						reconnection_count = 0;
						BUFFER_CLEAR(data_rx_0);
						mixed_state[sock] = MIXED_SERVER;
#endif
						return;
					}
					
					// TCP connect
					connect(sock, network_connection[sock].remote_ip, network_connection[sock].remote_port);
					
#ifdef MIXED_CLIENT_LIMITED_CONNECT
					reconnection_count++;
					if(reconnection_count >= MAX_RECONNECTION_COUNT)
					{
						process_socket_termination(sock);
						reconnection_count = 0;
						BUFFER_CLEAR(data_rx_0);
						mixed_state[sock] = MIXED_SERVER;
					}
	#ifdef _SEG_DEBUG_
					if(reconnection_count != 0)	
                    {
                        printf(" > SEG:TCP_MIXED_MODE:CLIENT_CONNECTION [%d]\r\n", reconnection_count);
                    }
					else						
                    {
                        printf(" > SEG:TCP_MIXED_MODE:CLIENT_CONNECTION_RETRY FAILED\r\n");
                    }
	#endif
#endif
				}
			}			
			break;
		
		case SOCK_LISTEN:
			// UART Rx interrupt detection in MIXED_SERVER mode
			// => Switch to MIXED_CLIENT mode
			if((mixed_state == MIXED_SERVER) && (M_BUFFER_USED_SIZE(sock)))
			{
				process_socket_termination(sock);
				mixed_state[sock] = MIXED_CLIENT;
				
				reconnection_time[sock] = tcp_option[0].reconnection; // rapid initial connection
			}
			break;
		
		case SOCK_ESTABLISHED:
			if(getSn_IR(sock) & Sn_IR_CON)
			{
				///////////////////////////////////////////////////////////////////////////////////////////////////
				// S2E: TCP mixed (server or client) mode initialize after connection established (only once)
				///////////////////////////////////////////////////////////////////////////////////////////////////
				//net->state = ST_CONNECT;
				set_device_status(sock, ST_CONNECT);
				
				if(!inactivity_time[sock] && tcp_option[sock].inactivity)		
                {
                    enable_inactivity_timer[sock] = SEG_ENABLE;
                }
				if(!keepalive_time[sock] && tcp_option[sock].keepalive_en)	
                {
                    enable_keepalive_timer[sock] = SEG_ENABLE;
                }
				
				// Connection Password option: TCP server mode only (+ mixed_server)
				if((tcp_option[sock].pw_connect_en == SEG_DISABLE) || (mixed_state[sock] == MIXED_CLIENT))
				{
					flag_connect_pw_auth[sock] = SEG_ENABLE;
				}
				else if((mixed_state[sock] == MIXED_SERVER) && (flag_connect_pw_auth[sock] == SEG_DISABLE))
				{
					// Connection password auth timer initialize
					enable_connection_auth_timer[sock] = SEG_ENABLE;
					connection_auth_time[sock]  = 0;
				}
				
				// Serial debug message printout
				if(serial_common->serial_debug_en == SEG_ENABLE)
				{
					getsockopt(sock, SO_DESTIP, &destip);
					getsockopt(sock, SO_DESTPORT, &destport);
					
					if(mixed_state[sock] == MIXED_SERVER)		
                    {
                        printf(" > SEG:CONNECTED FROM - %d.%d.%d.%d : %d\r\n",destip[0], destip[1], destip[2], destip[3], destport);
                    }
					else								
                    {
                        printf(" > SEG:CONNECTED TO - %d.%d.%d.%d : %d\r\n",destip[0], destip[1], destip[2], destip[3], destport);
                    }
				}
				
				
				// Mixed mode option init
				if(mixed_state[sock] == MIXED_SERVER)
				{
					// UART Ring buffer clear
					BUFFER_CLEAR(data_rx_0);
				}
				else if(mixed_state[sock] == MIXED_CLIENT)
				{
					// Mixed-mode flag switching in advance
					mixed_state[sock] = MIXED_SERVER;
				}
				
#ifdef MIXED_CLIENT_LIMITED_CONNECT
				reconnection_count = 0;
#endif
				
				setSn_IR(sock, Sn_IR_CON);
			}
			
			// Serial to Ethernet process
			if(M_BUFFER_USED_SIZE(sock) || u2e_size[sock])	
            {
                uart_to_ether(sock);
            }
			if(getSn_RX_RSR(sock) 	|| e2u_size[sock])		
            {
                ether_to_uart(sock);
            }
			
			// Check the inactivity timer
			if((enable_inactivity_timer[sock] == SEG_ENABLE) && (inactivity_time[sock] >= tcp_option[sock].inactivity))
			{
				//disconnect(sock);
				process_socket_termination(sock);
				
				// Keep-alive timer disabled
				enable_keepalive_timer[sock] = DISABLE;
				keepalive_time[sock] = 0;
#ifdef _SEG_DEBUG_
				printf(" > INACTIVITY TIMER: TIMEOUT\r\n");
#endif
				// TCP mixed mode state transition: initial state
				mixed_state[sock] = MIXED_SERVER;
			}
			
			// Check the keee-alive timer
			if((tcp_option[sock].keepalive_en == SEG_ENABLE) && (enable_keepalive_timer[sock] == SEG_ENABLE))
			{
				// Send the first keee-alive packet
				if((flag_sent_first_keepalive[sock] == SEG_DISABLE) 
                    && (keepalive_time[sock] >= tcp_option[sock].keepalive_wait_time) 
                    && (tcp_option[sock].keepalive_wait_time != 0))
				{
#ifdef _SEG_DEBUG_
					printf(" >> send_keepalive_packet_first [%d]\r\n", keepalive_time[sock]);
#endif
					send_keepalive_packet_manual(sock); // <-> send_keepalive_packet_auto()
					keepalive_time[sock] = 0;
					
					flag_sent_first_keepalive[sock] = SEG_ENABLE;
				}
				// Send the keee-alive packet periodically
				if((flag_sent_first_keepalive[sock] == SEG_ENABLE) 
                    && (keepalive_time[sock] >= tcp_option[sock].keepalive_retry_time) 
                    && (tcp_option[sock].keepalive_retry_time != 0))
				{
#ifdef _SEG_DEBUG_
					printf(" >> send_keepalive_packet_manual [%d]\r\n", keepalive_time[sock]);
#endif
					send_keepalive_packet_manual(sock);
					keepalive_time[sock] = 0;
				}
			}
			
			// Check the connection password auth timer
			if((mixed_state[sock] == MIXED_SERVER) && (tcp_option[sock].pw_connect_en == SEG_ENABLE))
			{
				if((flag_connect_pw_auth[sock] == SEG_DISABLE) && (connection_auth_time[sock] >= MAX_CONNECTION_AUTH_TIME)) // timeout default: 5000ms (5 sec)
				{
					//disconnect(sock);
					process_socket_termination(sock);
					
					enable_connection_auth_timer[sock] = DISABLE;
					connection_auth_time[sock] = 0;
#ifdef _SEG_DEBUG_
					printf(" > CONNECTION PW: AUTH TIMEOUT\r\n");
#endif
				}
			}
			break;
		
		case SOCK_CLOSE_WAIT:
			while(getSn_RX_RSR(sock) || e2u_size[sock])
            {
                ether_to_uart(sock); // receive remaining packets
            }
			disconnect(sock);
			break;
		
		case SOCK_FIN_WAIT:
		case SOCK_CLOSED:
			set_device_status(sock, ST_OPEN);

			if(mixed_state[sock] == MIXED_SERVER) // MIXED_SERVER
			{
				reset_SEG_timeflags(sock);
				
				u2e_size[sock] = 0;
				e2u_size[sock] = 0;
				
				if(socket(sock, Sn_MR_TCP, network_connection[sock].local_port, Sn_MR_ND) == sock)
				{
					// Replace the command mode switch code GAP time (default: 500ms)
					if((serial_command->serial_command == SEG_ENABLE) && serial_data_packing[0].packing_time) 
                    {
                        modeswitch_gap_time = serial_data_packing[sock].packing_time;
                    }
					
					// TCP Server listen
					listen(sock);
					
					if(serial_common->serial_debug_en == SEG_ENABLE)
					{
						printf(" > SEG:TCP_MIXED_MODE:SERVER_SOCKOPEN\r\n");
					}
				}
			}
			else	// MIXED_CLIENT
			{
				e2u_size[sock] = 0;
				
				source_port = get_tcp_any_port(sock);
#ifdef _SEG_DEBUG_
				printf(" > TCP CLIENT: any_port = %d\r\n", source_port);
#endif		
				if(socket(sock, Sn_MR_TCP, source_port, Sn_MR_ND) == sock)
				{
					// Replace the command mode switch code GAP time (default: 500ms)
					if((serial_command->serial_command == SEG_ENABLE) && serial_data_packing[sock].packing_time) 
                    {
                        modeswitch_gap_time = serial_data_packing[sock].packing_time;
                    }
					
					// Enable the reconnection Timer
					if((enable_reconnection_timer[sock] == SEG_DISABLE) && tcp_option[sock].reconnection) 
                    {
                        enable_reconnection_timer[sock] = SEG_ENABLE;
                    }
					
					if(serial_common->serial_debug_en == SEG_ENABLE)
					{
						printf(" > SEG:TCP_MIXED_MODE:CLIENT_SOCKOPEN\r\n");
					}
				}
			}
			break;
			
		default:
			break;
	}
}

void uart_to_ether(uint8_t sock)
{
	struct __network_connection *network_connection = (struct __network_connection *)(get_DevConfig_pointer()->network_connection);
	struct __serial_common *serial_common = (struct __serial_common *)&get_DevConfig_pointer()->serial_common;
    struct __tcp_option *tcp_option = (struct __tcp_option *)get_DevConfig_pointer()->tcp_option;
    
	uint16_t len;
	int16_t sent_len;
	//uint16_t ret;
	//uint16_t i; // ## for debugging
	
#if (DEVICE_BOARD_NAME == WIZ750SR)
	if(get_phylink_in_pin() != 0) return; // PHY link down
#endif
	
	// UART ring buffer -> user's buffer
	len = get_serial_data(sock);
	add_data_transfer_bytecount(sock, SEG_UART_RX, len);
	
	/*
	// ## for debugging
	if(len)
	{
		printf("flag_connect_pw_auth: %d\r\n", flag_connect_pw_auth);
		printf("uart_to_ether: ");
		for(i = 0; i < len; i++) printf("%c ", g_send_buf[i]);
		printf("\r\n");
	}
	*/
	
	
	if(len > 0)
	{
		/*
		// ## for debugging
		printf("> U2E len: %d, ", len); // ## for debugging
		for(i = 0; i < len; i++) printf("%c", g_send_buf[i]);
		printf("\r\n");
		*/
		
		switch(getSn_SR(sock))
		{
			case SOCK_UDP: // UDP_MODE
				if((network_connection[0].remote_ip[0] == 0x00) 
                    && (network_connection[0].remote_ip[1] == 0x00) 
                    && (network_connection[0].remote_ip[2] == 0x00) 
                    && (network_connection[0].remote_ip[3] == 0x00))
				{
					if((peerip[0] == 0x00) 
                        && (peerip[1] == 0x00) 
                        && (peerip[2] == 0x00) 
                        && (peerip[3] == 0x00))
					{
						if(serial_common->serial_debug_en == SEG_ENABLE) 
                        {
                            printf(" > SEG:UDP_MODE:DATA SEND FAILED - UDP Peer IP/Port required (0.0.0.0)\r\n");
                        }
					}
					else
					{
						// UDP 1:N mode
						sent_len = (int16_t)sendto(sock, g_send_buf[sock], len, peerip, peerport);
					}
				}
				else
				{
					// UDP 1:1 mode
					sent_len = (int16_t)sendto(sock, g_send_buf[sock], len, network_connection[0].remote_ip, network_connection[0].remote_port);
				}
				
				if(sent_len > 0) 
                {
                    u2e_size[sock]-=sent_len;
                }
				
				break;
			
			case SOCK_ESTABLISHED: // TCP_SERVER_MODE, TCP_CLIENT_MODE, TCP_MIXED_MODE
			case SOCK_CLOSE_WAIT:
				// Connection password is only checked in the TCP SERVER MODE / TCP MIXED MODE (MIXED_SERVER)
				if(flag_connect_pw_auth[sock] == SEG_ENABLE)
				{
					
					/* ## 1
					len = send(sock, g_send_buf, len);
					u2e_size = 0;
					*/
					
					// ## 2: TCP send operation- Stability improvements
					/*
					do {
						ret = send(sock, g_send_buf, len);
					} while(ret != len);
					u2e_size = 0;
					*/
					
					// ## 3: 
					sent_len = (int16_t)send(sock, g_send_buf[sock], len);
					if(sent_len > 0) 
                    {
                        u2e_size[sock]-=sent_len;
                    }
					
					add_data_transfer_bytecount(sock, SEG_UART_TX, len);
					//printf("sent len = %d\r\n", len); // ## for debugging
					
					//if(!keepalive_time && netinfo->keepalive_en)
					//if((netinfo->keepalive_en == ENABLE) && (flag_sent_first_keepalive == DISABLE))
					if(tcp_option[sock].keepalive_en == ENABLE)
					{
						if(flag_sent_first_keepalive[sock] == DISABLE)
						{
							enable_keepalive_timer[sock] = SEG_ENABLE;
						}
						else
						{
							flag_sent_first_keepalive[sock] = SEG_DISABLE;
						}
						keepalive_time[sock] = 0;
					}
				}
				break;
			
			case SOCK_LISTEN:
				u2e_size[sock] = 0;
				return;
			
			default:
				break;
		}
	}
	
	inactivity_time[sock] = 0;
	//flag_serial_input_time_elapse = SEG_DISABLE; // this flag is cleared in the 'Data packing delimiter:time' checker routine
}

uint16_t get_serial_data(uint8_t socket)
{
	struct __serial_data_packing *serial_data_packing = (struct __serial_data_packing *)(get_DevConfig_pointer()->serial_data_packing);
    
	uint16_t i;
	uint16_t len;
	
	len = M_BUFFER_USED_SIZE(socket);
	
	if((len + u2e_size[socket]) >= DATA_BUF_SIZE) // Avoiding u2e buffer (g_send_buf) overflow	
	{
		/* Checking Data packing option: charactor delimiter */
		if((serial_data_packing[socket].packing_delimiter[0] != 0x00) && (len == 1))
		{
			g_send_buf[socket][u2e_size[socket]] = (uint8_t)uart_getc(socket);
			if(serial_data_packing[socket].packing_delimiter[0] == g_send_buf[socket][u2e_size[socket]])
			{
				return u2e_size[socket];
			}
		}
		
		//BUFFER_CLEAR(data_rx);
		//return 0; 
		
		// serial data length value update for avoiding u2e buffer overflow
		len = DATA_BUF_SIZE - u2e_size[socket];
	}
	
	if((!serial_data_packing[socket].packing_time) 
        && (!serial_data_packing[socket].packing_size) 
        && (!serial_data_packing[socket].packing_delimiter[0])) // No Packing delimiters.
	{
		//ret = uart_gets(SEG_DATA_UART, g_send_buf, len);
		//return (uint16_t)ret;
		
		// ## 20150427 bugfix: Incorrect serial data storing (UART ring buffer to g_send_buf)
		for(i = 0; i < len; i++)
		{
			g_send_buf[socket][u2e_size[socket]++] = (uint8_t)uart_getc(socket);
		}
		
		return u2e_size[socket];
	}
	else
	{
		/* Checking Data packing options */
		for(i = 0; i < len; i++)
		{
			g_send_buf[socket][u2e_size[socket]++] = (uint8_t)uart_getc(socket);
			
			// Packing delimiter: character option
			if((serial_data_packing[socket].packing_delimiter[0] != 0x00) && (serial_data_packing[socket].packing_delimiter[0] == g_send_buf[socket][u2e_size[socket] - 1]))
			{
				return u2e_size[socket];
			}
			
			// Packing delimiter: size option
			if((serial_data_packing[socket].packing_size != 0) && (serial_data_packing[socket].packing_size == u2e_size[socket]))
			{
				return u2e_size[socket];
			}
		}
	}
	
	// Packing delimiter: time option
	if((serial_data_packing[socket].packing_time != 0) 
        && (u2e_size[socket] != 0) 
        && (flag_serial_input_time_elapse[socket]))
	{
		if(M_BUFFER_USED_SIZE(socket) == 0) 
        {
            flag_serial_input_time_elapse[socket] = SEG_DISABLE; // ##
        }
		
		return u2e_size[socket];
	}
	
	return 0;
}

void ether_to_uart(uint8_t sock)
{
	struct __serial_option *serial_option = (struct __serial_option *)&(get_DevConfig_pointer()->serial_option);
    struct __serial_common *serial_common = (struct __serial_common *)&(get_DevConfig_pointer()->serial_common);
    struct __network_connection *network_connection = (struct __network_connection *)(get_DevConfig_pointer()->network_connection);
    struct __tcp_option *tcp_option = (struct __tcp_option *)(get_DevConfig_pointer()->tcp_option);

	uint16_t len;
	uint16_t i;

	if(serial_option[sock].flow_control == flow_rts_cts)
	{
#ifdef __USE_GPIO_HARDWARE_FLOWCONTROL__
		if(get_uart_cts_pin(sock) != UART_CTS_LOW) 
        {
            return;
        }
#else
		; // check the CTS reg
#endif
	}


	// H/W Socket buffer -> User's buffer
	len = getSn_RX_RSR(sock);
	if(len > DATA_BUF_SIZE) 
    {
        len = DATA_BUF_SIZE; // avoiding buffer overflow
    }
	
	//printf("ether_to_uart: %d\r\n", len); // ## for debugging
	
	if(len > 0)
	{
		switch(getSn_SR(sock))
		{
			case SOCK_UDP: // UDP_MODE
				e2u_size[sock] = recvfrom(sock, g_recv_buf[sock], len, peerip, &peerport);
				
				if(memcmp(peerip_tmp, peerip, 4) !=  0)
				{
					memcpy(peerip_tmp, peerip, 4);
					if(serial_common->serial_debug_en == SEG_ENABLE) 
                    {
                        printf(" > UDP Peer IP/Port: %d.%d.%d.%d : %d\r\n", peerip[0], peerip[1], peerip[2], peerip[3], peerport);
                    }
				}
				break;
			
			case SOCK_ESTABLISHED: // TCP_SERVER_MODE, TCP_CLIENT_MODE, TCP_MIXED_MODE
			case SOCK_CLOSE_WAIT:
				e2u_size[sock] = recv(sock, g_recv_buf[sock], len);
				break;
			
			default:
				break;
		}
		
		inactivity_time[sock] = 0;
		keepalive_time[sock] = 0;
		flag_sent_first_keepalive[sock] = DISABLE;
		
		add_data_transfer_bytecount(sock, SEG_ETHER_RX, e2u_size[sock]);
	}
	
	if((network_connection[sock].working_state == TCP_SERVER_MODE) 
        || ((network_connection[sock].working_state == TCP_MIXED_MODE) && (mixed_state == MIXED_SERVER)))
	{
		// Connection password authentication
		if((tcp_option[sock].pw_connect_en == SEG_ENABLE) && (flag_connect_pw_auth == SEG_DISABLE))
		{
			if(check_connect_pw_auth(g_recv_buf[sock], len) == SEG_ENABLE)
			{
				flag_connect_pw_auth[sock] = SEG_ENABLE;
			}
			else
			{
				flag_connect_pw_auth[sock] = SEG_DISABLE;
			}
			
			e2u_size[sock] = 0;
			
			if(flag_connect_pw_auth[sock] == SEG_DISABLE)
			{
				disconnect(sock);
				return;
			}
		}
	}
	
	// Ethernet data transfer to DATA UART
	if(e2u_size != 0)
	{
		if(serial_option[sock].dsr_en == SEG_ENABLE) // DTR / DSR handshake (flowcontrol)
		{
			if(get_flowcontrol_dsr_pin() == 0) 
            {
                return;
            }
		}
//////////////////////////////////////////////////////////////////////
		if(serial_option[sock].uart_interface == UART_IF_RS422_485)
		{
			uart_rs485_enable(sock);
			//uart_puts(SEG_DATA_UART, g_recv_buf, e2u_size);
			for(i = 0; i < e2u_size[sock]; i++) 
            {
                uart_putc(SEG_DATA_UART, g_recv_buf[sock][i]);
            }
			uart_rs485_disable(SEG_DATA_UART);
			
			add_data_transfer_bytecount(sock, SEG_ETHER_TX, e2u_size[sock]);
			e2u_size[sock] = 0;
		}
//////////////////////////////////////////////////////////////////////
		else if(serial_option[sock].flow_control == flow_xon_xoff) 
		{
			if(isXON == SEG_ENABLE)
			{
				//uart_puts(SEG_DATA_UART, g_recv_buf, e2u_size);
				for(i = 0; i < e2u_size[sock]; i++) 
                {  
                    uart_putc(SEG_DATA_UART, g_recv_buf[sock][i]);
                }
				add_data_transfer_bytecount(sock, SEG_ETHER_TX, e2u_size[sock]);
				e2u_size[sock] = 0;
			}
			//else
			//{
			//	;//XOFF!!
			//}
		}
		else
		{
			//uart_puts(SEG_DATA_UART, g_recv_buf, e2u_size);
			for(i = 0; i < e2u_size[sock]; i++) 
            {   
                uart_putc(SEG_DATA_UART, g_recv_buf[sock][i]);
            }
			
			add_data_transfer_bytecount(sock, SEG_ETHER_TX, e2u_size[sock]);
			e2u_size[sock] = 0;
		}
	}
}


uint16_t get_tcp_any_port(uint8_t socket)
{
	if(client_any_port[socket])
	{
		if(client_any_port[socket] < 0xffff) 	
		{
			client_any_port[socket]++;
		}
		else							
		{
			client_any_port[socket] = 0;
		}
	}
	
	if(client_any_port[socket] == 0)
	{
		srand(get_phylink_downtime());
		client_any_port[socket] = (rand() % 10000) + 35000; // 35000 ~ 44999
	}
	
	return client_any_port[socket];
}


void send_keepalive_packet_manual(uint8_t sock)
{
	setsockopt(sock, SO_KEEPALIVESEND, 0);
	//keepalive_time = 0;
#ifdef _SEG_DEBUG_
	printf(" > SOCKET[%x]: SEND KEEP-ALIVE PACKET\r\n", sock);
#endif 
}


uint8_t process_socket_termination(uint8_t sock)
{
	struct __network_connection *network_connection = (struct __network_connection *)get_DevConfig_pointer()->network_connection;
    
	uint8_t sock_status = getSn_SR(sock);
	
	if(sock_status == SOCK_CLOSED) return sock;
	
	if(network_connection[sock].working_mode != UDP_MODE) // TCP_SERVER_MODE / TCP_CLIENT_MODE / TCP_MIXED_MODE
	{
		if((sock_status == SOCK_ESTABLISHED) || (sock_status == SOCK_CLOSE_WAIT))
		{
			disconnect(sock);
		}
	}
	
	close(sock);
	
	return sock;
}


uint8_t check_connect_pw_auth(uint8_t * buf, uint16_t len)
{
	struct __tcp_option *tcp_option = (struct __tcp_option *)(get_DevConfig_pointer()->tcp_option);
	uint8_t ret = SEG_DISABLE;
	uint8_t pwbuf[11] = {0,};
	
	if(len >= sizeof(pwbuf)) 
    {
        len = sizeof(pwbuf) - 1;
    }
	
	memcpy(pwbuf, buf, len);
	if((len == strlen(tcp_option[0].pw_connect)) && (memcmp(tcp_option[0].pw_connect, pwbuf, len) == 0))
	{
		ret = SEG_ENABLE; // Connection password auth success
	}
	
#ifdef _SEG_DEBUG_
	printf(" > Connection password: %s, len: %d\r\n", option->pw_connect, strlen(option->pw_connect));
	printf(" > Entered password: %s, len: %d\r\n", pwbuf, len);
	printf(" >> Auth %s\r\n", ret ? "success":"failed");
#endif
	
	return ret;
}


void init_trigger_modeswitch(uint8_t mode)
{
	struct __serial_common *serial_common = (struct __serial_common *)&(get_DevConfig_pointer()->serial_common);
	struct __network_connection *network_connection = (struct __network_connection *)(get_DevConfig_pointer()->network_connection);
	
	uint8_t i;
	
	if(mode == DEVICE_AT_MODE)
	{
		opmode = DEVICE_AT_MODE;
		for(i=0; i<2; i++)
		{
			set_device_status(i, ST_ATMODE);
		}
		
		if(serial_common->serial_debug_en)
		{
			printf(" > SEG:AT Mode\r\n");
			uart_puts(SEG_DATA_UART, (uint8_t *)"SEG:AT Mode\r\n", sizeof("SEG:AT Mode\r\n"));
		}
	}
	else // DEVICE_GW_MODE
	{
		opmode = DEVICE_GW_MODE;
		for(i=0; i<2; i++)
		{
			set_device_status(i, ST_OPEN);
			if(network_connection[i].working_mode == TCP_MIXED_MODE) 
			{
					mixed_state[i] = MIXED_SERVER;
			}
		}	
		if(serial_common->serial_debug_en)
		{
			printf(" > SEG:GW Mode\r\n");
			uart_puts(SEG_DATA_UART, (uint8_t *)"SEG:GW Mode\r\n", sizeof("SEG:GW Mode\r\n"));
		}
	}
	
	for(i=0; i<2; i++)
	{
		u2e_size[i] = 0;

		enable_inactivity_timer[i] = SEG_DISABLE;
		enable_keepalive_timer[i] = SEG_DISABLE;
		enable_serial_input_timer[i] = SEG_DISABLE;
		
		
		inactivity_time[i] = 0;
		keepalive_time[i] = 0;
		serial_input_time[i] = 0;
		flag_serial_input_time_elapse[i] = 0;
	}
	enable_modeswitch_timer = SEG_DISABLE;
	BUFFER_CLEAR(data_rx_0);
	modeswitch_time = 0;
}

uint8_t check_modeswitch_trigger(uint8_t ch)
{
	struct __serial_command *serial_command = (struct __serial_command *)&(get_DevConfig_pointer()->serial_command);
	
	uint8_t modeswitch_failed = SEG_DISABLE;
	uint8_t ret = 0;
	
	if(opmode != DEVICE_GW_MODE) 				return 0;
	if(serial_command->serial_command == SEG_DISABLE) 	return 0;
	
	switch(triggercode_idx)
	{
		case 0:
			if((ch == serial_command->serial_trigger[triggercode_idx]) && (modeswitch_time == modeswitch_gap_time)) // comparision succeed
			{
				ch_tmp[triggercode_idx] = ch;
				triggercode_idx++;
				enable_modeswitch_timer = SEG_ENABLE;
			}
			break;
		case 1:
		case 2:
			if((ch == serial_command->serial_trigger[triggercode_idx]) && (modeswitch_time < modeswitch_gap_time)) // comparision succeed
			{
				ch_tmp[triggercode_idx] = ch;
				triggercode_idx++;
			}
			else // comparision failed: invalid trigger code
			{
				modeswitch_failed = SEG_ENABLE; 
			}
			break;
		case 3:
			if(modeswitch_time < modeswitch_gap_time) // comparision failed: end gap
			{
				modeswitch_failed = SEG_ENABLE;
			}
			break;
	}
	
	if(modeswitch_failed == SEG_ENABLE)
	{
		restore_serial_data(triggercode_idx);
	}
	
	modeswitch_time = 0; // reset the inter gap time count for each trigger code recognition (Allowable interval)
	ret = triggercode_idx;
	
	return ret;
}

// when serial command mode trigger code comparision failed
void restore_serial_data(uint8_t idx)
{
	uint8_t i;
	
	for(i = 0; i < idx; i++)
	{
		BUFFER_IN(data_rx_0) = ch_tmp[i];
		BUFFER_IN_MOVE(data_rx_0, 1);
		ch_tmp[i] = 0x00;
	}
	
	enable_modeswitch_timer = SEG_DISABLE;
	triggercode_idx = 0;
}

uint8_t check_serial_store_permitted(uint8_t socket, uint8_t ch)
{
	struct __network_connection *network_connection = (struct __network_connection *)get_DevConfig_pointer()->network_connection;
    struct __serial_option *serial_option = (struct __serial_option *)get_DevConfig_pointer()->serial_option;

	uint8_t ret = SEG_DISABLE; // SEG_DISABLE: Doesn't put the serial data in a ring buffer
	
	switch(network_connection[socket].working_state)
	{
		case ST_OPEN:
			if(network_connection[socket].working_mode != TCP_MIXED_MODE) break;
		case ST_CONNECT:
		case ST_UDP:
		case ST_ATMODE:
			ret = SEG_ENABLE;
			break;
		default:
			break;
	}
	
	// Software flow control: Check the XON/XOFF start/stop commands
	// [Peer] -> [WIZnet Device]
	if((ret == SEG_ENABLE) && (serial_option[socket].flow_control == flow_xon_xoff))
	{
		if(ch == UART_XON)
		{
			isXON = SEG_ENABLE;
			ret = SEG_DISABLE; 
		}
		else if(ch == UART_XOFF)
		{
			isXON = SEG_DISABLE;
			ret = SEG_DISABLE;
		}
	}
	return ret;
}

void reset_SEG_timeflags(uint8_t socket)
{
	// Timer disable
	enable_inactivity_timer[socket] = SEG_DISABLE;
	enable_serial_input_timer[socket] = SEG_DISABLE;
	enable_keepalive_timer[socket] = SEG_DISABLE;
	enable_connection_auth_timer[socket] = SEG_DISABLE;
	
	// Flag clear
	flag_serial_input_time_elapse[socket] = SEG_DISABLE;
	flag_sent_keepalive[socket] = SEG_DISABLE;
	//flag_sent_keepalive_wait = SEG_DISABLE;
	flag_connect_pw_auth[socket] = SEG_DISABLE; // TCP_SERVER_MODE only (+ MIXED_SERVER)
	
	// Timer value clear
	inactivity_time[socket] = 0;
	serial_input_time[socket] = 0;
	keepalive_time[socket] = 0;
	connection_auth_time[socket] = 0;	
}

void init_time_delimiter_timer(uint8_t socket)
{
	struct __serial_command *serial_command = (struct __serial_command *)&(get_DevConfig_pointer()->serial_command);
    struct __serial_data_packing *serial_data_packing = (struct __serial_data_packing *)(get_DevConfig_pointer()->serial_data_packing);

	//DevConfig *s2e = get_DevConfig_pointer();
	
	if((serial_command->serial_command == SEG_ENABLE) && (opmode == DEVICE_GW_MODE))
	{
		if(serial_data_packing[socket].packing_time != 0)
		{
			if(enable_serial_input_timer[socket] == SEG_DISABLE) 
            {
                enable_serial_input_timer[socket] = SEG_ENABLE;
            }
			serial_input_time[socket] = 0;
		}
	}
}

uint8_t check_tcp_connect_exception(uint8_t socket)
{
	struct __network_option *network_option = (struct __network_option *)&get_DevConfig_pointer()->network_option;
    struct __serial_common *serial_common = (struct __serial_common *)&get_DevConfig_pointer()->serial_common;
     struct __network_connection *network_connection = (struct __network_connection *)get_DevConfig_pointer()->network_connection;
	
	uint8_t srcip[4] = {0, };
	uint8_t ret = OFF;
	
	getSIPR(srcip);
	
	// DNS failed
	if((network_option->dns_use == SEG_ENABLE) && (flag_process_dns_success != ON))
	{
		if(serial_common->serial_debug_en == SEG_ENABLE) 
        {
            printf(" > SEG:CONNECTION FAILED - DNS Failed\r\n");
        }
		ret = ON;
	}	
	// if dhcp failed (0.0.0.0), this case do not connect to peer
	else if((srcip[0] == 0x00) 
                && (srcip[1] == 0x00) 
                && (srcip[2] == 0x00) 
                && (srcip[3] == 0x00))
	{
		if(serial_common->serial_debug_en == SEG_ENABLE) 
        {
            printf(" > SEG:CONNECTION FAILED - Invalid IP address: Zero IP\r\n");
        }
		ret = ON;
	}
	// Destination zero IP
	else if((network_connection[socket].remote_ip[0] == 0x00) 
                && (network_connection[socket].remote_ip[1] == 0x00) 
                && (network_connection[socket].remote_ip[2] == 0x00) 
                && (network_connection[socket].remote_ip[3] == 0x00))
	{
		if(serial_common->serial_debug_en == SEG_ENABLE) 
        {
            printf(" > SEG:CONNECTION FAILED - Invalid Destination IP address: Zero IP\r\n");
        }
		ret = ON;
	}
	 // Duplicate IP address
	else if((srcip[0] == network_connection[socket].remote_ip[0]) 
                && (srcip[1] == network_connection[socket].remote_ip[1]) 
                && (srcip[2] == network_connection[socket].remote_ip[2]) 
                && (srcip[3] == network_connection[socket].remote_ip[3]))
	{
		if(serial_common->serial_debug_en == SEG_ENABLE) 
        {
            printf(" > SEG:CONNECTION FAILED - Duplicate IP address\r\n");
        }
		ret = ON;
	}
	else if((srcip[0] == 192) && (srcip[1] == 168)) // local IP address == Class C private IP
	{
		// Static IP address obtained
		if((network_option->dhcp_use == SEG_DISABLE) 
            && ((network_connection[socket].remote_ip[0] == 192) && (network_connection[socket].remote_ip[1] == 168)))
		{
			if(srcip[2] != network_connection[socket].remote_ip[2]) // Class C Private IP network mismatch
			{
				if(serial_common->serial_debug_en == SEG_ENABLE) 
                {
                    printf(" > SEG:CONNECTION FAILED - Invalid IP address range (%d.%d.[%d].%d)\r\n", network_connection[0].remote_ip[0], network_connection[0].remote_ip[1], network_connection[0].remote_ip[2], network_connection[0].remote_ip[3]);
                }
				ret = ON; 
			}
		}
	}
	
	return ret;
}
	

void clear_data_transfer_bytecount(uint8_t socket, teDATADIR dir)
{
	switch(dir)
	{
		case SEG_ALL:
			s2e_uart_rx_bytecount[socket] = 0;
			s2e_uart_tx_bytecount[socket] = 0;
			s2e_ether_rx_bytecount[socket] = 0;
			s2e_ether_tx_bytecount[socket] = 0;
			break;
		
		case SEG_UART_RX:
			s2e_uart_rx_bytecount[socket] = 0;
			break;
		
		case SEG_UART_TX:
			s2e_uart_tx_bytecount[socket] = 0;
			break;
		
		case SEG_ETHER_RX:
			s2e_ether_rx_bytecount[socket] = 0;
			break;
		
		case SEG_ETHER_TX:
			s2e_ether_tx_bytecount[socket] = 0;
			break;
		
		default:
			break;
	}
}


void add_data_transfer_bytecount(uint8_t socket, teDATADIR dir, uint16_t len)
{
	if(len > 0)
	{
		switch(dir)
		{
			case SEG_UART_RX:
				if(s2e_uart_rx_bytecount[socket] < 0xffffffff)	
				{
					s2e_uart_rx_bytecount[socket] += len;
				}
				else 
				{					
					s2e_uart_rx_bytecount[socket] = 0;
				}
				break;
			
			case SEG_UART_TX:
				if(s2e_uart_rx_bytecount[socket] < 0xffffffff)	
				{
					s2e_uart_tx_bytecount[socket] += len;
				}
				else 									
				{
					s2e_uart_tx_bytecount[socket] = 0;
				}
				break;
			
			case SEG_ETHER_RX:
				if(s2e_uart_rx_bytecount[socket] < 0xffffffff)	
				{
					s2e_ether_rx_bytecount[socket] += len;
				}
				else 									
				{
					s2e_ether_rx_bytecount[socket] = 0;
				}
				break;
			
			case SEG_ETHER_TX:
				if(s2e_uart_rx_bytecount[socket] < 0xffffffff)	
				{
					s2e_ether_tx_bytecount[socket] += len;
				}
				else 									
				{
					s2e_ether_tx_bytecount[socket] = 0;
				}
				break;
			
			default:
				break;
		}
	}
}

/*
uint32_t get_data_transfer_bytecount(teDATADIR dir)
{
	uint32_t ret = 0;
	
	switch(dir)
	{
		case SEG_UART_RX:
			ret = s2e_uart_rx_bytecount;
			break;
		
		case SEG_UART_TX:
			ret = s2e_uart_tx_bytecount;
			break;
		
		case SEG_ETHER_RX:
			ret = s2e_ether_rx_bytecount;
			break;
		
		case SEG_ETHER_TX:
			ret = s2e_ether_tx_bytecount;
			break;
		
		default:
			break;
	}
	return ret;
}
*/

// This function have to call every 1 millisecond by Timer IRQ handler routine.
void seg_timer_msec(void)
{
	struct __serial_data_packing *serial_data_packing = (struct __serial_data_packing *)(get_DevConfig_pointer()->serial_data_packing);
	uint8_t i;
	
	// Firmware update timer for timeout
	// DHCP timer for timeout
	
	// SEGCP Keep-alive timer (for configuration tool, TCP mode)
	
	// Reconnection timer: Time count routine (msec)
	for(i=0; i<2; i++)
	{
		if(enable_reconnection_timer[i])
		{
			if(reconnection_time[i] < 0xFFFF) 	
			{
				reconnection_time[i]++;
			}
			else 							
			{
				reconnection_time[i] = 0;
			}
		}
		
		// Keep-alive timer: Time count routine (msec)
		if(enable_keepalive_timer[i])
		{
			if(keepalive_time[i] < 0xFFFF) 	
			{
				keepalive_time[i]++;
			}
			else							
			{
				keepalive_time[i] = 0;
			}
		}
	}
	
	// Mode switch timer: Time count routine (msec) (GW mode <-> Serial command mode, for s/w mode switch trigger code)
	if(modeswitch_time < modeswitch_gap_time) 
    {
        modeswitch_time++;
    }
	
	if((enable_modeswitch_timer) && (modeswitch_time == modeswitch_gap_time))
	{
		// result of command mode trigger code comparision
		if(triggercode_idx == 3) 	
        {
            sw_modeswitch_at_mode_on = SEG_ENABLE; 	// success
        }
		else						
        {
            restore_serial_data(triggercode_idx);	// failed
        }
		
		triggercode_idx = 0;
		enable_modeswitch_timer = SEG_DISABLE;
	}
	
	// Serial data packing time delimiter timer
	for(i=0; i<2; i++)
	{
		if(enable_serial_input_timer[i])
		{
			if(serial_input_time[i] < serial_data_packing[i].packing_time)
			{
				serial_input_time[i]++;
			}
			else
			{
				serial_input_time[i] = 0;
				enable_serial_input_timer[i] = 0;
				flag_serial_input_time_elapse[i] = 1;
			}
		}
		
		// Connection password auth timer
		if(enable_connection_auth_timer[i])
		{
			if(connection_auth_time[i] < 0xffff) 	
			{
					connection_auth_time[i]++;
			}
			else								
			{
					connection_auth_time[i] = 0;
			}
		}
	}
}

// This function have to call every 1 second by Timer IRQ handler routine.
void seg_timer_sec(void)
{
    uint8_t i;

    for(i=0; i<2; i++)
    {
        // Inactivity timer: Time count routine (sec)
        if(enable_inactivity_timer[i])
        {
            if(inactivity_time[i] < 0xFFFF) 
            {
                inactivity_time[i]++;
            }
        }
    }
    tmp_timeflag_for_debug = 1;
}


