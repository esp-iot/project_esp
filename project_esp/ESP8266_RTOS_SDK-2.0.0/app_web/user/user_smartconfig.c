/******************************************************************************
 * Copyright 2016-2017 
 *
 * FileName: user_gpio.c
 *
 * Description: gpio's function realization
 *
 *  design by Martin
 *
 * Modification history:
 * (1) 2016/7/18, v1.0 create this file.
*******************************************************************************/
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "espressif/espconn.h"
#include "espressif/airkiss.h"


#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"





#include "user_config.h"
#include "dmsg.h"




#define server_ip "192.168.101.142"
#define server_port 9669



#define DEVICE_TYPE 		"gh_9e2cff3dfa51" //wechat public number
#define DEVICE_ID 			"122475" //model ID

#define DEFAULT_LAN_PORT 	12476

LOCAL esp_udp ssdp_udp;
LOCAL struct espconn pssdpudpconn;
LOCAL os_timer_t ssdp_time_serv;
struct station_config s_staconf;

uint8  lan_buf[200];
uint16 lan_buf_len;
uint8  udp_sent_cnt = 0;


const airkiss_config_t akconf =
{
	(airkiss_memset_fn)&memset,
	(airkiss_memcpy_fn)&memcpy,
	(airkiss_memcmp_fn)&memcmp,
	0,
};

LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_time_callback(void)
{
	uint16 i;
	airkiss_lan_ret_t ret;
	
	if ((udp_sent_cnt++) >30) {
		udp_sent_cnt = 0;
		os_timer_disarm(&ssdp_time_serv);//s
		//return;
	}

	ssdp_udp.remote_port = DEFAULT_LAN_PORT;
	ssdp_udp.remote_ip[0] = 255;
	ssdp_udp.remote_ip[1] = 255;
	ssdp_udp.remote_ip[2] = 255;
	ssdp_udp.remote_ip[3] = 255;
	lan_buf_len = sizeof(lan_buf);
	ret = airkiss_lan_pack(AIRKISS_LAN_SSDP_NOTIFY_CMD,
		DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);
	if (ret != AIRKISS_LAN_PAKE_READY) {
		os_printf("Pack lan packet error!");
		return;
	}
	
	ret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
	if (ret != 0) {
		os_printf("UDP send error!");
	}
	os_printf("Finish send notify!\n");
}

LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_recv_callbk(void *arg, char *pdata, unsigned short len)
{
	uint16 i;
	remot_info* pcon_info = NULL;
		
	airkiss_lan_ret_t ret = airkiss_lan_recv(pdata, len, &akconf);
	airkiss_lan_ret_t packret;
	
	switch (ret){
	case AIRKISS_LAN_SSDP_REQ:
		espconn_get_connection_info(&pssdpudpconn, &pcon_info, 0);
		os_printf("remote ip: %d.%d.%d.%d \r\n",pcon_info->remote_ip[0],pcon_info->remote_ip[1],
			                                    pcon_info->remote_ip[2],pcon_info->remote_ip[3]);
		os_printf("remote port: %d \r\n",pcon_info->remote_port);
      
        pssdpudpconn.proto.udp->remote_port = pcon_info->remote_port;
		memcpy(pssdpudpconn.proto.udp->remote_ip,pcon_info->remote_ip,4);
		ssdp_udp.remote_port = DEFAULT_LAN_PORT;
		
		lan_buf_len = sizeof(lan_buf);
		packret = airkiss_lan_pack(AIRKISS_LAN_SSDP_RESP_CMD,
			DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);
		
		if (packret != AIRKISS_LAN_PAKE_READY) {
			os_printf("Pack lan packet error!");
			return;
		}

		os_printf("\r\n\r\n");
		for (i=0; i<lan_buf_len; i++)
			os_printf("%c",lan_buf[i]);
		os_printf("\r\n\r\n");
		
		packret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
		if (packret != 0) {
			os_printf("LAN UDP Send err!");
		}
		
		break;
	default:
		os_printf("Pack is not ssdq req!%d\r\n",ret);
		break;
	}
}

void ICACHE_FLASH_ATTR
airkiss_start_discover(void)
{
	ssdp_udp.local_port = DEFAULT_LAN_PORT;
	pssdpudpconn.type = ESPCONN_UDP;
	pssdpudpconn.proto.udp = &(ssdp_udp);
	espconn_regist_recvcb(&pssdpudpconn, airkiss_wifilan_recv_callbk);
	espconn_create(&pssdpudpconn);

	os_timer_disarm(&ssdp_time_serv);
	os_timer_setfn(&ssdp_time_serv, (os_timer_func_t *)airkiss_wifilan_time_callback, NULL);
	os_timer_arm(&ssdp_time_serv, 1000, 1);//1s
}


