#include "oxp.h"
/* Independent Ring 3 proof-of-concept service. The Ring 0 compositor and
 * actual Start/taskbar remain unchanged. This demonstrates the IPC boundary. */
void oxp_main(void) {
    if (oxp_listen()!=0)oxp_exit(1);
    if (oxp_open("Window Service (experimental)"))oxp_exit(2);
    if (oxp_fill(0,0,320,180,0x192a43u))oxp_exit(3);
    if (oxp_label(12,16,"RING 3 WINDOW SERVICE",1,0xffffffu))oxp_exit(4);
    if (oxp_label(12,40,"Waiting for IPC client...",1,0x71e8c7u))oxp_exit(5);
    uint64_t begin=oxp_ticks();
    int64_t channel=-1;
    while (oxp_ticks()-begin<1200) {
        channel=oxp_accept();
        if(channel>0)break;
        if(channel!=-11)oxp_exit(6);
        if(oxp_ipc_wait(0,20)<0)oxp_exit(7);
        if(oxp_event()==OXP_CLOSE)oxp_exit(0);
    }
    if(channel<=0)oxp_exit(8);
    char request[64]={0};int64_t n=-11;
    begin=oxp_ticks();
    while(oxp_ticks()-begin<300) {
        n=oxp_recv((uint64_t)channel,request,sizeof(request));
        if(n>0)break;
        if(n!=-11)oxp_exit(9);
        if(oxp_ipc_wait((uint64_t)channel,10)<0)oxp_exit(10);
    }
    if(n!=4 || request[0]!='P' || request[1]!='I' || request[2]!='N' || request[3]!='G')oxp_exit(11);
    static const char reply[]="PONG";
    if(oxp_send((uint64_t)channel,reply,4)!=4)oxp_exit(12);
    (void)oxp_label(12,62,"IPC client connected",1,0xffffffu);
    (void)oxp_label(12,82,"PING -> PONG",1,0x71e8c7u);
    (void)oxp_close((uint64_t)channel);
    /* Give the client a chance to observe the completed response. */
    (void)oxp_wait(15);
    oxp_exit(0);
}
