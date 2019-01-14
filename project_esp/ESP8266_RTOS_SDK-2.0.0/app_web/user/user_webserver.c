/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2016 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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




#include "lwip/lwip/mem.h"
//#include "user_interface.h"

#include "espconn.h"
#include "user_config.h"
#include "user_webserver.h"

#include "osapi.h"
#include "os_type.h"


#ifdef SERVER_SSL_ENABLE
#include "ssl/cert.h"
#include "ssl/private_key.h"
#endif






#define HTML	"<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/></head>"	\
				"<body align=\"center\"><h2>WiFi Settings</h2><form method='post'>" \
				"<input type='text' name='ssid' placeholder='ssid'><br>"	\
				"<input type='text' name='pass' placeholder='pass'><br>"  \
				"<input type='submit'><br></form></body></html>"


extern uint32 priv_param_start_sec;


LOCAL char *precvbuffer;
static uint32 dat_sumlength = 0;
LOCAL bool ICACHE_FLASH_ATTR
save_data(char *precv, uint16 length)
{
	bool flag = false;
	char length_buf[10] = {0};
	char *ptemp = NULL;
	char *pdata = NULL;
	uint16 headlength = 0;
	static uint32 totallength = 0;

	ptemp = (char *)os_strstr(precv, "\r\n\r\n");

	if (ptemp != NULL) {
		length -= ptemp - precv;
		length -= 4;
		totallength += length;
		headlength = ptemp - precv + 4;
		pdata = (char *)os_strstr(precv, "Content-Length: ");

		if (pdata != NULL) {
			pdata += 16;
			precvbuffer = (char *)os_strstr(pdata, "\r\n");

			if (precvbuffer != NULL) {
				os_memcpy(length_buf, pdata, precvbuffer - pdata);
				dat_sumlength = atoi(length_buf);
			}
		} else {
			if (totallength != 0x00){
				totallength = 0;
				dat_sumlength = 0;
				return false;
			}
		}
		if ((dat_sumlength + headlength) >= 1024) {
			precvbuffer = (char *)os_zalloc(headlength + 1);
			os_memcpy(precvbuffer, precv, headlength + 1);
		} else {
			precvbuffer = (char *)os_zalloc(dat_sumlength + headlength + 1);
			os_memcpy(precvbuffer, precv, os_strlen(precv));
		}
	} else {
		if (precvbuffer != NULL) {
			totallength += length;
			os_memcpy(precvbuffer + os_strlen(precvbuffer), precv, length);
		} else {
			totallength = 0;
			dat_sumlength = 0;
			return false;
		}
	}

	if (totallength == dat_sumlength) {
		totallength = 0;
		dat_sumlength = 0;
		return true;
	} else {
		return false;
	}
}

LOCAL bool ICACHE_FLASH_ATTR
check_data(char *precv, uint16 length)
{
		//bool flag = true;
	char length_buf[10] = {0};
	char *ptemp = NULL;
	char *pdata = NULL;
	char *tmp_precvbuffer;
	uint16 tmp_length = length;
	uint32 tmp_totallength = 0;
	
	ptemp = (char *)os_strstr(precv, "\r\n\r\n");
	
	if (ptemp != NULL) {
		tmp_length -= ptemp - precv;
		tmp_length -= 4;
		tmp_totallength += tmp_length;
		
		pdata = (char *)os_strstr(precv, "Content-Length: ");
		
		if (pdata != NULL){
			pdata += 16;
			tmp_precvbuffer = (char *)os_strstr(pdata, "\r\n");
			
			if (tmp_precvbuffer != NULL){
				os_memcpy(length_buf, pdata, tmp_precvbuffer - pdata);
				dat_sumlength = atoi(length_buf);
				os_printf("A_dat:%u,tot:%u,lenght:%u\n",dat_sumlength,tmp_totallength,tmp_length);
				if(dat_sumlength != tmp_totallength){
					return false;
				}
			}
		}
	}
	return true;
}