void smartconfig_done(sc_status status, void *pdata)
{
    switch(status) {
        case SC_STATUS_WAIT:
            printf("SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            printf("SC_STATUS_FIND_CHANNEL\n");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            printf("SC_STATUS_GETTING_SSID_PSWD\n");
            sc_type *type = pdata;
            if (*type == SC_TYPE_ESPTOUCH) {
                printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
            } else {
                printf("SC_TYPE:SC_TYPE_AIRKISS\n");
            }
            break;
        case SC_STATUS_LINK:
            printf("SC_STATUS_LINK\n");
            struct station_config *sta_conf = pdata;
	
	        wifi_station_set_config(sta_conf);
	        wifi_station_disconnect();
	        wifi_station_connect();
            break;
        case SC_STATUS_LINK_OVER:
            printf("SC_STATUS_LINK_OVER\n");
            if (pdata != NULL) {
				//SC_TYPE_ESPTOUCH
                uint8 phone_ip[4] = {0};

                memcpy(phone_ip, (uint8*)pdata, 4);
                printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
			
            } else {
            	//SC_TYPE_AIRKISS - support airkiss v2.0
				airkiss_start_discover();
			}
            smartconfig_stop();
            break;
    }
	
}

void ICACHE_FLASH_ATTR
smartconfig_task(void *pvParameters)
{
    smartconfig_start(smartconfig_done);

	printf("------vTaskDelete smartconfig_task\n");
    vTaskDelete(NULL);
}

void ICACHE_FLASH_ATTR
connect_ap(void *arg, STATUS status)
{
	        wifi_station_set_config(&s_staconf);
	        wifi_station_disconnect();
	        wifi_station_connect();
//wifi_station_set_config_current(&s_staconf);
}

void ICACHE_FLASH_ATTR
user_scan(void)
{
   struct scan_config config;
   os_printf("user_scan\n");
   memset(&config, 0, sizeof(config));

   config.ssid = s_staconf.ssid;

   wifi_station_scan(&config, connect_ap);

}

LOCAL void ICACHE_FLASH_ATTR wifi_task(void *pvParameters)
{
    uint8_t status;

    if (wifi_get_opmode() != STATION_MODE)
    {
        wifi_set_opmode(STATION_MODE);
        vTaskDelay(1000 / portTICK_RATE_MS);
        system_restart();
    }

    while (1)
    {
        dmsg_puts("WiFi: Connecting to WiFi\n");
        wifi_station_connect();
        user_scan();
        status = wifi_station_get_connect_status();
        int8_t retries = 30;
        while ((status != STATION_GOT_IP) && (retries > 0))
        {
            status = wifi_station_get_connect_status();
            if (status == STATION_WRONG_PASSWORD)
            {
                dmsg_puts("WiFi: Wrong password\n");
                break;
            }
            else if (status == STATION_NO_AP_FOUND)
            {
                dmsg_puts("WiFi: AP not found\n");
                break;
            }
            else if (status == STATION_CONNECT_FAIL)
            {
                dmsg_puts("WiFi: Connection failed\n");
                break;
            }
            vTaskDelay(1000 / portTICK_RATE_MS);
            --retries;
        }
#if 1
        if (status == STATION_GOT_IP)
        {
            dmsg_puts("WiFi: Connected\n");
			 //xSemaphoreGive(wifi_alive);
			 
            vTaskDelay(1000 / portTICK_RATE_MS);
			vTaskDelete(NULL);
			return;
        }
#if 0
        while ((status = wifi_station_get_connect_status()) == STATION_GOT_IP)
        {
            xSemaphoreGive(wifi_alive);
            // dmsg_puts("WiFi: Alive\n");
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
#endif
        dmsg_puts("WiFi: Disconnected\n");
        wifi_station_disconnect();
        vTaskDelay(1000 / portTICK_RATE_MS);
#endif
    }
}



void ICACHE_FLASH_ATTR
init_done_cb_init(void)
{
	struct station_config stconfig;
	struct softap_config  apconfig;
	
	//print_chip_info();
	
	os_memset(&stconfig, 0, sizeof(struct station_config));
	wifi_station_get_config_default(&stconfig);
	if(os_strlen(stconfig.ssid) != 0){
		os_printf("ssid[%s]pass[%s]", stconfig.ssid, stconfig.password);
		wifi_set_opmode(STATION_MODE);
		wifi_station_set_reconnect_policy(TRUE);
		wifi_station_set_auto_connect(TRUE);
	}
	else{
		wifi_set_opmode(SOFTAP_MODE);
		wifi_softap_get_config(&apconfig);
		os_memset(apconfig.ssid, 0, 32);
		os_memset(apconfig.password, 0, 64);
		os_sprintf(apconfig.ssid,"ESP_%X",system_get_chip_id());
		apconfig.authmode = AUTH_OPEN;
		apconfig.ssid_len = 0;
		apconfig.max_connection = 5; 
		wifi_softap_set_config(&apconfig);
		wifi_station_set_reconnect_policy(FALSE);
		wifi_station_set_auto_connect(FALSE);
		wifi_station_disconnect();
	}

	user_webserver_init(SERVER_PORT);
}


/**
	* @brief  no .	  
	* @note   no.
	* @param  no.
	* @retval no.
	*/
	
void SmartConfigTaskCreate( void )
{
	uint16 i ;
	/* need to set opmode before you set config */
    wifi_set_opmode(STATION_MODE);
	
	
	wifi_station_get_config_default(&s_staconf);
	for(i=0;i<10;i++)
		os_printf("s_staconf.ssid= %d\r\n",s_staconf.ssid[i]);

	if (strlen(s_staconf.ssid) != 0) {
		xTaskCreate(wifi_task, "wifi_task", 256, NULL, 1, NULL);
    } else {
     	os_printf("smartcfg\n");

   		xTaskCreate(smartconfig_task, "smartconfig_task", 256, NULL, 1, NULL);
   }

}

