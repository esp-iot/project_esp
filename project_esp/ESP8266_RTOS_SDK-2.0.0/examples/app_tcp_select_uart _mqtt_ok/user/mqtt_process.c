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

#define MQTT_CLIENT_THREAD_NAME         "mqtt_client_thread"
#define MQTT_CLIENT_THREAD_STACK_WORDS  3000
#define MQTT_CLIENT_THREAD_PRIO         8

LOCAL xTaskHandle mqttc_client_handle;
MQTTClient client;
u8 process_state = MATT_NETWORK_CONNECT;
unsigned char sendbuf[512], readbuf[512] = {0};

typedef struct  
{  
    int led; 
}state;  


void get_led_state(char *json_info, int len)
{
    cJSON * root = NULL;
    cJSON * item = NULL;//cjson����
	cJSON * object=NULL;

    char *pstart = json_info;
    int i;
	state led_state[2];

    DEBUG(" Json len = %d\n", len);

    for(i=0; i<len; i++)
    {
        if(*pstart == '{')
            break;
        else
            pstart++;
    }

    DEBUG(" Json pstart = %c\n", *pstart);

    if(i == len) return;

    root = cJSON_Parse(pstart);
	printf("%s\n\n", cJSON_Print(root));//有格式的方式打印Json
	if(root == NULL)
	{
        DEBUG("Error before: [%s]\n",cJSON_GetErrorPtr());
    }
	else
	{
		int size=cJSON_GetArraySize(root);  //获取cjson对象数组成员的个数
        printf("cJSON_GetArraySize: size=%d\n",size);  

		for(i=0;i<size;i++)  
        {  
            printf("i=%d\n",i);  
            object=cJSON_GetArrayItem(root,i);  
  
           	item=cJSON_GetObjectItem(object,"LED");  
			if(item!=NULL)  
            {  
                 printf("cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n",item->type,item->string,item->valuestring,item->valueint);  
                 //memcpy(led_state[i].led,item->valuestring,strlen(item->valuestring)); 
				 led_state[i].led = item->valueint;
				 GPIO_OUTPUT_SET(13, led_state[i].led);
				 //led_control(13,led_state[i].led);
            }  
			
		}
	}
	cJSON_Delete(root);
#if 0
	if (!root)
    
    else
    {
    	printf("111111111\n");
        item = cJSON_GetObjectItem(root, "ledState");
        //printf("%s %d\r\n", item->string, item->valueint);
        GPIO_OUTPUT_SET(2, item->valueint);
    }

    
#endif
}

static void message_arrived(MessageData* data)
{
    printf("Message arrived: %s\n", data->message->payload);

    get_led_state(data->message->payload, data->message->payloadlen);

    memset(readbuf, 0, sizeof(readbuf));
}

static void mqtt_process_thread(void* pvParameters)
{
    printf("mqtt client thread starts\n");

    Network network;
    int rc = 0;
    int count = 0;
    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
    char* address = MQTT_BROKER;
    int port = MQTT_PORT;
    u8 process_state = MATT_NETWORK_CONNECT;
    MQTTMessage message;
    char payload[30];
    char *sub_topic = "$baidu/iot/shadow/my_mqtt/update/accepted";

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
                    printf("Return code from network connect is %d\n", rc);
                    process_state = MATT_NETWORK_CONNECT;
                }
                else
                {
                    process_state = MATT_CONNECT;
                }
                break;
            case MATT_CONNECT:
                //connectData.willFlag = 0xc2;
                connectData.clientID.cstring = "my_mqtt_a";
                connectData.username.cstring = "7iidyq0/my_mqtt";
                connectData.password.cstring = "XAFhs6qLXLubmUeo";

                if ((rc = MQTTConnect(&client, &connectData)) != 0)
                {
                    printf("Return code from MQTT connect is %d\n", rc);
                    process_state = MATT_NETWORK_CONNECT;
                }
                else
                {
                    printf("MQTT Connected\n");
                    process_state = MATT_FREE;
                    //process_state = MATT_PUBLISH
                    #if defined(MQTT_TASK)
                    if ((rc = MQTTStartTask(&client)) != pdPASS)
                    {
                        printf("Return code from start tasks is %d\n", rc);
                    }
                    else
                    {
                        printf("Use MQTTStartTask\n");
                    }
                    #endif


                    if ((rc = MQTTSubscribe(&client, sub_topic, 0, message_arrived)) != 0)
                    {
                        printf("Return code from MQTT subscribe is %d\n", rc);
                    }
                    else
                    {
                        printf("MQTT subscribe to topic \"%s\"\n", sub_topic);
                    }
                }
                break;
            case MATT_PUBLISH:
                message.qos = QOS2;
                message.retained = 0;
                message.payload = payload;
                sprintf(payload, "{\"reported\": {\"Temperature\": %d}}", count);
                message.payloadlen = strlen(payload);

                if ((rc = MQTTPublish(&client, "$baidu/iot/shadow/my_mqtt/update/", &message)) != 0)
                {
                    printf("Return code from MQTT publish is %d\n", rc);
                    process_state = MATT_NETWORK_CONNECT;
                    MQTTDisconnect(&client);
                }
                else
                {
                    printf("MQTT publish topic \"ESP8266/sample/pub\", "
                            "message number is %d\n", count++);

                    process_state = MATT_PUBLISH;
                }
                break;
            default:
                break;
        }

        vTaskDelay(10 / portTICK_RATE_MS);  //send every 1 seconds
    }

    printf("mqtt_client_thread going to be deleted\n");
    vTaskDelete(NULL);
    return;
}

void mqtt_process_init(void)
{
    int ret;
    ret = xTaskCreate(mqtt_process_thread,
                      MQTT_CLIENT_THREAD_NAME,
                      MQTT_CLIENT_THREAD_STACK_WORDS,
                      NULL,
                      MQTT_CLIENT_THREAD_PRIO,
                      &mqttc_client_handle);

    if (ret != pdPASS)
    {
        printf("mqtt create client thread %s failed ret=%d\n", MQTT_CLIENT_THREAD_NAME,ret);
    }
}