/******************************************************************************
 * FunctionName : data_send
 * Description  : processing the data as http format and send to the client or server
 * Parameters   : arg -- argument to set for client or server
 *				responseOK -- true or false
 *				psend -- The send data
 * Returns	  :
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
data_send(void *arg, bool responseOK, char *psend)
{
	uint16 length = 0;
	char *pbuf = NULL;
	char httphead[256];
	struct espconn *ptrespconn = arg;
	os_memset(httphead, 0, 256);

	if (responseOK) {
		os_sprintf(httphead,
				   "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\n",
				   psend ? os_strlen(psend) : 0);

		if (psend) {
			os_sprintf(httphead + os_strlen(httphead),
					   "Content-type: application/json\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n");
			length = os_strlen(httphead) + os_strlen(psend);
			pbuf = (char *)os_zalloc(length + 1);
			os_memcpy(pbuf, httphead, os_strlen(httphead));
			os_memcpy(pbuf + os_strlen(httphead), psend, os_strlen(psend));
		} else {
			os_sprintf(httphead + os_strlen(httphead), "\n");
			length = os_strlen(httphead);
		}
	} else {
		os_sprintf(httphead, "HTTP/1.0 400 BadRequest\r\n\
Content-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
		length = os_strlen(httphead);
	}

	if (psend) {
#ifdef SERVER_SSL_ENABLE
		espconn_secure_sent(ptrespconn, pbuf, length);
#else
		espconn_sent(ptrespconn, pbuf, length);
#endif
	} else {
#ifdef SERVER_SSL_ENABLE
		espconn_secure_sent(ptrespconn, httphead, length);
#else
		espconn_sent(ptrespconn, httphead, length);
#endif
	}

	if (pbuf) {
		os_free(pbuf);
		pbuf = NULL;
	}
}

/******************************************************************************
 * FunctionName : html_send
 * Description  : processing the data as http format and send to the client or server
 * Parameters   : arg -- argument to set for client or server
 *				responseOK -- true or false
 *				psend -- The send data
 * Returns	  :
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
html_send(void *arg, bool responseOK, char *psend)
{
	uint16 length = 0;
	char *pbuf = NULL;
	char httphead[256];
	struct espconn *ptrespconn = arg;
	os_memset(httphead, 0, 256);

	if (responseOK) {
		os_sprintf(httphead,
				   "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\n",
				   psend ? os_strlen(psend) : 0);

		if (psend) {
			os_sprintf(httphead + os_strlen(httphead),
					   "Content-type: text/html\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n");
			length = os_strlen(httphead) + os_strlen(psend);
			pbuf = (char *)os_zalloc(length + 1);
			os_memcpy(pbuf, httphead, os_strlen(httphead));
			os_memcpy(pbuf + os_strlen(httphead), psend, os_strlen(psend));
		} else {
			os_sprintf(httphead + os_strlen(httphead), "\n");
			length = os_strlen(httphead);
		}
	} else {
		os_sprintf(httphead, "HTTP/1.0 400 BadRequest\r\n\
Content-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
		length = os_strlen(httphead);
	}

	if (psend) {
#ifdef SERVER_SSL_ENABLE
		espconn_secure_sent(ptrespconn, pbuf, length);
#else
		espconn_sent(ptrespconn, pbuf, length);
#endif
	} else {
#ifdef SERVER_SSL_ENABLE
		espconn_secure_sent(ptrespconn, httphead, length);
#else
		espconn_sent(ptrespconn, httphead, length);
#endif
	}

	if (pbuf) {
		os_free(pbuf);
		pbuf = NULL;
	}
}

/******************************************************************************
 * FunctionName : parse_url
 * Description  : parse the received data from the server
 * Parameters   : precv -- the received data
 *				purl_frame -- the result of parsing the url
 * Returns	  : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
parse_url(char *precv, URL_Frame *purl_frame)
{
	char *str = NULL;
	uint8 length = 0;
	char *pbuffer = NULL;
	char *pbufer = NULL;

	if (purl_frame == NULL || precv == NULL) {
		return;
	}

	pbuffer = (char *)os_strstr(precv, "Host:");

	if (pbuffer != NULL) {
		length = pbuffer - precv;
		pbufer = (char *)os_zalloc(length + 1);
		pbuffer = pbufer;
		os_memcpy(pbuffer, precv, length);
		os_memset(purl_frame->pSelect, 0, 5*URLSize);
		os_memset(purl_frame->pCommand, 0, URLSize);
		os_memset(purl_frame->pFilename, 0, URLSize);

		if (os_strncmp(pbuffer, "GET ", 4) == 0) {
			purl_frame->Type = GET;
			pbuffer += 4;
		} else if (os_strncmp(pbuffer, "POST ", 5) == 0) {
			purl_frame->Type = POST;
			pbuffer += 5;
		}

		pbuffer ++;
		str = (char *)os_strstr(pbuffer, "?");

		if (str != NULL) {
			length = str - pbuffer;
			os_memcpy(purl_frame->pSelect, pbuffer, length);
			str ++;
			pbuffer = (char *)os_strstr(str, "=");

			if (pbuffer != NULL) {
				length = pbuffer - str;
				os_memcpy(purl_frame->pCommand, str, length);
				pbuffer ++;
				str = (char *)os_strstr(pbuffer, "&");

				if (str != NULL) {
					length = str - pbuffer;
					os_memcpy(purl_frame->pFilename, pbuffer, length);
				} else {
					str = (char *)os_strstr(pbuffer, " HTTP");

					if (str != NULL) {
						length = str - pbuffer;
						os_memcpy(purl_frame->pFilename, pbuffer, length);
					}
				}
			}
		}else{
			//如果没有参数直接把请求连接给pSelect
		   	pbuffer --;
			str = (char *)os_strstr(pbuffer, " HTTP/1");
			if (str != NULL) {
				length = str - pbuffer;
				os_memcpy(purl_frame->pSelect, pbuffer, length);
			}
		}
		os_free(pbufer);
	} else {
		return;
	}
}

LOCAL void ICACHE_FLASH_ATTR
webconfig_set_wifi(char* urlparam){
	char *p = NULL, *q = NULL;
	char ssid[10], pass[15];

	os_memset(ssid, 0, sizeof(ssid));
	os_memset(pass, 0, sizeof(pass));

	p = (char *)os_strstr(urlparam, "ssid=");
	q = (char *)os_strstr(urlparam, "pass=");
	if ( p == NULL || q == NULL ){
		return;
	}
	os_memcpy(ssid, p + 5, q - p - 6);
	os_memcpy(pass, q + 5, os_strlen(urlparam) - (q - urlparam) - 5);
	os_printf("ssid[%s]pass[%s]\r\n", ssid, pass);
	
	wifi_set_opmode(STATION_MODE);
	struct station_config stConf;
	stConf.bssid_set = 0;
	os_memset(&stConf.ssid, 0, sizeof(stConf.ssid));
	os_memset(&stConf.password, 0, sizeof(stConf.password));
	
	os_memcpy(&stConf.ssid, ssid, os_strlen(ssid));
	os_memcpy(&stConf.password, pass, os_strlen(pass));

	wifi_station_set_config(&stConf);
//	wifi_station_disconnect();
//	wifi_station_connect();
//	wifi_station_set_reconnect_policy(TRUE);
//	wifi_station_set_auto_connect(TRUE);
	system_restart();
	return ;
}

/******************************************************************************
 * FunctionName : webserver_recv
 * Description  : Processing the received data from the server
 * Parameters   : arg -- Additional argument to pass to the callback function
 *				pusrdata -- The received data (or NULL when the connection has been closed!)
 *				length -- The length of received data
 * Returns	  : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
webserver_recv(void *arg, char *pusrdata, unsigned short length){
	struct espconn *ptrespconn = arg;
	URL_Frame *pURL_Frame = NULL;
	char *pParseBuffer = NULL;
	char szOut[512];
	int stat = -1;
	
	static char post_data[1024];
	static bool last_post = false;

	os_printf("length[%d]\r\n", length);
	os_printf("recv[%s]\r\n", pusrdata);
	
	if(check_data(pusrdata, length) == false){
		/*---POST BUG---*/
		last_post = true;
		os_memset(post_data, 0, sizeof(post_data));
		os_memcpy(post_data, pusrdata, length);
		/*---POST BUG---*/
		os_printf("check error\r\n");
