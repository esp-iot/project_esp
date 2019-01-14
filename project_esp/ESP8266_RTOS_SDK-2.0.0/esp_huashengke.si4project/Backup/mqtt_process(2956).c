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
char payload[30];


xTaskHandle uart_mutex;


typedef struct
{
	int led;
}state;




int parse_Json(char * pJson)
{
	cJSON * pSub_1 = NULL , * pRoot = NULL , * pSub_2 = NULL;


    if(NULL == pJson)
    {
        return 1;
    }

    pRoot = cJSON_Parse(pJson);
    if(NULL == pRoot)
    {
        return 2;
    }

	if(cJSON_GetObjectItem(pRoot, "device_id") == NULL && cJSON_GetObjectItem(pRoot, "get") == NULL )
		return -1;

	pSub_1 = cJSON_GetObjectItem(pRoot, "device_id");
    if(NULL != pSub_1)
    {
        DEBUG( "cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", pSub_1->type, pSub_1->string, pSub_1->valuestring, pSub_1->valueint );
		pSub_1 = cJSON_GetObjectItem(pRoot, "get");
	    if(NULL != pSub_1)
	    {
	    	DEBUG( "cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", pSub_1->type, pSub_1->string, pSub_1->valuestring, pSub_1->valueint );
			DEBUG("pSub_1->valuestring = %s\n",pSub_1->valuestring);
			if(strcmp(pSub_1->valuestring , "led_stat") == 0){
				DEBUG("----led_stat\n");
			}

		}

		pSub_1 = cJSON_GetObjectItem(pRoot, "set");
	    if(NULL != pSub_1)
	    {
	    	DEBUG( "cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", pSub_1->type, pSub_1->string, pSub_1->valuestring, pSub_1->valueint );
			pSub_2 = cJSON_GetObjectItem(pSub_1, "led_stat");
		    if(NULL != pSub_2)
		    {
				DEBUG( "cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", pSub_2->type, pSub_2->string, pSub_2->valuestring, pSub_2->valueint );
		    }

		}
		 
    }
	else{
		pSub_1 = cJSON_GetObjectItem(pRoot, "get");
	    if(NULL != pSub_1)
	    {
	    	DEBUG( "cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", pSub_1->type, pSub_1->string, pSub_1->valuestring, pSub_1->valueint );
			
			if(strcmp(pSub_1->valuestring , "device_id") == 0){
				DEBUG( "-----device_id\n" );
				sprintf(payload , "1234567");
				process_state = MATT_PUBLISH;
			}

		}
	}
    
	
   
    

	pSub_1 = cJSON_GetObjectItem(pRoot, "set");
    if(NULL != pSub_1)
    {
         DEBUG("get Sub Obj 1\n");
    }  

    cJSON_Delete(pRoot);
    return 0;
}



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

			item = cJSON_GetObjectItem( object, "device_id" );//用来判断是否为对应的id，不是的话直接跳过
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

		//LED 继电器
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

		
		//温度
		for ( i = 0; i < size; i++ )
		{
			DEBUG( "i=%d\n", i );
			object = cJSON_GetArrayItem( root, i );

			item = cJSON_GetObjectItem( object, "Temperature" );
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

#if 0
{
	char *p = NULL;
	int i = 0 , ret = 0;
	DEBUG("data : %s , len : %d \n", data , len);

}
#endif

void parse_data(char *json_info, int len)
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

	DEBUG("json_info : %s , len : %d \n", json_info , len);


	DEBUG( " Json len = %d\n", len );
#if 1
	for ( i = 0; i < len; i++ )
	{
		if ( *pstart == '{' )
			break;
		else
			pstart++;
	}
#endif
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

#if 1
		int size = cJSON_GetArraySize( root );  /* 获取cjson对象数组成员的个数 */
		DEBUG( "cJSON_GetArraySize: size=%d\n", size );

		for ( i = 0; i < size; i++ )
		{
			DEBUG( "i=%d\n", i );
			object = cJSON_GetArrayItem( root, i );

			

			item = cJSON_GetObjectItem( object, "get" );//用来判断是否为对应的id，不是的话直接跳过
			if ( item != NULL )
			{
				DEBUG( "cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", item->type, item->string, item->valuestring, item->valueint );
				
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

			

			item = cJSON_GetObjectItem( object, "device_id" );//用来判断是否为对应的id，不是的话直接跳过
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

		//LED 继电器
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

		
		//温度
		for ( i = 0; i < size; i++ )
		{
			DEBUG( "i=%d\n", i );
			object = cJSON_GetArrayItem( root, i );

			item = cJSON_GetObjectItem( object, "Temperature" );
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

#endif
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


static void message_arrived_huashengke(MessageData* data)
{
    DEBUG("Message arrived: %s\n", data->message->payload);

	parse_Json(data->message->payload);
	//parse_data(data->message->payload , data->message->payloadlen);
    //get_led_state(data->message->payload, data->message->payloadlen);

    memset(readbuf, 0, sizeof(readbuf));
}

void mqtt_handler(char* address , int port ,  char *sub_topic , char *push_topic ,MQTTPacket_connectData connectData , void * pvParameters , int cloud_ser)
{
	Network network;
    int rc = 0;
    int count = 0;
    

    MQTTMessage message;
    

    while (1)
    {
        switch(process_state)
        {
            case MATT_NETWORK_CONNECT:
                pvParameters = 0;
                NetworkInit(&network);

                MQTTClientInit(&client, &network, 30000, sendbuf,
                        sizeof(sendbuf), readbuf, sizeof(readbuf));

                if ((rc = NetworkConnect(&network, address, port)) != 0)
                {
                    DEBUG("Return code from network connect is %d\n", rc);
                    process_state = MATT_NETWORK_CONNECT;
                }
                else
                {
                    process_state = MATT_CONNECT;
                }
                break;
				
            case MATT_CONNECT:
                if ((rc = MQTTConnect(&client, &connectData)) != 0)
                {
                    DEBUG("Return code from MQTT connect is %d\n", rc);
                    process_state = MATT_NETWORK_CONNECT;
                }
                else
                {
                    DEBUG("MQTT Connected\n");
                    process_state = MATT_FREE;
                    //process_state = MATT_PUBLISH
                    #if defined(MQTT_TASK)
                    if ((rc = MQTTStartTask(&client)) != pdPASS)
                    {
                        DEBUG("Return code from start tasks is %d\n", rc);
                    }
                    else
                    {
                        DEBUG("Use MQTTStartTask\n");
                    }
                    #endif

					if(cloud_ser == HUASHENGKE_SER)
                    	rc = MQTTSubscribe(&client, sub_topic, 0, message_arrived_huashengke) ;
					else if(cloud_ser == BAIDU_SER)
						rc = MQTTSubscribe(&client, sub_topic, 0, message_arrived) ;
					
					if(rc != 0)
                    {
                        DEBUG("Return code from MQTT subscribe is %d\n", rc);
						process_state = MATT_NETWORK_CONNECT;
                    }
                    else
                    {
                        DEBUG("MQTT subscribe to topic \"%s\"\n", sub_topic);
                    }
                }
                break;
            case MATT_PUBLISH:
                message.qos = QOS2;
                message.retained = 0;
                message.payload = payload;
                //sprintf(payload, "{\"reported\": {\"Temperature\": %d}}", count);
                //message.payloadlen = strlen(payload);

				printf("MATT_PUBLISH \n");
                if ((rc = MQTTPublish(&client, push_topic , &message)) != 0)
                {
                    DEBUG("Return code from MQTT publish is %d\n", rc);
                    process_state = MATT_NETWORK_CONNECT;
                    MQTTDisconnect(&client);
                }
                else
                {
                    DEBUG("MQTT publish topic \"ESP8266/sample/pub\", "
                            "message number is %d\n", count++);
                    process_state = MATT_FREE;
                }
                break;
            default:
                break;
        }

        vTaskDelay(10 / portTICK_RATE_MS);  //send every 1 seconds
    }

    DEBUG("mqtt_client_thread going to be deleted\n");
    vTaskDelete(NULL);
    return;
}


char * get_client_id()
{
	int my_id = -1;
	static char mqtt_client_id[20] = "";
	/* Unique client ID */
	my_id = system_get_chip_id(); /* 设备id */
	DEBUG( "my_id=%d\n", my_id );
	
	sprintf( mqtt_client_id, "ESP_%d", my_id );
	
	DEBUG( "mqtt_client_id=%s\n", mqtt_client_id );

	return mqtt_client_id ; 


}

static void mqtt_process_thread_huashengke(void* pvParameters)
{
    DEBUG("mqtt client thread starts\n");
	MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

	
	
	
	/* connectData.willFlag = 0xc2; */
	connectData.clientID.cstring = get_client_id();
    //connectData.username.cstring = "7iidyq0/my_mqtt";
    //connectData.password.cstring = "KsC6Pw0Loay42CIM";

	pvParameters = 0 ; 


	mqtt_handler(MQTT_BROKER_HuaShengKe , MQTT_PORT_HuaShengKe  , SUB_TOPIC_HuaShengKe , PUSH_TOPIC_HuaShengKe , connectData , pvParameters , HUASHENGKE_SER);
}


static void mqtt_process_thread(void* pvParameters)
{
	DEBUG("mqtt client thread starts\n");
	MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
	
	//connectData.willFlag = 0xc2;
	connectData.clientID.cstring = "my_mqtt_a";
	connectData.username.cstring = "7iidyq0/my_mqtt";
	connectData.password.cstring = "KsC6Pw0Loay42CIM";
	
	pvParameters = 0 ; 
	
	
	mqtt_handler(MQTT_BROKER , MQTT_PORT  , SUB_TOPIC , PUSH_TOPIC ,connectData , pvParameters , BAIDU_SER);
}


void mqtt_process_init(void)
{
	int ret;
	
#if 0
	ret = xTaskCreate( mqtt_process_thread,
			   MQTT_CLIENT_THREAD_NAME,
			   MQTT_CLIENT_THREAD_STACK_WORDS,
			   NULL,
			   MQTT_CLIENT_THREAD_PRIO,
			   &mqttc_client_handle );
#endif
	ret = xTaskCreate(mqtt_process_thread_huashengke,
						  MQTT_CLIENT_THREAD_NAME,
						  MQTT_CLIENT_THREAD_STACK_WORDS,
						  NULL,
						  MQTT_CLIENT_THREAD_PRIO,
						  &mqttc_client_handle);



	if ( ret != pdPASS )
	{
		DEBUG( "mqtt create client thread %s failed ret=%d\n", MQTT_CLIENT_THREAD_NAME, ret );
	}
}
