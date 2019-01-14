/*******************************************************************************
* Copyright (c) 2014 IBM Corp.
*
* All rights reserved. This program and the accompanying materials
* are made available under the terms of the Eclipse Public License v1.0
* and Eclipse Distribution License v1.0 which accompany this distribution.
*
* The Eclipse Public License is available at
*    http://www.eclipse.org/legal/epl-v10.html
* and the Eclipse Distribution License is available at
*   http://www.eclipse.org/org/documents/edl-v10.php.
*
* Contributors:
*    Ian Craggs - initial API and implementation and/or initial documentation
*******************************************************************************/

#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt/MQTTClient.h"
#include "json/cjson.h"
#include "gpio.h"
#include "log_print.h"
#include "mqtt_process.h"

#include "common.h"


#include "esp_common.h"

#include "espressif/esp8266/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "lwip/sys.h"
#include "lwip/sockets.h"

#include "Led.h"
#include "log_print.h"


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>


#define MQTT_CLIENT_THREAD_NAME		"mqtt_client_thread"
#define MQTT_CLIENT_THREAD_STACK_WORDS	3000
#define MQTT_CLIENT_THREAD_PRIO		8

LOCAL xTaskHandle	mqttc_client_handle;
MQTTClient		client;
u8			process_state = MATT_NETWORK_CONNECT;
unsigned char		sendbuf[512], readbuf[512] = { 0 };

xTaskHandle uart_mutex;


typedef struct
{
	int led;
}state;


void get_led_state( char *json_info, int len )
{
	cJSON	* root		= NULL;
	cJSON	* item		= NULL; /* cjson���� */
	cJSON	* object	= NULL;

	char	*pstart = json_info;
	int	i;
	state	led_state[2];

	int esp_device_id = -1, my_id = -1;

	bit_byte4 Data0;
	Data0.bytes4 = 0X00; /*按字节修改 */


	DEBUG( " Json len = %d\n", len );

	for ( i = 0; i < len; i++ )
	{
		if ( *pstart == '{' )
			break;
		else
			pstart++;
	}

	DEBUG( " Json pstart = %c\n", *pstart );

	if ( i == len )
		return;

	root = cJSON_Parse( pstart );
	DEBUG( "%s\n\n", cJSON_Print( root ) );         /*有格式的方式打印Json */
	if ( root == NULL )
	{
		DEBUG( "Error before: [%s]\n", cJSON_GetErrorPtr() );
	}
	else     
	{
		int size = cJSON_GetArraySize( root );  /* 获取cjson对象数组成员的个数 */
		DEBUG( "cJSON_GetArraySize: size=%d\n", size );

		for ( i = 0; i < size; i++ )
		{
			DEBUG( "i=%d\n", i );
			object = cJSON_GetArrayItem( root, i );

			item = cJSON_GetObjectItem( object, "device_id" );
			if ( item != NULL )
			{
				DEBUG( "cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", item->type, item->string, item->valuestring, item->valueint );
				esp_device_id	= item->valueint;
				my_id		= system_get_chip_id(); /* 设备id */

				if ( my_id != esp_device_id )
				{
					DEBUG( "Id matching failed\n" );
					cJSON_Delete( root );
					return ; 
				}else{
					
					DEBUG( "Id matching successful\n" );
				}
			}

			else if(i == size)
			{
				DEBUG( "NO device_id\n" );
				cJSON_Delete( root );
				return ; 

			}

		}

		for ( i = 0; i < size; i++ )
		{
			DEBUG( "i=%d\n", i );
			object = cJSON_GetArrayItem( root, i );

			item = cJSON_GetObjectItem( object, "LED" );
			if ( item != NULL )
			{
				DEBUG( "cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", item->type, item->string, item->valuestring, item->valueint );
				/* memcpy(led_state[i].led,item->valuestring,strlen(item->valuestring)); */

				/*
				 * 接收到LED数据时，开始上锁，不上锁的话可能串口传输会超时，导致数据丢失
				 * sys_mutex_lock(&uart_mutex);
				 */

				Data0.bytes4 = dec2hex( item->valueint );       /* 接收到的数据转换成十六进制 */

#if 0
				STATUS Send_Status;
				do
				{
					Send_Status = ReadInverter( 0x01, 0x06, 0x02, Data0.bytes4 );
					vTaskDelay( 50 / portTICK_RATE_MS );    /* 延时50毫秒 */
				}
				while ( Send_Status != OK );

#endif


				i = 0;
				for ( i; i < 10; i++ ) /* 因为串口发送数据是一个字节一个字节的发送，可能时序不对导致发送失败 */
				{
					ReadInverter( 0x01, 0x06, 0x02, Data0.bytes4 );
					vTaskDelay( 50 / portTICK_RATE_MS );
				}


#if 0
				led_state[i].led = item->valueint;
				GPIO_OUTPUT_SET( 13, led_state[i].led );
				/* led_control(13,led_state[i].led); */
#endif
			}
		}
	}

	cJSON_Delete( root );
#if 0
	if ( !root )

		else{
			DEBUG( "111111111\n" );
			item = cJSON_GetObjectItem( root, "ledState" );
			/* DEBUG("%s %d\r\n", item->string, item->valueint); */
			GPIO_OUTPUT_SET( 2, item->valueint );
		}


#endif
}


static void message_arrived( MessageData* data )
{
	/* 接收到LED数据时，开始上锁，不上锁的话可能串口传输会超时，导致数据丢失 */
	sys_mutex_lock( &uart_mutex );
	DEBUG( "Message arrived: %s\n", data->message->payload );

	get_led_state( data->message->payload, data->message->payloadlen );

	/* 发送成功，释放锁 */
	sys_mutex_unlock( &uart_mutex );

	memset( readbuf, 0, sizeof(readbuf) );
}


