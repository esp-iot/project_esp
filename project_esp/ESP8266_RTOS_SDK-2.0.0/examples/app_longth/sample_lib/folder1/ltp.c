void lt_receive(char* data,size_t rev_len,in_addr_t addr,in_port_t port){
    //printf("%s %d\r\n",__FUNCTION__,rev_len);
    MEMCPY(&RDBUF[RDBUF_LEN], data, rev_len);
	//printf("RDBUF_LEN + recv_len : %d\n",RDBUF_LEN + rev_len);
    int offset = 0;
    RDBUF_LEN+=rev_len;
    while (RDBUF_LEN > 19)
    {
        int l = (ltp_bytes_int32(&RDBUF[offset]) & 0xffff) + 2;
        if (l < 1473)
        {
            if(RDBUF_LEN>=l)
            {
                ltp_decode(addr, port, &RDBUF[offset], l);
                offset += l;
                RDBUF_LEN-=l;
                if(RDBUF_LEN<20){ // fix packet lost problem
                    MEMMOVE(RDBUF, &RDBUF[offset], RDBUF_LEN);
                }
            } else {
                MEMMOVE(RDBUF, &RDBUF[offset], RDBUF_LEN);
                break;
            }
        } else {
            RDBUF_LEN = 0;
        }
    }
}



void lt_tasks(){
    int now = lt_datetime();
    ltp_tcp_sock_maintain(now);
    //ltp_udp_sock_maintain(now);
    //ltp_multicast_sock_maintain(now);
    //ltp_peer_maintain(now);
    //ltp_tunnel_maintain(now);
    //ltp_task_maintain(now);
    //lt_send(); by robin
}