//		data_send(ptrespconn, false, NULL);
		return ;
	}
	/*---POST BUG--END--*/
	if(last_post){
		last_post = false;
		os_strcat(post_data, pusrdata);
		os_printf("all data:[%u]\r\n[%s]\r\n", os_strlen(post_data), post_data);
		if(save_data(post_data, strlen(post_data)) == false){
			os_printf("save_data error\r\n");
			data_send(ptrespconn, false, NULL);
			return ;
		}
	}else{
	/*---POST BUG--END--*/
	if(save_data(pusrdata, length) == false){
		os_printf("save_data error\r\n");
		data_send(ptrespconn, false, NULL);
		return ;
	}
	}
	pURL_Frame = (URL_Frame *)os_zalloc(sizeof(URL_Frame));
	parse_url(precvbuffer, pURL_Frame);
	os_printf("Type[%d]\r\n", pURL_Frame->Type);
	os_printf("pSelect[%s]\r\n", pURL_Frame->pSelect);
	os_printf("pCommand[%s]\r\n", pURL_Frame->pCommand);
	os_printf("pFilename[%s]\r\n", pURL_Frame->pFilename);
	switch (pURL_Frame->Type) {
		case GET:
			os_printf("We have a GET request.\n");
			if((strlen(pURL_Frame->pSelect) == 1) && 
				(os_strncmp(pURL_Frame->pSelect, "/", 1) == 0)){
				//获取webconfig页面
				html_send(ptrespconn, true, HTML);
			}
			if(os_strcmp(pURL_Frame->pSelect, "config") == 0 &&
				os_strcmp(pURL_Frame->pCommand, "command") == 0) {
				os_memset(szOut, 0, sizeof(szOut));

				if(stat==0){
					os_printf("get22szOut[%s]\r\n", szOut);
					data_send(ptrespconn, true, szOut);
				}else
					data_send(ptrespconn, false, NULL);
			}else
				data_send(ptrespconn, false, NULL);
			break ;
		case POST:
			os_printf("We have a POST request.\n");
			pParseBuffer = (char *)os_strstr(precvbuffer, "\r\n\r\n");
			if (pParseBuffer == NULL) {
				break;
			}
			pParseBuffer += 4;
			os_printf("pParseBuffer[%s]\r\n", pParseBuffer);
			
			if((strlen(pURL_Frame->pSelect) == 1) && 
				(os_strncmp(pURL_Frame->pSelect, "/", 1) == 0)){
				webconfig_set_wifi(pParseBuffer);
				data_send(ptrespconn, false, NULL);
				break ;
			}else if (os_strcmp(pURL_Frame->pSelect, "config") == 0 &&
				os_strcmp(pURL_Frame->pCommand, "command") == 0) {
				os_memset(szOut, 0, sizeof(szOut));

				if(stat == 0){
					os_printf("post22szOut[%s]\r\n", szOut);
					data_send(ptrespconn, true, szOut);
				}else
					data_send(ptrespconn, false, NULL);
				
			} else {
				os_printf("\r\nparse error!\r\n");
				data_send(ptrespconn, false, NULL);
			}
			break ;
	}
	
	if (precvbuffer != NULL){
		os_free(precvbuffer);
		precvbuffer = NULL;
	}
	os_free(pURL_Frame);
	pURL_Frame = NULL;
}

