
#ifndef __MQTT_PROCESS_H__
#define __MQTT_PROCESS_H__




#define MQTT_BROKER  "7iidyq0.mqtt.iot.gz.baidubce.com"  /* MQTT Broker Address*/
//#define MQTT_BROKER  "192.168.1.4"
#define MQTT_PORT    1883             /* MQTT Port*/

#define MQTT_USRENAME    "7iidyq0/my_mqtt"             /* MQTT user name*/
#define MQTT_PASSWORD    "XAFhs6qLXLubmUeo"             /* MQTT password*/

#define SUB_TOPIC   "$baidu/iot/shadow/my_mqtt/update/accepted"
#define PUSH_TOPIC "$baidu/iot/shadow/my_mqtt/update/"



#define MQTT_BROKER_HuaShengKe  "t2a3267714.51mypc.cn"  /* MQTT Broker Address*/
#define MQTT_PORT_HuaShengKe   31949             /* MQTT Port*/

#define SUB_TOPIC_HuaShengKe  "$huashengke/iot/app/update/"
#define PUSH_TOPIC_HuaShengKe  "$huashengke/iot/esp/update/"


enum MQTT_PROCESS_STATE_LIST{
    MATT_NETWORK_CONNECT,
    MATT_CONNECT,
    MATT_PUBLISH,
    MATT_FREE,
};

enum MQTT_CLOUD{
    HUASHENGKE_SER,
    BAIDU_SER,
    ALI_SER,
	//...
};
	




void mqtt_process_init(void);

#endif
