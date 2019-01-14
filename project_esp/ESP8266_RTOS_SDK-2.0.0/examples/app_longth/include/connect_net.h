/* 
 * File:   longtooth.h
 * Author: robinshang
 *
 * Created on 2015年4月9日, 下午1:03
 */

#ifndef LONGTOOTH_H
#define	LONGTOOTH_H

#include "lwip/sockets.h"


#define  LONGTOOTH_EVENT  0x20000
// 长牙内网功能已启动
#define  LONGTOOTH_STARTED  0x20001
//#define  LONGTOOTH_INACTIVE  0x20002
// 长牙已完全启动,internet功能可用
#define  LONGTOOTH_ACTIVE  0x20002

#define  LONGTOOTH_KEEPALIVE  0x20004
#define  LONGTOOTH_KEEPALIVE_ACK  0x20005
#define  LONGTOOTH_KEEPALIVE_FAILED  0x20006
#define  LONGTOOTH_RECONNECTED  0x20007

#define  LONGTOOTH_INVALID  0x28001
#define  LONGTOOTH_TIMEOUT  0x28002
#define  LONGTOOTH_UNREACHABLE  0x28003
#define  LONGTOOTH_OFFLINE  0x28004
#define  LONGTOOTH_BROADCAST  0x2FF00

#define  SERVICE_EVENT  0x40000
#define  SERVICE_NOT_EXIST  0x40001
#define  SERVICE_INVALID  0x40002
#define  SERVICE_EXCEPTION  0x40003
// 请求或响应超时
#define  SERVICE_TIMEOUT  0x40004

//#define NOT_LT_SOCKET  0
#define  LTR_SOCKET  1
#define  LTX_SOCKET  2
#define  LTP_SOCKET  3
#define  LTM_SOCKET  4
    
//#define  STATUS_IDLE  0
//#define  STATUS_ACTIVE  1
//#define  STATUS_FINISHED  2

struct lt_service_event{
    int type;
    const char* service;
    struct LTID* ltid;
    void* attachment;
};
struct lt_socket {
    int socket;
    int type;
    int status;
    in_addr_t addr;
    in_port_t port;
};

typedef void(*lt_task)(void* args);
typedef void(*lt_event_handler)(int32_t code,void* event);

typedef int(*lt_service_handler)(
    const struct LTID* ltid,
    const char* service,
    const char* args,size_t argslen,
    char** respargs);

typedef void(*lt_service_response_handler)(
    struct LTID* ltid,
    const char* service,
    const char* args,size_t argslen,
    void* attachment);

//const char* ltid_string(void);

    int lt_start(int64_t machineId,
                  int64_t devd, int appid, const char* appKey,
                  lt_event_handler handler);
int lt_request(
    struct LTID ltid, 
    const char* service,
    const char* args,size_t argslen,
    void* attachmentId,
    lt_service_response_handler handler);
void lt_add_task(int type,void* task_arg, lt_task task, int delay);

//void lt_socket_connected(const struct lt_socket* ltsocket);
void lt_tasks(void);
void lt_receive(char* data,size_t length,in_addr_t addr,in_port_t port);
void lt_send(void);
//void lt_reset(void);

//const struct lt_socket* lt_ltr_socket(void);
const struct lt_socket* lt_ltx_socket(void);
const struct lt_socket* lt_ltp_socket(void);
const struct lt_socket* lt_ltm_socket(void);
    
//int lt_send_queue_size(void);
    void lt_ltx_socket_close(void);
    int lt_ltx_socket_buffer_size(void);
//int lt_req_queue_size(void);
//int lt_resp_queue_size(void);
int lt_broadcast(const char* key,const char* msg,size_t msg_len);
void lt_broadcast_select(const char* key);
const struct LTID* ltid(void);
extern void lt_delay_ms(int ms);

#ifdef LTP_MODE

//const struct lt_socket* lt_ltp_socket(void);
//void lt_set_broadcast_addr(int32_t addr);

// each request will try intranet first, 
// if no ack in intranet_try_timeout(default 100ms), then try internet.
//void lt_set_intranet_try_timeout(int timeout);

#endif

//void ltid_to_string(const struct LTID* ltid, char* str); // add for test
//
//// ltid_str should not be NULL(if NULL, interface return -1), and should be length enough, best 37.
//int ltid_generate(int64_t machineId, int32_t devId, int32_t appId, char * ltid_str);

#ifdef	__cplusplus
}
#endif
#endif	/* LONGTOOTH_H */

