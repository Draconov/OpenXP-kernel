#include "oxp.h"
#include "wm_protocol.h"

static int exchange(uint64_t channel,wm_packet_t *packet){
    if(oxp_send(channel,packet,sizeof(*packet))!=(int64_t)sizeof(*packet))return 1;
    uint32_t op=packet->operation;
    uint64_t begin=oxp_ticks();
    for(;;){
        wm_packet_t response={0};
        int64_t n=oxp_recv(channel,&response,sizeof(response));
        if(n==(int64_t)sizeof(response)){
            if(response.magic!=WM_MAGIC || response.version!=WM_VERSION ||
               response.operation!=(op|WM_REPLY_FLAG) || response.status!=WM_OK)return 2;
            *packet=response;return 0;
        }
        if(n!=-11 || oxp_ticks()-begin>=120u)return 3;
        if(oxp_ipc_wait(channel,8)<0)return 4;
    }
}
void oxp_main(void) {
    if(oxp_open("Window Protocol Client"))oxp_exit(1);
    if(oxp_fill(0,0,320,180,0x1f3348u))oxp_exit(2);
    if(oxp_label(12,18,"RING 3 WM CLIENT",1,0xffffffu))oxp_exit(3);
    uint64_t begin=oxp_ticks();int64_t channel=-32;
    while(oxp_ticks()-begin<300u){
        channel=oxp_connect();if(channel>0)break;
        if(channel!=-32)oxp_exit(4);
        (void)oxp_wait(2);
    }
    if(channel<=0)oxp_exit(5);
    wm_packet_t p={.magic=WM_MAGIC,.version=WM_VERSION,.operation=WM_CREATE,
                    .x=16,.y=32,.width=240,.height=140,
                    .title="Native Client"};
    if(exchange((uint64_t)channel,&p))oxp_exit(6);
    uint32_t id=p.window_id;if(!id)oxp_exit(7);
    /* Keep the stages visible long enough to inspect in VirtualBox while
     * staying below windowd's four-second demonstration deadline. */
    (void)oxp_wait(45);
    (void)oxp_label(12,42,"CREATE OK",1,0x71e8c7u);
    p=(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,.operation=WM_MOVE,
                    .window_id=id,.x=70,.y=96};
    if(exchange((uint64_t)channel,&p) || p.x!=70 || p.y!=96)oxp_exit(8);
    (void)oxp_label(12,62,"MOVE OK",1,0x71e8c7u);
    (void)oxp_wait(45);
    p=(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,.operation=WM_FOCUS,.window_id=id};
    if(exchange((uint64_t)channel,&p))oxp_exit(9);
    (void)oxp_label(12,82,"FOCUS OK",1,0x71e8c7u);
    (void)oxp_wait(45);
    p=(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,.operation=WM_QUERY,.window_id=id};
    if(exchange((uint64_t)channel,&p) || p.x!=70 || p.y!=96 || p.width!=240)oxp_exit(10);
    (void)oxp_label(12,102,"QUERY OK",1,0x71e8c7u);
    (void)oxp_wait(100);
    p=(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,.operation=WM_CLOSE,.window_id=id};
    if(exchange((uint64_t)channel,&p))oxp_exit(11);
    (void)oxp_label(12,122,"CLOSE OK",1,0x71e8c7u);
    (void)oxp_close((uint64_t)channel);
    (void)oxp_wait(15);
    oxp_exit(0);
}
