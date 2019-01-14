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


#define MQTT_CLIENT_THREAD_NAME         "mqtt_client_thread"
#define MQTT_CLIENT_THREAD_STACK_WORDS  2048
#define MQTT_CLIENT_THREAD_PRIO         8

LOCAL xTaskHandle mqttc_client_handle;

static void messageArrived(MessageData* data)
{
    printf("Message arrived: %s\n", data->message->payload);
}


/******************************************************************************
 * FunctionName : get_my_id
 * Description  : generate the id using Mac address for mqtt_task mqtt_client_id 
 * Parameters   :none
 * Returns      : my_id
*******************************************************************************/
LOCAL const char * ICACHE_FLASH_ATTR get_my_id(void)
{
    // Use MAC address for Station as unique ID
    static char my_id[13];
    static bool my_id_done = false;
    int8_t i;
    uint8_t x;
    if (my_id_done)
        return my_id;
    if (!wifi_get_macaddr(STATION_IF, my_id))
        return NULL;
    for (i = 5; i >= 0; --i)
    {
        x = my_id[i] & 0x0F;
        if (x > 9) x += 7;
        my_id[i * 2 + 1] = x + '0';
        x = my_id[i] >> 4;
        if (x > 9) x += 7;
        my_id[i * 2] = x + '0';
    }
    my_id[12] = '\0';
    my_id_done = true;
    return my_id;
}


static void mqtt_client_thread(void* pvParameters)
{
    printf("mqtt client thread starts\n");
    MQTTClient client;
    Network network;
    unsigned char sendbuf[80], readbuf[80] = {0};
    int rc = 0, count = 0;
	char mqtt_client_id[120] = "";
    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

    pvParameters = 0;
    NetworkInit(&network);
    MQTTClientInit(&client, &network, 30000, sendbuf, sizeof(sendbuf), readbuf, sizeof(readbuf));

    char* address = MQTT_BROKER;
	uint32 my_id =  system_get_chip_id()
	printf("my_id=%s\n",my_id);
	// Unique client ID
    strcpy(mqtt_client_id, "ESP-");
    strcat(mqtt_client_id, my_id);

	printf("mqtt_client_id=%s\n",mqtt_client_id);

    if ((rc = NetworkConnect(&network, address, MQTT_PORT)) != 0) {
        printf("Return code from network connect is %d\n", rc);
    }

#if defined(MQTT_TASK)

    if ((rc = MQTTStartTask(&client)) != pdPASS) {
        printf("Return code from start tasks is %d\n", rc);
    } else {
        printf("Use MQTTStartTask\n");
    }

#endif

	connectData.MQTTVersion = 3;
	connectData.clientID.cstring = mqtt_client_id;
	connectData.username.cstring = MQTT_USER;
	connectData.password.cstring = MQTT_PASS;
	connectData.keepAliveInterval = 140;
	connectData.cleansession = true;

    if ((rc = MQTTConnect(&client, &connectData)) != 0) {
        printf("Return code from MQTT connect is %d\n", rc);
    } else {
        printf("MQTT Connected\n");
    }


	



#if 1

	MQTTMessage message;
    char payload[128] = "{\"reported\": {\"Temperature\": 32}}";

    message.qos = QOS0;
    message.retained = 0;
    message.payload = payload;
    message.payloadlen = strlen(payload);

    if ((rc = MQTTPublish(&client, "$baidu/iot/shadow/my_mqtt/update", &message)) != 0) {
         printf("Return code from MQTT publish is %d\n", rc);
    } else {
         printf("MQTT publish topic \"baidu/iot/shadow/my_mqtt/update\", message number is %d\n", count);
    }

	if ((rc = MQTTSubscribe(&client, "$baidu/iot/shadow/my_mqtt/update/accepted", QOS0, messageArrived)) != 0) {
	      printf("Return code from MQTT subscribe is %d\n", rc);
	 } else {
	      printf("MQTT subscribe to topic \"baidu/iot/shadow/my_mqtt/update/accepted\" rc =%d\n",rc);
	 }

#endif
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