/******************************************************************************
 * FunctionName : webserver_recon
 * Description  : the connection has been err, reconnection
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns	  : none
*******************************************************************************/
LOCAL ICACHE_FLASH_ATTR
void webserver_recon(void *arg, sint8 err)
{
	struct espconn *pesp_conn = arg;

	os_printf("webserver's %d.%d.%d.%d:%d err %d reconnect\n", pesp_conn->proto.tcp->remote_ip[0],
			pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
			pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port, err);
}

/******************************************************************************
 * FunctionName : webserver_recon
 * Description  : the connection has been err, reconnection
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns	  : none
*******************************************************************************/
LOCAL ICACHE_FLASH_ATTR
void webserver_discon(void *arg)
{
	struct espconn *pesp_conn = arg;

	os_printf("webserver's %d.%d.%d.%d:%d disconnect\n", pesp_conn->proto.tcp->remote_ip[0],
				pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
				pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port);
}

/******************************************************************************
 * FunctionName : user_accept_listen
 * Description  : server listened a connection successfully
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns	  : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
webserver_listen(void *arg)
{
	struct espconn *pesp_conn = arg;

	espconn_regist_recvcb(pesp_conn, webserver_recv);
	espconn_regist_reconcb(pesp_conn, webserver_recon);
	espconn_regist_disconcb(pesp_conn, webserver_discon);
}

/******************************************************************************
 * FunctionName : user_webserver_init
 * Description  : parameter initialize as a server
 * Parameters   : port -- server port
 * Returns	  : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_webserver_init(uint32 port)
{
	LOCAL struct espconn esp_conn;
	LOCAL esp_tcp esptcp;

	esp_conn.type = ESPCONN_TCP;
	esp_conn.state = ESPCONN_NONE;
	esp_conn.proto.tcp = &esptcp;
	esp_conn.proto.tcp->local_port = port;
	espconn_regist_connectcb(&esp_conn, webserver_listen);

#ifdef SERVER_SSL_ENABLE
	espconn_secure_set_default_certificate(default_certificate, default_certificate_len);
	espconn_secure_set_default_private_key(default_private_key, default_private_key_len);
	espconn_secure_accept(&esp_conn);
#else
	espconn_accept(&esp_conn);
#endif
}
