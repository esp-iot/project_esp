/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#include "connect_net.h"


#define STATE_ROUTER_KEEPALIVE_FAILURE -4
#define STATE_ROUTER_FAILURE -3
#define STATE_PASSPORT_FAILURE -2
#define STATE_LOCATE_FAILURE -1
#define STATE_LOCATE_INIT 0
#define STATE_LOCATE_CONNECT 1
#define STATE_LOCATE 2
#define STATE_PASSPORT_INIT 3
#define STATE_PASSPORT_CONNECT 4
#define STATE_PASSPORT 5
#define STATE_PASSPORT_WAIT 6
#define STATE_ROUTER_INIT 7
//#define STATE_ROUTER_CONNECT 8
#define STATE_ROUTER_REGISTER 9
#define STATE_ROUTER_KEEPALIVE 10
#define STATE_ROUTER_KEEPALIVE_ACK 11





static struct lt_socket TCP = {-1,LTR_SOCKET,1,0,53199};
static int TCP_STATE = 0;


static int WKBUF_LEN = 0;
static int64_t TCP_MAINTAIN_TIMESTAMP = 0;
static int64_t TCP_STATE_TIMESTAMP = 0;


void ltp_tcp_sock_state_set(int state){
    TCP_STATE = state;
    TCP_STATE_TIMESTAMP = lt_datetime();
}


const struct lt_socket* lt_ltx_socket(void){
    return &TCP;
}


int lt_ltx_socket_buffer_size(void){
    return WKBUF_LEN;
}

void ltp_tcp_sock_maintain(int64_t now)
{
	if(now-TCP_MAINTAIN_TIMESTAMP<2000){
        return;
    }

	TCP_MAINTAIN_TIMESTAMP = now;
    switch (TCP_STATE)
    {
        case STATE_LOCATE_INIT:
			printf("----STATE_LOCATE_INIT\r\n");
            TCP.addr = htonl(INADDR_ANY); //lt_get_register_host();
            TCP.socket = lt_socket_connect(&TCP);
            if (TCP.socket != -1) {
                ltp_tcp_sock_state_set(STATE_LOCATE_CONNECT);
            }
			
            break;
        case STATE_LOCATE_CONNECT:
			
            break;
        case STATE_LOCATE:
            break;
        case STATE_PASSPORT_INIT:
            
            break;
        case STATE_PASSPORT_CONNECT:
            
            break;
        case STATE_PASSPORT:
            break;
        case STATE_PASSPORT_WAIT:
            break;
        case STATE_ROUTER_INIT:
            
            break;
        case STATE_ROUTER_REGISTER:
			
            break;
        case STATE_ROUTER_KEEPALIVE:
		
            break;
        case STATE_ROUTER_KEEPALIVE_ACK:
			
            break;
    }

}


