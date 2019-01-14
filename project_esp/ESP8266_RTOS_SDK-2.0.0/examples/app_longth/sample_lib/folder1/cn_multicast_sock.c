#include "connect_net.h"



static struct lt_socket MULTICAST = {-1,LTM_SOCKET,0,0,0};
static int64_t MULTCAST_STATE_TIMESTAMP = 0 ;
//static int INTERVAL = 1000;
const struct lt_socket* lt_ltm_socket(){
    return &MULTICAST;
}

