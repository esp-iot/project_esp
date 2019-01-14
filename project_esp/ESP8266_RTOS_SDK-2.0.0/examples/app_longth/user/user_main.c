/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#include "connect_net.h"





xTaskHandle key_handler_task_handle;


#define server_ip "192.168.101.142"
#define server_port 9669


#define DEVICE_TYPE 		"gh_9e2cff3dfa51" //wechat public number
#define DEVICE_ID 			"122475" //model ID

#define DEFAULT_LAN_PORT 	12476

LOCAL esp_udp ssdp_udp;
LOCAL struct espconn pssdpudpconn;
LOCAL os_timer_t ssdp_time_serv;

uint8  lan_buf[200];
uint16 lan_buf_len;
uint8  udp_sent_cnt = 0;



static os_timer_t ip_timer;
static os_timer_t bob_timer;
static os_timer_t bob1_timer;


static fd_set WRFDS;
static fd_set RDFDS;


const airkiss_config_t akconf =
{
	(airkiss_memset_fn)&memset,
	(airkiss_memcpy_fn)&memcpy,
	(airkiss_memcmp_fn)&memcmp,
	0,
};

LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_time_callback(void)
{
	uint16 i;
	airkiss_lan_ret_t ret;
	
	if ((udp_sent_cnt++) >30) {
		udp_sent_cnt = 0;
		os_timer_disarm(&ssdp_time_serv);//s
		//return;
	}

	ssdp_udp.remote_port = DEFAULT_LAN_PORT;
	ssdp_udp.remote_ip[0] = 255;
	ssdp_udp.remote_ip[1] = 255;
	ssdp_udp.remote_ip[2] = 255;
	ssdp_udp.remote_ip[3] = 255;
	lan_buf_len = sizeof(lan_buf);
	ret = airkiss_lan_pack(AIRKISS_LAN_SSDP_NOTIFY_CMD,
		DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);
	if (ret != AIRKISS_LAN_PAKE_READY) {
		os_printf("Pack lan packet error!");
		return;
	}
	
	ret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
	if (ret != 0) {
		os_printf("UDP send error!");
	}
	os_printf("Finish send notify!\n");
}

LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_recv_callbk(void *arg, char *pdata, unsigned short len)
{
	uint16 i;
	remot_info* pcon_info = NULL;
		
	airkiss_lan_ret_t ret = airkiss_lan_recv(pdata, len, &akconf);
	airkiss_lan_ret_t packret;
	
	switch (ret){
	case AIRKISS_LAN_SSDP_REQ:
		espconn_get_connection_info(&pssdpudpconn, &pcon_info, 0);
		os_printf("remote ip: %d.%d.%d.%d \r\n",pcon_info->remote_ip[0],pcon_info->remote_ip[1],
			                                    pcon_info->remote_ip[2],pcon_info->remote_ip[3]);
		os_printf("remote port: %d \r\n",pcon_info->remote_port);
      
        pssdpudpconn.proto.udp->remote_port = pcon_info->remote_port;
		memcpy(pssdpudpconn.proto.udp->remote_ip,pcon_info->remote_ip,4);
		ssdp_udp.remote_port = DEFAULT_LAN_PORT;
		
		lan_buf_len = sizeof(lan_buf);
		packret = airkiss_lan_pack(AIRKISS_LAN_SSDP_RESP_CMD,
			DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);
		
		if (packret != AIRKISS_LAN_PAKE_READY) {
			os_printf("Pack lan packet error!");
			return;
		}

		os_printf("\r\n\r\n");
		for (i=0; i<lan_buf_len; i++)
			os_printf("%c",lan_buf[i]);
		os_printf("\r\n\r\n");
		
		packret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
		if (packret != 0) {
			os_printf("LAN UDP Send err!");
		}
		
		break;
	default:
		os_printf("Pack is not ssdq req!%d\r\n",ret);
		break;
	}
}

void ICACHE_FLASH_ATTR
airkiss_start_discover(void)
{
	ssdp_udp.local_port = DEFAULT_LAN_PORT;
	pssdpudpconn.type = ESPCONN_UDP;
	pssdpudpconn.proto.udp = &(ssdp_udp);
	espconn_regist_recvcb(&pssdpudpconn, airkiss_wifilan_recv_callbk);
	espconn_create(&pssdpudpconn);

	os_timer_disarm(&ssdp_time_serv);
	os_timer_setfn(&ssdp_time_serv, (os_timer_func_t *)airkiss_wifilan_time_callback, NULL);
	os_timer_arm(&ssdp_time_serv, 1000, 1);//1s
}


