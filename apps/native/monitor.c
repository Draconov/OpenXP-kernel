#include "oxp.h"
void oxp_main(void){
    if(oxp_open("System Monitor - native"))oxp_exit(1);
    uint64_t last=UINT64_MAX;
    for(;;){
        uint64_t tick=oxp_ticks();
        if(tick/100!=last){
            last=tick/100;
            oxp_draw_cmd_t bg[]={{OXP_DRAW_CLEAR,0,0,0,0,0xe9f2fc},
                {OXP_DRAW_FILL,12,35,296,117,0xffffff},
                {OXP_DRAW_FILL,18,124,284,19,0xd3e6fc}};
            oxp_draw(bg,3);oxp_label(17,11,"OPENXP SYSTEM MONITOR",1,0x1a4d87);
            char uptime[38]="UPTIME SECONDS ";size_t n=15;oxp_u64(uptime,&n,sizeof uptime,last);
            oxp_label(25,53,uptime,1,0x244d78);
            char memory[38]="FREE 4K PAGES ";n=14;oxp_u64(memory,&n,sizeof memory,oxp_call(26,0,0));
            oxp_label(25,73,memory,1,0x244d78);
            oxp_label(25,99,"PROCESS ISOLATED: RING 3",1,0x244d78);
            oxp_label(25,130,"LIVE KERNEL TELEMETRY",1,0x1b5eaa);
            oxp_label(18,165,"READ-ONLY  -  ESC CLOSE",1,0x466380);
        }
        uint32_t event=oxp_wait(25);
        if(event==OXP_CLOSE||event==27)break;
    }
    oxp_exit(0);
}
