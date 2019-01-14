#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include "lwip/err.h"
#include "lwip/arch.h"
#include <lwip/inet.h>
#include <lwip/sys.h>
#include <string.h>
//#include <errno.h>
#include <fcntl.h>

#include "esp_common.h"
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"
#include "espressif/espconn.h"
#include "espressif/airkiss.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "connect_net.h"
#include <errno.h>
#include <string.h>



// get system time with ms level
uint32 lt_datetime() {
	uint32 time = xTaskGetTickCount();
	int64_t time_ms = (int64_t)time * 10;
	return  time_ms;
}


int32_t lt_host_ip(const char* host) {
	int ip = 0;
	printf("gethostbyname started\r\n");
	struct hostent *h = gethostbyname(host); // lite version must register this LT server
	printf("gethostbyname ended\r\n");
	if (h == NULL) {
		return 0;
	}
	else {
		memcpy(&ip, &(h->h_addr_list[0])[0], 4);
	}
	return ip;
}

int32_t lt_get_register_host() {
	return lt_host_ip("118.178.233.149");
}

int set_socket_noblock(int sock)
{
	//	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
	int 	block = 1;
	return ioctlsocket(sock, FIONBIO, &block);
}

void lt_socket_close(int socket) {
	shutdown(socket, SHUT_RDWR);
	if (socket > -1) {
		close(socket);
	}
}


// set up socket connection
int lt_socket_connect(const struct lt_socket* ltsocket)
{
	struct sockaddr_in addr;
	int so_broadcast = 1;
	int sock = -1;
	int flags, ret;
	int ret_set;
	int type = ltsocket->type;
	in_addr_t ip = ltsocket->addr;
	in_port_t port = ltsocket->port;

	if (ltsocket->socket>-1) {
		lt_socket_close(ltsocket->socket);
	}


	if (type == LTP_SOCKET) // intranet
	{
		printf("---------------LTP_SOCKET------------------\r\n");

	}


	else if (type == LTM_SOCKET)
	{
		printf("---------------LTP_SOCKET------------------\r\n");

	}


	else // internet
	{
		printf("----TCP start--------type =%d\r\n",type);
		if (type == LTR_SOCKET) {// register
			port = htons(53199);
		}

		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = ip;
		addr.sin_port = (port);
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		printf("----------TCP start--------\r\n");

		if ( bind( sock, (struct sockaddr *)&addr, sizeof(addr)) < 0 )
		{
			printf("----------bind  error--------\r\n");
			return -1;
		}
	
		if ( port == 0)  /* if dynamically allocating a port */
		{
			printf("----------port  error--------\r\n");
			return -1;
		}
		if ( listen(sock_fd, 5) < 0 )
		{
			printf("----------listen  error--------\r\n");
			return -1;
		}

		if (type == LTR_SOCKET)
			printf("socket create ... LTR_SOCKET socket : %d\n", sock);
		else
			printf("socket create ... LTX_SOCKET socket : %d\n", sock);

		if (sock > -1)
		{
			//set socket noblock
			ret = set_socket_noblock(sock);
			printf("lt_socket_connect : set_socket_noblock ret = %d\r\n", ret);
			ret = connect(sock, (struct sockaddr*) &addr, sizeof(struct sockaddr));
			//printf("connect ... LTR_SOCKET,type=%d ret = %d -- (%d, %d/%d)  errno=%d,EINPROGRESS:%d\n\r", type, ret, addr.sin_addr.s_addr, addr.sin_port, ntohs(addr.sin_port), errno, EINPROGRESS);
			if (ret < 0)
			{
				
			}
			else {//connect success
				  
				
				return sock;
			}

			close(sock);
			sock = -1;
		}

	}

	if (sock >= 0) {
		ret = set_socket_noblock(sock);
		printf("lt_socket_connect : set_socket_noblock ret = %d\r\n", ret);
	}
	//printf("%s  -------- return sock = %d, type = %d, errno = %d\n\r", __FUNCTION__, sock, type, errno);
	return sock;
}