void ICACHE_FLASH_ATTR
smartconfig_done(sc_status status, void *pdata)
{
    switch(status) {
        case SC_STATUS_WAIT:
            printf("SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            printf("SC_STATUS_FIND_CHANNEL\n");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            printf("SC_STATUS_GETTING_SSID_PSWD\n");
            sc_type *type = pdata;
            if (*type == SC_TYPE_ESPTOUCH) {
                printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
            } else {
                printf("SC_TYPE:SC_TYPE_AIRKISS\n");
            }
            break;
        case SC_STATUS_LINK:
            printf("SC_STATUS_LINK\n");
            struct station_config *sta_conf = pdata;
	
	        wifi_station_set_config(sta_conf);
	        wifi_station_disconnect();
	        wifi_station_connect();
            break;
        case SC_STATUS_LINK_OVER:
            printf("SC_STATUS_LINK_OVER\n");
            if (pdata != NULL) {
				//SC_TYPE_ESPTOUCH
                uint8 phone_ip[4] = {0};

                memcpy(phone_ip, (uint8*)pdata, 4);
                printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
            } else {
            	//SC_TYPE_AIRKISS - support airkiss v2.0
				airkiss_start_discover();
			}
            smartconfig_stop();
            break;
    }
	
}

void ICACHE_FLASH_ATTR
smartconfig_task(void *pvParameters)
{
    smartconfig_start(smartconfig_done);
    
    vTaskDelete(NULL);
}

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}


/********************************led*******************************************/


void led_toggle_task( void *pvParameters )
{
	led_init( );
	for( ;; )
	{
		led_toggle( );
		//printf("led toggle!\r\n");
		vTaskDelay( 500 / portTICK_RATE_MS );
	}
	
	vTaskDelete(NULL);
}


/********************************key*******************************************/

void key_int_handler( void )
{
	uint32 gpio_status;

	gpio_status = GPIO_REG_READ( GPIO_STATUS_ADDRESS );

	GPIO_REG_WRITE( GPIO_STATUS_W1TC_ADDRESS , gpio_status );	//clear interrupt status

	if( gpio_status & (BIT(0)) )
	{
		xTaskResumeFromISR( key_handler_task_handle );	// 用于恢复一个挂起的任务，用在ISR中。参数是需要恢复的任务的句柄
	}
}

void key_init( void )
{
	GPIO_ConfigTypeDef	GPIOConfig;
	
	GPIOConfig.GPIO_Pin = GPIO_Pin_0;
	GPIOConfig.GPIO_Mode = GPIO_Mode_Input;
	GPIOConfig.GPIO_Pullup = GPIO_PullUp_DIS;
	GPIOConfig.GPIO_IntrType = GPIO_PIN_INTR_NEGEDGE;
	gpio_config( &GPIOConfig );
	
	gpio_intr_handler_register( key_int_handler , NULL );//注册中断函数

	_xt_isr_unmask(1 << 4);
}


void key_handler_task( void *pvParameters )
{
    uint32_t TickCountPre = 0 , TickCountCur = 0;

	key_init(); 
	
	for( ;; )
	{
		vTaskSuspend( NULL );	//函数被挂起，当只有当有中断触发时才继续执行

		TickCountPre = TickCountCur;
			
		TickCountCur = xTaskGetTickCount( );//用于获取系统当前运行的时钟节拍数。
		if( TickCountCur - TickCountPre > 7 )//消抖
		{
			uint8_t i;

			for( i = 0; i < 10 ; i ++ )
			{
				vTaskDelay( 500 / portTICK_RATE_MS );

				uint32_t gpio_value;

				gpio_value = gpio_input_get( );
				if( ( gpio_value & BIT(0) ) == BIT(0) )
				{
					break;
				}
			}

			if( i == 10 )
			{
				smartconfig_start( smartconfig_done );
			}
		}
			
	}
}


void selected(const struct lt_socket* ltsocket)
	{
		//printf("selected : (type, socket) = (%d, %d)\r\n",ltsocket->type,ltsocket->socket);
		ssize_t c = 0;
		int type = ltsocket->type;
		int sock = ltsocket->socket;
		if (sock != -1)
		{
			if (FD_ISSET(sock, &RDFDS))
			{
				if (type == LTP_SOCKET || type == LTM_SOCKET)
				{
					struct sockaddr_in addr;
					socklen_t addr_size = sizeof(addr);
					c = recvfrom(sock, recvbuf, recvbuflen, 0, (struct sockaddr *) & addr, &addr_size);
					//printf("recvfrom size:%d ip:%s sock:%d\r\n",c,inet_ntoa(addr.sin_addr),sock);
					if (c>0) {
						lt_receive(recvbuf, c, addr.sin_addr.s_addr, addr.sin_port);
					}
				}
				else {
					c = recv(sock, recvbuf, recvbuflen, 0);
					if (c > 0) {
						lt_receive(recvbuf, c, 0, 0);
					}
					else {
						lt_ltx_socket_close();
						return;
					}
				}
			}
	
			if (FD_ISSET(sock, &WRFDS))
			{
				if (type == LTR_SOCKET || type == LTX_SOCKET) {
					lt_send();
				}
			}
	
		}
		//lt_send(); // by robin
	}




