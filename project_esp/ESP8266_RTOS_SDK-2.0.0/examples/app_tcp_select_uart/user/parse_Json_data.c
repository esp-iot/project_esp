#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt/MQTTClient.h"
#include "log_print.h"


#include "lwip/sys.h"
#include "lwip/sockets.h"

#include "json/cjson.h"
//#include "parse_Json_data.h"
#include "mqtt_process.h"


/*
Parameters:
char * pJson : The json data received
int fd : TCP file descriptor , If fd is 0, it indicates MQTT communication

*/

int parse_Json_1(char * pJson , int fd )
{
	cJSON * pSub_1 = NULL , * pRoot = NULL , * pSub_2 = NULL;
	int num = -1;
	/*
	   例如，
	   1.这里b.c中定义了一个变量num，如果main.c中想要引用这个变量，
	   那么可以使用extern这个关键字，注意这里能成功引用的原因是，
	   num这个关键字在b.c中是一个全局变量，也就是说只有当一个变量是一个全局变量时，
	   extern变量才会起作用,
	   2.另外，extern关键字只需要指明类型和变量名就行了，不能再重新赋值

*/

	extern char payload[1024];
	extern u8	process_state;

	int esp_8266_id		= system_get_chip_id(); /* 设备id */
	int esp_led_stat = -1;

	DEBUG("----1\n");
	if(NULL == pJson)
	{
		DEBUG("----2\n");
		return -3;
	}

	pRoot = cJSON_Parse(pJson);
	if(NULL == pRoot)
	{
		DEBUG("----3\n");
		return -2;
	}

	if(cJSON_GetObjectItem(pRoot, "device_id") == NULL && cJSON_GetObjectItem(pRoot, "get") == NULL ){
		DEBUG("----4\n");
		return -1;
	}

	pSub_1 = cJSON_GetObjectItem(pRoot, "device_id");
	if(NULL != pSub_1)
	{
		DEBUG( "%s %d cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", __FUNCTION__,__LINE__,pSub_1->type, pSub_1->string, pSub_1->valuestring, pSub_1->valueint );
		if(pSub_1->valueint != esp_8266_id)
		{
			DEBUG("Matching failure\n");
			return -1;

		}


		pSub_1 = cJSON_GetObjectItem(pRoot, "get");
		if(NULL != pSub_1)
		{

			DEBUG( "cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", pSub_1->type, pSub_1->string, pSub_1->valuestring, pSub_1->valueint );
			DEBUG("pSub_1->valuestring = %s\n",pSub_1->valuestring);



			if(strstr(pSub_1->valuestring , "led_stat") != NULL){
				DEBUG("----led_stat\n");
				memset(payload,0,sizeof(payload));
				sprintf(payload , "{\"device_id\": %d , \"led_stat\":1 }" , esp_8266_id);
				DEBUG("fd = %d ,payload = %s\n ",fd , payload);
				if(fd != 0){ //tcp
					//回复客户端
					num = send(fd,payload,sizeof(payload),0);
					if(num < 0){
						perror("fail to send");
						return ;
					}else{
						//DEBUG("send reply\n");
					}
				}
				else{//mqtt
					process_state = MATT_PUBLISH;
				}
			}

			if(strstr(pSub_1->valuestring , "device_stat") != NULL){
				memset(payload,0,sizeof(payload));
				sprintf(payload , "{\"device_id\": %d , \"device_stat\":1 }" , esp_8266_id);
				DEBUG("fd = %d ,payload = %s\n ",fd , payload);
				if(fd != 0){ //tcp
					//回复客户端
					num = send(fd,payload,sizeof(payload),0);
					if(num < 0){
						perror("fail to send");
						return ;
					}else{
						//DEBUG("send reply\n");
					}
				}
				else{//mqtt
					process_state = MATT_PUBLISH;
				}
			}

			if(strstr(pSub_1->valuestring , "temperature_value") != NULL){
				DEBUG("----temperature_value\n");
				memset(payload,0,sizeof(payload));
				sprintf(payload , "{\"device_id\": %d , \"temperature_value\":22 }" , esp_8266_id);
				DEBUG("fd = %d ,payload = %s\n ",fd , payload);
				if(fd != 0){ //tcp
					//回复客户端
					num = send(fd,payload,sizeof(payload),0);
					if(num < 0){
						perror("fail to send");
						return ;
					}else{
						//DEBUG("send reply\n");
					}
				}
				else{//mqtt
					process_state = MATT_PUBLISH;
				}
			}

			if(strstr(pSub_1->valuestring , "humidity_value") != NULL){
				DEBUG("----humidity_value\n");
				memset(payload,0,sizeof(payload));
				sprintf(payload , "{\"device_id\": %d , \"humidity_value\":12 }" , esp_8266_id);
				DEBUG("fd = %d ,payload = %s\n ",fd , payload);
				if(fd != 0){ //tcp
					//回复客户端
					num = send(fd,payload,sizeof(payload),0);
					if(num < 0){
						perror("fail to send");
						return ;
					}else{
						//DEBUG("send reply\n");
					}
				}
				else{//mqtt
					process_state = MATT_PUBLISH;
				}
			}


			if(strstr(pSub_1->valuestring , "get_all_data") != NULL){
				DEBUG("----humidity_value\n");
			}

		}


		//char set_led_stat[]="{\"device_id\": 1234567 ,\"set\":  {\"led_stat\": 11}}";
		pSub_1 = cJSON_GetObjectItem(pRoot, "set");
		if(NULL != pSub_1)
		{
			DEBUG( "%s %d cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", __FUNCTION__,__LINE__,pSub_1->type, pSub_1->string, pSub_1->valuestring, pSub_1->valueint );
			pSub_2 = cJSON_GetObjectItem(pSub_1, "led_stat");
			if(NULL != pSub_2)
			{
				DEBUG( "%s %d cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", __FUNCTION__,__LINE__,pSub_2->type, pSub_2->string, pSub_2->valuestring, pSub_2->valueint );
				esp_led_stat = dec2hex(pSub_2->valueint); 
				DEBUG("esp_led_stat = %x ， %d\n",esp_led_stat,esp_led_stat);
				ReadInverter(0x01 , 0x06 ,0x02 , esp_led_stat);
			}else{
				if(strstr(pSub_1->valuestring , "system_restore") != NULL){
					DEBUG("--------system restore\n");
					system_restore();//恢复出厂设置
					system_restart();//重启
				}

				if(strstr(pSub_1->valuestring , "reboot") != NULL){
					DEBUG("--------system restore\n");
					system_restart();//重启
				}


			}


		}

	}


	else{
		pSub_1 = cJSON_GetObjectItem(pRoot, "get");
		if(NULL != pSub_1)
		{
			DEBUG( "cJSON_GetObjectItem: type=%d, string is %s valuestring is %s ,item->valueint %d\n", pSub_1->type, pSub_1->string, pSub_1->valuestring, pSub_1->valueint );

			if(strcmp(pSub_1->valuestring , "device_id") == 0){
				memset(payload,0,sizeof(payload));
				sprintf(payload , "{\"device_id\": %d }" , esp_8266_id);
				DEBUG("fd = %d ,payload = %s\n ",fd , payload);
				if(fd != 0){ //tcp
					//回复客户端

					num = send(fd,payload,sizeof(payload),0);
					//num = write(fd, pJson , sizeof(pJson));
					if(num < 0){
						perror("fail to send");
						return ;
					}else{
						//DEBUG("send reply\n");
					}
				}
				else{//mqtt
					process_state = MATT_PUBLISH;
				}
			}

		}


	}






	cJSON_Delete(pRoot);
	return 0;
}

