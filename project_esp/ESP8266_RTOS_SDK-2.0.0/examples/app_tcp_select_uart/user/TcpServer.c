/******************************************************************************
 * Copyright 2013-2014 
 *
 * FileName:TcpServer.c
 *
 * Description: 
 *
 * Modification history:
 *     2014/12/1, v1.0 create this file.
*******************************************************************************/
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

#include "common.h"

#include "json/cjson.h"
#include "parse_Json_data.h"


#define SERV_PORT 8000
#define LIST 20                //服务器最大接受连接
#define MAX_FD 5              //FD_SET支持描述符数量



xTaskHandle pvTcpServerThreadHandle;


enum {
    APP_EVENT_LED_ON,
    APP_EVENT_LED_OFF,
    APP_EVENT_SEND_DATE
};


int app_cmd_handler_func(char *cmd){
	DEBUG("cmd = %s\n",cmd);

	
	
	if( strncmp( cmd , "turn on led",11) == 0)
	{
		led_on( );
	}
	else if( strncmp( cmd , "turn off led",12) == 0)
	{
		led_off( );
	}
	else if( strncmp( cmd , "Send serial port data",21) == 0)
	{
		//ReadInverter(1,6,8,7);
	}
	else if( strncmp( cmd , "led status",10) == 0)
	{
		//ReadInverter(1,6,8,7);
	}
	else if( strncmp( cmd , "led_stat:",9) == 0)
	{
			DEBUG("cmd  = %s\n",cmd );
			char *p = NULL, tmp[2] = "";
			int led_stat = 0 , i = 0;


			led_stat = StringToInteger(cmd);
		
			led_stat = dec2hex(led_stat); 
			
			

			DEBUG("led_stat = %d\n",led_stat);
			
#if 0
			bit_byte4 Data0;
			Data0.bytes4=0X00;//按字节修改
			Data0.onebit.a0=0X01;//按位修改
			
			Data0.bytes4=0X00;//按字节修改
			Data0.onebit.a0=0;//按位修改
			Data0.onebit.a1=0;//按位修改
			Data0.onebit.a2=0;//按位修改
			 Data0.onebit.a3=1;//按位修改
			 Data0.onebit.a4=0;//按位修改
			Data0.onebit.a5=0;//按位修改
			Data0.onebit.a6=0;//按位修改
			 Data0.onebit.a7=0;//按位修改
			 Data0.onebit.a8=0;//按位修改
			 Data0.onebit.a9=0;//按位修改
			 
			 Data0.onebit.a10=0;//按位修改
			 Data0.onebit.a11=0;//按位修改
			 Data0.onebit.a12=0;//按位修改
			 Data0.onebit.a13=0;//按位修改
			 Data0.onebit.a14=0;//按位修改
			Data0.onebit.a15=0;//按位修改
#endif

			ReadInverter(0x01 , 0x06 ,0x02 , led_stat);
	}
	
	
	else if( strncmp( cmd , "system restore",14) == 0){
			DEBUG("--------system restore\n");
			system_restore();//恢复出厂设置
			system_restart();//重启
	}


}





int StartUp( uint16_t port )
{
		int sockfd;
		int ret = -1;
		struct sockaddr_in serv_addr;   //服务器地址
		socklen_t serv_len;

		//DEBUG("--------%s %d\n",__FUNCTION__,__LINE__);
	//创建TCP套接字
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if(sockfd < 0)
		{
			//DEBUG("fail to socket\n");
			return -1;
		}
		
		// 配置本地地址
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET; 		// ipv4
		serv_addr.sin_port = htons(port);	// 端口， 8080
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // ip
	 
		serv_len = sizeof(serv_addr);

		// 绑定
		ret = bind(sockfd, (struct sockaddr *)&serv_addr, serv_len);
		if(ret < 0)
		{
			//DEBUG("fail to bind\n");
			return -1;
		}
	 
		// 监听
		ret = listen(sockfd, 5);
		if(ret < 0)
		{
			//DEBUG("fail to listen\n");
			return -1;
		}
	
	return(sockfd);	
}

