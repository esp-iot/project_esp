
#ifndef __MQTT_PROCESS_H__
#define __MQTT_PROCESS_H__

#define MQTT_BROKER  "7iidyq0.mqtt.iot.gz.baidubce.com"  /* MQTT Broker Address*/
//#define MQTT_BROKER  "192.168.1.4"
#define MQTT_PORT    1883             /* MQTT Port*/

#define MQTT_USRENAME    "7iidyq0/my_mqtt"             /* MQTT user name*/
#define MQTT_PASSWORD    "XAFhs6qLXLubmUeo"             /* MQTT password*/


enum MQTT_PROCESS_STATE_LIST{
    MATT_NETWORK_CONNECT,
    MATT_CONNECT,
    MATT_PUBLISH,
    MATT_FREE,
};

void mqtt_process_init(void);

#endif