static void mqtt_process_thread( void* pvParameters )
{
	DEBUG( "mqtt client thread starts\n" );

	Network			network;
	int			rc		= 0;
	int			count		= 0;
	MQTTPacket_connectData	connectData	= MQTTPacket_connectData_initializer;
	char			* address	= MQTT_BROKER;
	int			port		= MQTT_PORT;
	u8			process_state	= MATT_NETWORK_CONNECT;
	MQTTMessage		message;
	char			payload[30];
	char			*sub_topic		= "$baidu/iot/shadow/my_mqtt/update/accepted";
	char			mqtt_client_id[20]	= "";
	int			my_id			= 0;

	int		con_stat = 0; /* 判断mqtt连接状态 */
	STATION_STATUS	StaStatus;

	if ( sys_mutex_new( &uart_mutex ) != ERR_OK )
	{
		DEBUG( "failed to create uart_mutex\n" );
	}

	while ( 1 )
	{
		switch ( process_state )
		{
		case MATT_NETWORK_CONNECT:
			pvParameters = 0;
			NetworkInit( &network );

			MQTTClientInit( &client, &network, 30000, sendbuf,
					sizeof(sendbuf), readbuf, sizeof(readbuf) );

			if ( (rc = NetworkConnect( &network, address, port ) ) != 0 )
			{
				DEBUG( "Return code from network connect is %d\n", rc );
				process_state = MATT_NETWORK_CONNECT;
			}else  {
				process_state = MATT_CONNECT;
			}
			break;
		case MATT_CONNECT:
			/* connectData.willFlag = 0xc2; */

			/* Unique client ID */
			my_id = system_get_chip_id(); /* 设备id */
			DEBUG( "my_id=%d\n", my_id );

			sprintf( mqtt_client_id, "ESP_%d", my_id );

			DEBUG( "mqtt_client_id=%s\n", mqtt_client_id );


			/* connectData.willFlag = 0xc2; */
			connectData.clientID.cstring = mqtt_client_id;
			/*
			 * connectData.username.cstring = "7iidyq0/my_mqtt";MQTT_USRENAME
			 * connectData.password.cstring = "XAFhs6qLXLubmUeo";
			 */

			connectData.username.cstring	= MQTT_USRENAME;
			connectData.password.cstring	= MQTT_PASSWORD;

			if ( (rc = MQTTConnect( &client, &connectData ) ) != 0 )
			{
				DEBUG( "Return code from MQTT connect is %d , MQTTConnect error\n", rc );
				network.disconnect( &network );
				process_state = MATT_NETWORK_CONNECT;
			}else  {
				DEBUG( "MQTT Connected\n" );
				process_state = MATT_FREE;
				/* process_state = MATT_PUBLISH */
#if defined(MQTT_TASK)
				if ( (rc = MQTTStartTask( &client ) ) != pdPASS )
				{
					DEBUG( "Return code from start tasks is %d\n", rc );
				}else  {
					DEBUG( "Use MQTTStartTask\n" );
				}
#endif


				if ( (rc = MQTTSubscribe( &client, sub_topic, 0, message_arrived ) ) != 0 )
				{
					DEBUG( "Return code from MQTT subscribe is %d\n", rc );
				}else  {
					DEBUG( "MQTT subscribe to topic \"%s\"\n", sub_topic );
				}
			}
			break;
		case MATT_PUBLISH:
			message.qos		= QOS2;
			message.retained	= 0;
			message.payload		= payload;
			sprintf( payload, "{\"reported\": {\"Temperature\": %d}}", count );
			message.payloadlen = strlen( payload );

			if ( (rc = MQTTPublish( &client, "$baidu/iot/shadow/my_mqtt/update/", &message ) ) != 0 )
			{
				DEBUG( "Return code from MQTT publish is %d\n", rc );
				network.disconnect( &network );
				process_state = MATT_NETWORK_CONNECT;
				MQTTDisconnect( &client );
			}else  {
				DEBUG( "MQTT publish topic \"ESP8266/sample/pub\", "
				       "message number is %d\n", count++ );

				process_state = MATT_PUBLISH;
			}
			break;
		default:
			con_stat = MQTTIsConnected( &client );
			/* DEBUG("----mqtt con_stat = %d\n",con_stat); */
#if 1
			if ( con_stat == 0 )
			{
				DEBUG( "mqtt connected error!\r\n" );

				do
				{
					/* 判断是否已经获取了路由器分配的IP,如果没有则需要重新去连接 */
					StaStatus = wifi_station_get_connect_status();

					vTaskDelay( 1000 / portTICK_RATE_MS );
				}
				while ( StaStatus != STATION_GOT_IP );
			}
#endif
			break;
		}

		vTaskDelay( 1000 / portTICK_RATE_MS ); /* //心跳频率是200hz，那么时间片是5ms。那么，vTaskDelay(1000/portTICK_RATE_MS );就是延迟200个时间片，也就是延迟1s。 */
	}

	DEBUG( "mqtt_client_thread going to be deleted\n" );
	vTaskDelete( NULL );
	return;
}


void mqtt_process_init( void )
{
	int ret;
	ret = xTaskCreate( mqtt_process_thread,
			   MQTT_CLIENT_THREAD_NAME,
			   MQTT_CLIENT_THREAD_STACK_WORDS,
			   NULL,
			   MQTT_CLIENT_THREAD_PRIO,
			   &mqttc_client_handle );

	if ( ret != pdPASS )
	{
		DEBUG( "mqtt create client thread %s failed ret=%d\n", MQTT_CLIENT_THREAD_NAME, ret );
	}
}