/**
	* @brief  no .	  
	* @note   no.
	* @param  no.
	* @retval no.
	*/
#define SA struct sockaddr

void tcp_server_thread( void *pvParameters )
{
		int sockfd,confd;
	
		sockfd=socket(AF_INET,SOCK_STREAM,0);
		if(sockfd<0)
		{
			perror("fail to socket");
			return;
	
		}
		struct sockaddr_in seraddr;
		seraddr.sin_family=AF_INET;
		seraddr.sin_port=htons(8000);
		seraddr.sin_addr.s_addr = htonl(INADDR_ANY); // ip
	
	
		int ret_bind;
		ret_bind=bind(sockfd,(SA *)&seraddr,sizeof(seraddr));
		if(ret_bind<0)
		{
			perror("fail to bind");
			return;
	
		}
	
	
		listen(sockfd,5);
		//建立一个	backlog大小的  队列（顺序）
		//用来为 accept 服务
		char buf[128];
		int ret_recv;
		int ret_select;
		int maxfd;
		fd_set current, global;
		FD_ZERO(&current);
		FD_ZERO(&global);
	
		FD_SET(0,&global);
		FD_SET(sockfd,&global);
		maxfd=sockfd;
		int i;
	
		while(1)
		{
			current=global;
	
			ret_select=select(maxfd+1,&current,NULL,NULL,NULL);
			if(ret_select<0)
			{
				perror("fail to select");
				return ;
			
			}
	
			if(FD_ISSET(0,&current))
			{
				bzero(buf,sizeof(buf));
				fgets(buf,sizeof(buf),stdin);
				DEBUG("fgets-buf:%s\n",buf);
			
			}
			if(FD_ISSET(sockfd,&current))
			{
				confd=accept(sockfd,NULL,NULL);
				if(confd<0)
				{
					perror("fail to accept");
					return ;
				}
				FD_SET(confd,&global);
				DEBUG("sockfd:%d confd: %d\n",sockfd,confd);
				if(confd>maxfd)
					maxfd=confd;
			}
	
			for(i=1;i<=maxfd;i++)
			{
				bzero(buf,sizeof(buf));
				if(sockfd==i)
					continue;
	
				if(FD_ISSET(i,&current))
				{
					ret_recv=recv(i,buf,sizeof(buf),0);
				
					if(ret_recv<0)
					{
						perror("fail to recv");
						return ;
					
					}
					if(ret_recv==0)
					{
						close(i);
						DEBUG("peer is closed: confd=%d\n",i);
	
						FD_CLR(i,&global);
	
						if(i==maxfd)
						{
							while(maxfd--)
							{
								if(FD_ISSET(maxfd,&global))
								break;
							
							}
		
						}
				
					
					}
	
					DEBUG("recv-buf:%s\n",buf);
					parse_Json_1(buf , i );
	
					//send(i,buf,sizeof(buf),0);
				
				}
	
			
			
			
			}
	
	
		}
	
		
		return ;
}


/**
	* @brief  no .	  
	* @note   no.
	* @param  no.
	* @retval no.
	*/
void tcp_server_thread_init( void )
{

	int ret;
    ret = xTaskCreate(tcp_server_thread,
                      "tcp_server",
                      1500,
                      NULL,
                      9,
                      &pvTcpServerThreadHandle);

    if (ret != pdPASS)
    {
        DEBUG("mqtt create client thread tcp_server_thread failed\n");
    }
#if 0
	pvTcpServerThreadHandle = sys_thread_new("tcp_server" ,  tcp_server_thread , NULL, 2000 , 9 );//第三个参数设置堆栈，太小会出现：(stack_size =xxx,task handle = xxxxx) overflow the heap_size.的错误
	if( pvTcpServerThreadHandle != NULL )
	{
		DEBUG("----tcp_server Created!\r\n"  );
	}
#endif
}

