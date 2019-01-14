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
#include "user_config.h"

#include "esp_common.h"

#include "espressif/esp8266/ets_sys.h"

#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "lwip/sys.h"
#include "lwip/sockets.h"


#include <stdio.h> 
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>


//�ṹ��
typedef struct esp_mqtt_msg_type {
	int power;
	int cw;
	int ww;
	int r;
	int g;
	int b;
	int workMode;
	int skill;
	char allData[1024];
} xMessage;



#define MQTT_CLIENT_THREAD_NAME         "mqtt_client_thread"
#define MQTT_CLIENT_THREAD_STACK_WORDS  2000
#define MQTT_CLIENT_THREAD_PRIO         8

LOCAL xTaskHandle mqttc_client_handle;
//MQTT信息队列句柄
xQueueHandle MqttMessageQueueHandler;

static void messageArrived(MessageData* data)
{
    printf("Message arrived: %s\n", data->message->payload);
}

static void mqtt_client_thread(void* pvParameters)
{


	bool isNeedQueue = true;
	Network network;
	unsigned char sendbuf[2048], readbuf[2048] = { 0 };
	int rc = 0, count = 0;
	MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
	pvParameters = 0;
	NetworkInit(&network);

	MQTTClient client;
	MQTTClientInit(&client, &network, 30000, sendbuf, sizeof(sendbuf), readbuf,
			sizeof(readbuf));

	for (;;) {

		while (wifi_station_get_connect_status() != STATION_GOT_IP) {
			vTaskDelay(1000 / portTICK_RATE_MS);
		}

		char* address = MQTT_BROKER; //服务器地址,user_config.h 定义
		connectData.MQTTVersion = 3;
		connectData.clientID.cstring = "my_mqtt";
		connectData.username.cstring = "7iidyq0/my_mqtt";
		connectData.password.cstring = "KsC6Pw0Loay42CIM";
		connectData.keepAliveInterval = 40;
		connectData.cleansession = true;


		if ((rc = NetworkConnect(&network, address, MQTT_PORT)) != 0) {
			printf("MClouds NetworkConnect connect is %d\n", rc);
		}

		if ((rc = MQTTStartTask(&client)) != pdPASS) {
			printf("Return code from start tasks is %d\n", rc);
		} else {
			printf("Use MQTTStartTask\n");
		}


		if ((rc = MQTTConnect(&client, &connectData)) != 0) {
			printf("[SY] MClouds connect is %d\n", rc);
			network.disconnect(&network);
			vTaskDelay(1000 / portTICK_RATE_MS);
		}
#if 1

		if ((rc = MQTTSubscribe(&client, "$baidu/iot/shadow/my_mqtt/update/accepted", QOS0, messageArrived)) != 0) {
			printf("[SY] MClouds sub fail is %d\n", rc);
			network.disconnect(&network);
			vTaskDelay(1000 / portTICK_RATE_MS);//心跳频率是200hz，那么时间片是5ms。那么，vTaskDelay(1000/portTICK_RATE_MS );就是延迟200个时间片，也就是延迟1s。
		}else{

			printf("ok  rc=%d\n",rc);
		}
#endif

		xQueueReset(MqttMessageQueueHandler);

		while (1) {
#if 1

			char payload[2048] = {0};

			if (isNeedQueue) {

				struct esp_mqtt_msg_type *pMsg;
				printf("MqttMessageQueueHandler waitting ..\n");
				//xQueueReceive(MqttMessageQueueHandler, &pMsg, portMAX_DELAY);
				sprintf(payload, "%s", pMsg->allData);

				printf(" [SY] 1 MQTT get freeHeap: %d\n",
						system_get_free_heap_size());
			} else {

				//sprintf(payload, "%s", tempMsg.allData);

				os_printf(" [SY] 2 MQTT get freeHeap: %d\n",
						system_get_free_heap_size());
			}


			MQTTMessage message;
			message.qos = QOS0;
			message.retained = false;
			message.payload = (void*) payload;
			message.payloadlen = strlen(payload) + 1;

			if ((rc = MQTTPublish(&client, "$baidu/iot/shadow/my_mqtt/update", &message)) != 0) {
				printf("Return code from MQTT publish is %d\n", rc);
			} else {
				printf("MQTT publish succeed ..\n");
			}

			if (rc != 0) {
				isNeedQueue = false;
				break;
			} else {
				isNeedQueue = true;
			}
#endif
		}
		network.disconnect(&network);
	}

	printf("mqtt_client_thread going to be deleted\n");
	vTaskDelete(NULL);
	return;

}

void user_conn_init(void)
{
    int ret;
    ret = xTaskCreate(mqtt_client_thread,
                      MQTT_CLIENT_THREAD_NAME,
                      MQTT_CLIENT_THREAD_STACK_WORDS,
                      NULL,
                      MQTT_CLIENT_THREAD_PRIO,
                      &mqttc_client_handle);

    if (ret != pdPASS)  {
        printf("mqtt create client thread %s failed\n", MQTT_CLIENT_THREAD_NAME);
    }
}
