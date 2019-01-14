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
            //mqtt_process_init();

			tcp_server_thread_init( );
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



/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_init(void)
{
		user_uart_init( );
		printf("SDK version:%s\n", system_get_sdk_version());
		led_init( );

#if 0
		/* need to set opmode before you set config */
		char ap_ssid[] = SSID;
    	char ap_passwd[] = PASSWORD;
   		struct station_config *config = (struct station_config*)zalloc(
        sizeof(struct station_config));

   		 wifi_set_opmode(STATION_MODE);
   		 printf("ESP8266 mode is wifi station!\r\n");
   		 sprintf(config->ssid, ap_ssid);
    	sprintf(config->password, ap_passwd);

    	wifi_station_set_config(config);
    	free(config);
#endif	

		 // xTaskCreate(smartconfig_task, "smartconfig_task", 256, NULL, 2, NULL);
	//	xTaskCreate(led_toggle_task, "led_toggle_task", 256, NULL, 1, NULL);
		//xTaskCreate(key_handler_task, "key_handler_task", 256, NULL, 3, &key_handler_task_handle );

		//user_txrx_init();

		
		UdpServerInit();
		
		user_txrx_init();

		SmartConfigTaskCreate();	//task priority 1,配置网络，第一次要手机配网，之后会记下ssid，自动配置
		//user_modbus_init();
		//txrx_thread_init();

		wifi_set_event_handler_cb(wifi_event_handler_cb);
    	wifi_station_connect();
		
}

