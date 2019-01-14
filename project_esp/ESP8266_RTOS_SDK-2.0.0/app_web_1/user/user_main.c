/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "espressif/espconn.h"
#include "espressif/airkiss.h"

#include "driver/uart.h"
#include "driver/gpio.h"




#include "freertos/queue.h"
#include "user_config.h"
#include "dmsg.h"
#include "user_smartconfig.h"

#include "user_webserver.h"


//#include "osapi.h"
//#include "at_custom.h"
//#include "user_interface.h"
#include "espconn.h"
#include "espressif/esp8266/ets_sys.h"
#include "lwip/mem.h"



xTaskHandle key_handler_task_handle;


#define server_ip "192.168.101.142"
#define server_port 9669


#define DEVICE_TYPE 		"gh_9e2cff3dfa51" //wechat public number
#define DEVICE_ID 			"122475" //model ID

#define DEFAULT_LAN_PORT 	12476

LOCAL esp_udp ssdp_udp;
LOCAL struct espconn pssdpudpconn;
LOCAL os_timer_t ssdp_time_serv;

uint8  lan_buf[200];
uint16 lan_buf_len;




/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}


/********************************led*******************************************/


void led_toggle_task( void *pvParameters )
{
	led_init( );
	for( ;; )
	{
		led_toggle( );
		//printf("led toggle!\r\n");
		vTaskDelay( 500 / portTICK_RATE_MS );
	}
	
	vTaskDelete(NULL);
}


/********************************key*******************************************/

void key_int_handler( void )
{
	uint32 gpio_status;

	gpio_status = GPIO_REG_READ( GPIO_STATUS_ADDRESS );

	GPIO_REG_WRITE( GPIO_STATUS_W1TC_ADDRESS , gpio_status );	//clear interrupt status

	if( gpio_status & (BIT(0)) )
	{
		xTaskResumeFromISR( key_handler_task_handle );	// 用于恢复一个挂起的任务，用在ISR中。参数是需要恢复的任务的句柄
	}
}

void key_init( void )
{
	GPIO_ConfigTypeDef	GPIOConfig;
	
	GPIOConfig.GPIO_Pin = GPIO_Pin_0;
	GPIOConfig.GPIO_Mode = GPIO_Mode_Input;
	GPIOConfig.GPIO_Pullup = GPIO_PullUp_DIS;
	GPIOConfig.GPIO_IntrType = GPIO_PIN_INTR_NEGEDGE;
	gpio_config( &GPIOConfig );
	
	gpio_intr_handler_register( key_int_handler , NULL );//注册中断函数

	_xt_isr_unmask(1 << 4);
}


void key_handler_task( void *pvParameters )
{
    uint32_t TickCountPre = 0 , TickCountCur = 0;

	key_init(); 
	
	for( ;; )
	{
		vTaskSuspend( NULL );	//函数被挂起，当只有当有中断触发时才继续执行

		TickCountPre = TickCountCur;
			
		TickCountCur = xTaskGetTickCount( );//用于获取系统当前运行的时钟节拍数。
		if( TickCountCur - TickCountPre > 7 )//消抖
		{
			uint8_t i;

			for( i = 0; i < 10 ; i ++ )
			{
				vTaskDelay( 500 / portTICK_RATE_MS );

				uint32_t gpio_value;

				gpio_value = gpio_input_get( );
				if( ( gpio_value & BIT(0) ) == BIT(0) )
				{
					break;
				}
			}

			if( i == 10 )
			{
				smartconfig_start( smartconfig_done );
			}
		}
			
	}
}



void wifi_event_handler_cb(System_Event_t *event)
{
    if (event == NULL) return;

    switch (event->event_id) {
        case EVENT_STAMODE_GOT_IP:
            os_printf("sta got ip ,create task and free heap size is %d\n", system_get_free_heap_size());
			tcp_server_thread_init( );
			mqtt_process_init();
			
            break;

        case EVENT_STAMODE_CONNECTED:
            os_printf("sta connected\n");
            break;

        case EVENT_STAMODE_DISCONNECTED:
            wifi_station_connect();
            break;

        default:
            break;
    }
}



void ICACHE_FLASH_ATTR
user_set_softap_config(void)
{
	struct station_config stconfig;
	struct softap_config  apconfig;

	
	memset(&stconfig, 0, sizeof(struct station_config));
	wifi_station_get_config_default(&stconfig);
	if(strlen(stconfig.ssid) != 0){
		printf("ssid[%s]pass[%s]", stconfig.ssid, stconfig.password);
		wifi_set_opmode(STATION_MODE);
		wifi_station_set_reconnect_policy(TRUE);
		wifi_station_set_auto_connect(TRUE);
	}
	else{
		wifi_set_opmode(SOFTAP_MODE);
		wifi_softap_get_config(&apconfig);
		memset(apconfig.ssid, 0, 32);
		memset(apconfig.password, 0, 64);
		sprintf(apconfig.ssid,"ESP_%X",system_get_chip_id());
		apconfig.authmode = AUTH_OPEN;
		apconfig.ssid_len = 0;
		apconfig.max_connection = 5; 
		wifi_softap_set_config(&apconfig);
		wifi_station_set_reconnect_policy(FALSE);
		wifi_station_set_auto_connect(FALSE);
		wifi_station_disconnect();
	}


}



void ICACHE_FLASH_ATTR
user_init(void)
{
	user_uart_init( );
    os_printf("SDK version:%s\r\n", system_get_sdk_version());
    os_printf("Compile time:%s %s\r\n", __DATE__, __TIME__);
    
    wifi_set_opmode(STATIONAP_MODE);
    // ESP8266 softAP set config.
    user_set_softap_config();

    user_webserver_init(SERVER_PORT);
}



