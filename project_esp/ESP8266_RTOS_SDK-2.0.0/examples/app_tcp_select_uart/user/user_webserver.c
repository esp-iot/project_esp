
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

#include "user_webserver.h"


//#include "osapi.h"
//#include "at_custom.h"
//#include "user_interface.h"
#include "espconn.h"
#include "espressif/esp8266/ets_sys.h"
#include "lwip/mem.h"


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

	pbuffer = (char *)strstr(precv, "Host:");

	if (pbuffer != NULL) {
		length = pbuffer - precv;
		pbufer = (char *)os_zalloc(length + 1);
		pbuffer = pbufer;
		memcpy(pbuffer, precv, length);
		memset(purl_frame->pSelect, 0, 5*URLSize);
		memset(purl_frame->pCommand, 0, URLSize);
		memset(purl_frame->pFilename, 0, URLSize);

		if (strncmp(pbuffer, "GET ", 4) == 0) {
			purl_frame->Type = GET;
			pbuffer += 4;
		} else if (strncmp(pbuffer, "POST ", 5) == 0) {
			purl_frame->Type = POST;
			pbuffer += 5;
		}

		pbuffer ++;
		str = (char *)strstr(pbuffer, "?");

		if (str != NULL) {
			length = str - pbuffer;
			memcpy(purl_frame->pSelect, pbuffer, length);
			str ++;
			pbuffer = (char *)strstr(str, "=");

			if (pbuffer != NULL) {
				length = pbuffer - str;
				memcpy(purl_frame->pCommand, str, length);
				pbuffer ++;
				str = (char *)strstr(pbuffer, "&");

				if (str != NULL) {
					length = str - pbuffer;
					memcpy(purl_frame->pFilename, pbuffer, length);
				} else {
					str = (char *)strstr(pbuffer, " HTTP");

					if (str != NULL) {
						length = str - pbuffer;
						memcpy(purl_frame->pFilename, pbuffer, length);
					}
				}
			}
		}else{
		   	pbuffer --;
			str = (char *)strstr(pbuffer, " HTTP/1");
			if (str != NULL) {
				length = str - pbuffer;
				memcpy(purl_frame->pSelect, pbuffer, length);
			}
		}
		free(pbufer);
	} else {
		return;
	}
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
	memset(httphead, 0, 256);

	if (responseOK) {
		sprintf(httphead,
				   "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\n",
				   psend ? strlen(psend) : 0);

		if (psend) {
			sprintf(httphead + strlen(httphead),
					   "Content-type: text/html\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n");
			length = strlen(httphead) + strlen(psend);
			pbuf = (char *)os_zalloc(length + 1);
			memcpy(pbuf, httphead, strlen(httphead));
			memcpy(pbuf + strlen(httphead), psend, strlen(psend));
		} else {
			sprintf(httphead + strlen(httphead), "\n");
			length = strlen(httphead);
		}
	} else {
		sprintf(httphead, "HTTP/1.0 400 BadRequest\r\n\
Content-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
		length = strlen(httphead);
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
		free(pbuf);
		pbuf = NULL;
	}
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
	memset(httphead, 0, 256);

	if (responseOK) {
		sprintf(httphead,
				   "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\n",
				   psend ? strlen(psend) : 0);

		if (psend) {
			sprintf(httphead + strlen(httphead),
					   "Content-type: application/json\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n");
			length = strlen(httphead) + strlen(psend);
			pbuf = (char *)os_zalloc(length + 1);
			memcpy(pbuf, httphead, strlen(httphead));
			memcpy(pbuf + strlen(httphead), psend, strlen(psend));
		} else {
			sprintf(httphead + strlen(httphead), "\n");
			length = strlen(httphead);
		}
	} else {
		sprintf(httphead, "HTTP/1.0 400 BadRequest\r\n\
Content-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
		length = strlen(httphead);
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
		free(pbuf);
		pbuf = NULL;
	}
}

LOCAL void ICACHE_FLASH_ATTR
webconfig_set_wifi(char* urlparam){
	char *p = NULL, *q = NULL;
	char ssid[10], pass[15];

	memset(ssid, 0, sizeof(ssid));
	memset(pass, 0, sizeof(pass));

	p = (char *)strstr(urlparam, "ssid=");
	q = (char *)strstr(urlparam, "pass=");
	if ( p == NULL || q == NULL ){
		return;
	}
	memcpy(ssid, p + 5, q - p - 6);
	memcpy(pass, q + 5, strlen(urlparam) - (q - urlparam) - 5);
	printf("ssid[%s]pass[%s]\r\n", ssid, pass);
	
	wifi_set_opmode(STATION_MODE);
	struct station_config stConf;
	stConf.bssid_set = 0;
	memset(&stConf.ssid, 0, sizeof(stConf.ssid));
	memset(&stConf.password, 0, sizeof(stConf.password));
	
	memcpy(&stConf.ssid, ssid, strlen(ssid));
	memcpy(&stConf.password, pass, strlen(pass));

	wifi_station_set_config(&stConf);
//	wifi_station_disconnect();
//	wifi_station_connect();
//	wifi_station_set_reconnect_policy(TRUE);
//	wifi_station_set_auto_connect(TRUE);
	system_restart();
	return ;
}


void ICACHE_FLASH_ATTR
webserver_recv(void *arg, char *pusrdata, unsigned short length)
{
    URL_Frame *pURL_Frame = NULL;
    char *pParseBuffer = NULL;
    char *index = NULL;
    SpiFlashOpResult ret = 0;
	struct espconn *ptrespconn = arg;

	char szOut[512];
	int stat = -1;

    printf("len:%u\r\n",length);
    printf("Webserver recv:-------------------------------\r\n%s\r\n", pusrdata);

    pURL_Frame = (URL_Frame *)os_zalloc(sizeof(URL_Frame));
    parse_url(pusrdata, pURL_Frame);

	printf("Type[%d]\r\n", pURL_Frame->Type);
	printf("pSelect[%s]\r\n", pURL_Frame->pSelect);
	printf("pCommand[%s]\r\n", pURL_Frame->pCommand);
	printf("pFilename[%s]\r\n", pURL_Frame->pFilename);
	/*

	powerOn=1
	
	Type[1]
	
	pSelect[/setLight]
	
	pCommand[]
	
	pFilename[]
	*/

    switch (pURL_Frame->Type) {
        case GET:
            printf("We have a GET request.\r\n");

            if(pURL_Frame->pFilename[0] == 0){
                index = (char *)os_zalloc(FLASH_READ_SIZE+1);
                if(index == NULL){
                    printf("os_zalloc error!\r\n");
                    goto _temp_exit;
                }

                // Flash read/write has to be aligned to the 4-bytes boundary
                ret = spi_flash_read(0xD0*SPI_FLASH_SEC_SIZE, (uint32 *)index, FLASH_READ_SIZE);  // start address:0x10000 + 0xC0000
                if(ret != SPI_FLASH_RESULT_OK){
                    printf("spi_flash_read err:%d\r\n", ret);
                    free(index);
                    index = NULL;
                    goto _temp_exit;
                }

		/*在拿到了网页信息之后，要自己设置字符串内容的结束符，这就需要我们的Html文件有多大？
		注意：我们要的是显示全部内容下的时候才拿到这个Html文件大小，注意我们上面的是格式符%s，取出来的当然会小很多！*/
                index[HTML_FILE_SIZE] = 0;   // put 0 to the end
				html_send(ptrespconn, true, index);
				//data_send(arg, true, index);
				//espconn_send((struct espconn *)arg,index,sizeof(index));
                free(index);
                index = NULL;
            }
            break;

        case POST:
			os_printf("We have a POST request.\n");
			pParseBuffer = (char *)strstr(pusrdata, "\r\n\r\n");
			if (pParseBuffer == NULL) {
				break;
			}
			pParseBuffer += 4;
			printf("pParseBuffer[%s]\r\n", pParseBuffer);
			
			if((strlen(pURL_Frame->pSelect) == 1) && 
				(strncmp(pURL_Frame->pSelect, "/", 1) == 0)){
				webconfig_set_wifi(pParseBuffer);
				data_send(ptrespconn, false, NULL);
				break ;
			}else if (strcmp(pURL_Frame->pSelect, "config") == 0 &&
				strcmp(pURL_Frame->pCommand, "command") == 0) {
				memset(szOut, 0, sizeof(szOut));

				if(stat == 0){
					printf("post22szOut[%s]\r\n", szOut);
					data_send(ptrespconn, true, szOut);
				}else
					data_send(ptrespconn, false, NULL);
				
			} else if(strcmp(pURL_Frame->pSelect, "/setLight") == 0 &&
				strcmp(pParseBuffer, "powerOn=1") == 0){

				printf("-----------led on\n");
			}

			else {
				printf("\r\nparse error!\r\n");
				data_send(ptrespconn, false, NULL);
			}
			break ;

#if 0	
            printf("We have a POST request.\r\n");

			printf("pusrdata = %s \n",pusrdata);
            pParseBuffer = (char *)strstr(pusrdata, "\r\n\r\n");
            if (pParseBuffer == NULL) {
                data_send(arg, false, NULL);
                break;
            }
            // Prase the POST data ...
            break;
#endif
    }

    _temp_exit:
        ;
    if(pURL_Frame != NULL){
        free(pURL_Frame);
        pURL_Frame = NULL;
    }
}

void ICACHE_FLASH_ATTR webserver_sent(void *arg){//发送数据成功的回调函数
    printf("send data ok!!\r\n");
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


LOCAL void ICACHE_FLASH_ATTR
webserver_listen(void *arg)
{
    struct espconn *pesp_conn = arg;

    espconn_regist_recvcb(pesp_conn, webserver_recv); 
    espconn_regist_reconcb(pesp_conn, webserver_recon);
    espconn_regist_disconcb(pesp_conn, webserver_discon);
    espconn_regist_sentcb(pesp_conn, webserver_sent);//注册一个发送数据成功的回调函数
}




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

    espconn_accept(&esp_conn);
}