void  main_task_test(void *pvParameters){

	printf("......%s ........%s\n", __FUNCTION__, __TIME__);

	int64_t time;
	uint8_t keyv_old = 0;
	int _started = 1;

	STATION_STATUS StaStatus;
	do
	{
		StaStatus = wifi_station_get_connect_status( );

		vTaskDelay( 1000 / portTICK_RATE_MS );
	}
	while( StaStatus != STATION_GOT_IP );//当wifi连接成功时继续
	
	while (_started == 1)
	{
		int maxfd = 0;
		FD_ZERO(&RDFDS);
		FD_ZERO(&WRFDS);
		const struct lt_socket* ltx = lt_ltx_socket();
		const struct lt_socket* ltp = lt_ltp_socket();
		const struct lt_socket* ltm = lt_ltm_socket();

		
		if (ltx->socket != -1)
		{
			printf("......%s ........%s SET FD\n", __FUNCTION__, __TIME__);
			maxfd = ltx->socket;
			FD_SET(ltx->socket, &RDFDS);
			//            if(lt_send_queue_size()>0){
			if (lt_ltx_socket_buffer_size()>0) {
				FD_SET(ltx->socket, &WRFDS);	//将写权限文件描述符加入集合
			}

		}

		//监视内部网和外部网套接字
		if (ltp->socket != -1)
		{
			printf("......%s ........%s SET FD\n", __FUNCTION__, __TIME__);
			FD_SET(ltp->socket, &RDFDS);	//将读权限文件描述符加入集合
			if (maxfd<ltp->socket) {
				maxfd = ltp->socket;
			}
		}

		if (ltm->socket != -1)
		{
			printf("......%s ........%s SET FD\n", __FUNCTION__, __TIME__);
			FD_SET(ltm->socket, &RDFDS);	//将文件描述符加入集合
			if (maxfd<ltm->socket) {
				maxfd = ltm->socket;
			}
		}

		maxfd++;
		struct timeval t;
		t.tv_sec = 1;		//秒数
		t.tv_usec = 10000;	//毫秒数

		printf("......%s ........%s maxfd=%d\n", __FUNCTION__, __TIME__,maxfd);

		/*int maxfd是一个整数值，是指集合中所有文件描述符的范围，即所有文件描述符的最大值加1，不能错

		

		
		struct fd_set可以理解为一个集合，这个集合中存放的是文件描述符(file descriptor)，即文件句柄，
			这可以是我们所说的普通意义的文件，当然Unix下任何设备、管道、FIFO等都是文件形式，全部包括在内，
			所以，毫无疑问，一个socket就是一个文件，socket句柄就是一个文件描述符。fd_set集合可以通过一些宏由人为来操作，
			比如清空集合：FD_ZERO(fd_set*)，
			将一个给定的文件描述符加入集合之中:FD_SET(int, fd_set*)，
			将一个给定的文件描述符从集合中删除:FD_CLR(int,   fd_set*)，
			检查集合中指定的文件描述符是否可以读写:FD_ISSET(int, fd_set*)

		fd_set* readfds是指向fd_set结构的指针，这个集合中应该包括文件描述符，我们是要监视这些文件描述符的读变化的，
			即我们关心是否可以从这些文件中读取数据了，如果这个集合中有一个文件可读，select就会返回一个大于0的值，
			表示有文件可读，如果没有可读的文件，则根据timeout参数再判断是否超时，若超出timeout的时间，select返回0，
			若发生错误返回负值。可以传入NULL值，表示不关心任何文件的读变化。


		fd_set* writefds是指向fd_set结构的指针，这个集合中应该包括文件描述符，我们是要监视这些文件描述符的写变化的，
			即我们关心是否可以向这些文件中写入数据了，如果这个集合中有一个文件可写，select就会返回一个大于0的值，表示有文件可写，
			如果没有可写的文件，则根据timeout再判断是否超时，若超出timeout的时间，select返回0，若发生错误返回负值。
			可以传入NULL值，表示不关心任何文件的写变化。

		fe_set* errorfds同上面两个参数的意图，用来监视文件错误异常。
			
		struct timeval:是一个大家常用的结构，用来代表时间值，有两个成员，一个是秒数，另一个毫秒数。
		struct timeval* timeout是select的超时时间，这个参数至关重要，它可以使select处于三种状态。

		    第一：若将NULL以形参传入，即不传入时间结构，就是将select置于阻塞状态，一定等到监视文件描述符集合中某个文件描述符发生变化为止；
		    第二：若将时间值设为0秒0毫秒，就变成一个纯粹的非阻塞函数，不管文件描述符是否有变化，都立刻返回继续执行，文件无变化返回0，有变化返回一个正值；
		    第三：timeout的值大于0，这就是等待的超时时间，即select在timeout时间内阻塞，超时时间之内有事件到来就返回了，否则在超时后不管怎样一定返回，返回值同上述。

		return：
			负值：select错误

		    正值：某些文件可读写或出错

		    0：等待超时，没有可读写或错误的文件

		*/
		
		int count = select(maxfd, &RDFDS, &WRFDS, NULL, &t);
		if (count > 0) // number of prepared fd
		{
			printf("--------count = %d\n",count);
			selected(ltx);
			selected(ltp);
			selected(ltm);
		}

		
		//LT队列的任务(请求响应ltpeer队列和任务队列)
		lt_tasks();

	}



}

