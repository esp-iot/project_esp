#include "connect_net.h"



static struct lt_socket UDP = {-1,LTP_SOCKET,0,0,0};
static int64_t UDP_STATE_TIMESTAMP = 0;
const struct lt_socket* lt_ltp_socket(void){
    return &UDP;
}

