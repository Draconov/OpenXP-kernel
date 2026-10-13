#include "oxp.h"
void oxp_main(void) {
    if(oxp_open("Window IPC Client"))oxp_exit(1);
    if(oxp_fill(0,0,320,180,0x1f3348u))oxp_exit(2);
    if(oxp_label(14,22,"CONNECTING TO WINDOWD...",1,0xffffffu))oxp_exit(3);
    uint64_t begin=oxp_ticks();int64_t channel=-1;
    while(oxp_ticks()-begin<300){
        channel=oxp_connect();
        if(channel>0)break;
        if(channel!=-32)oxp_exit(4);
        (void)oxp_wait(2);
    }
    if(channel<=0)oxp_exit(5);
    static const char ping[]="PING";
    if(oxp_send((uint64_t)channel,ping,4)!=4)oxp_exit(6);
    char response[64]={0};int64_t n=-11;
    begin=oxp_ticks();
    while(oxp_ticks()-begin<300) {
        n=oxp_recv((uint64_t)channel,response,sizeof(response));
        if(n>0)break;
        if(n!=-11)oxp_exit(7);
        if(oxp_ipc_wait((uint64_t)channel,10)<0)oxp_exit(8);
    }
    if(n!=4 || response[0]!='P'||response[1]!='O'||response[2]!='N'||response[3]!='G')oxp_exit(9);
    if(oxp_label(14,56,"CONNECTED: PONG",1,0x71e8c7u))oxp_exit(10);
    (void)oxp_close((uint64_t)channel);
    (void)oxp_wait(15);
    oxp_exit(0);
}