void Led_Cmd(bool status) {
	if (status == true) {
		GPIO_OUTPUT_SET(GPIO_ID_PIN(13), 1);
	}
	else {
		GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 0);
	}
}



void Led_Cmd1(bool status) {
	if (status == true) {
		GPIO_OUTPUT_SET(GPIO_ID_PIN(13), 1);
	}
	else {
		GPIO_OUTPUT_SET(GPIO_ID_PIN(0), 0);
	}
}
void hw_bob1_timer_cb(void) {
	static bool status = false;

	if (status == true) {
		status = false;
		os_printf("Led_Cmd false");
	}
	else {
		status = true;
		os_printf("Led_Cmd true");
	}
	Led_Cmd1(status);

}



void hw_test_timer_cb(void) {
	static bool status = false;

	if (status == true) {
		status = false;
		os_printf("Led_Cmd false");
	}
	else {
		status = true;
		os_printf("Led_Cmd true");
	}
	Led_Cmd(status);

}



/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_init(void)
{
	user_uart_init();//串口初始化
	printf("SDK version:%s\n", system_get_sdk_version());
	
	//设置定时时间
	Led_Cmd(false);
		
			//设置定时器
	//参数一：要设置的定时器；参数二：定时器到时间要执行的回调函数；参数三：回调函数的参数
	os_timer_setfn(&ip_timer, (os_timer_func_t *)hw_test_timer_cb, NULL);

	os_timer_setfn(&bob1_timer, (os_timer_func_t *)hw_bob1_timer_cb, NULL);

	uint8 statue = 0;
	vTaskDelay(300 / portTICK_RATE_MS);//delay 300ms
	printf("application start......\n");
	


#if 0	
	//............gpio.....................
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12); //R  
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13); //G  
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14); //B  
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5); //R 
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO5);
	gpio16_output_conf();

	GPIO_OUTPUT_SET(12, 0);
	GPIO_OUTPUT_SET(0, 0);
	GPIO_OUTPUT_SET(13, 0);
	GPIO_OUTPUT_SET(14, 0);
	//GPIO_OUTPUT_SET(15, 0);
	GPIO_OUTPUT_SET(5, 0);
	gpio16_output_set(0);




	printf("^_^!Miracle-->start to init i2c.....................................................\n");

	uint8_t deviceAddr = 0x0;
	MCP23017_Self mcpSelf;

	i2c_master_gpio_init(); // uses the pins defined by I2C_MASTER_SDA_GPIO & I2C_MASTER_SCL_GPIO
	i2c_master_init();
	printf("^_^!Miracle-->i2c_master_gpio_init: I2C_MASTER_SDA_GPIO=GPIO%d, I2C_MASTER_SCL_GPIO=GPIO%d, I2C_MASTER_HALF_CYCLE=%d\n", I2C_MASTER_SDA_GPIO, I2C_MASTER_SCL_GPIO, I2C_MASTER_HALF_CYCLE);
	mcp23017_init(&mcpSelf);

	mcp23017_initKeyPadMode(&mcpSelf, deviceAddr);

	uint8_t keyv = 0;
	mcp23017_KeyPadMatrixScan(&mcpSelf, deviceAddr, &keyv);
	printf("^_^!Miracle-->KeyPad init value is :%d\n", keyv);

	printf("^_^!Miracle-->stop to init i2c.....................................................\n");

#endif
	

	wifi_set_opmode(STATION_MODE);

	//按键按下5s开始做smartconfig
	xTaskCreate(key_handler_task, "key_handler_task", 256, NULL, 3, &key_handler_task_handle );
	

	xTaskCreate(main_task_test, "main_task_test", 8096, NULL, 4, NULL);


}

