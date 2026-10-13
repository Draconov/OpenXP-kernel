#include "oxp.h"
void oxp_main(void){
    if(oxp_open("Clock - native ELF64"))oxp_exit(1);
    uint64_t previous=UINT64_MAX;
    for(;;){
        uint64_t seconds=oxp_ticks()/100;
        if(seconds!=previous){
            oxp_draw_cmd_t shapes[]={
                {OXP_DRAW_CLEAR,0,0,0,0,0xeaf3ff},
                {OXP_DRAW_FILL,10,38,300,65,0xffffff},
                {OXP_DRAW_FILL,16,120,288,24,0xb3d5fa},
                {OXP_DRAW_FILL,16,120,16+(unsigned)(seconds%55)*5,24,0x298dba}
            };
            oxp_draw(shapes,4);
            oxp_label(16,14,"SYSTEM UPTIME",2,0x165398);
            char digits[32]={0};size_t n=0;
            oxp_u64(digits,&n,sizeof(digits),seconds);
            oxp_label(30,59,digits,2,0x1a4a7c);
            oxp_label(16,158,"SECONDS SINCE BOOT",1,0x165398);
            previous=seconds;
        }
        /* Periodic deadline for the uptime display; mouse/keyboard wakes
         * the clock before expiry without burning a core. */
        uint32_t event=oxp_wait(10);
        if(event==OXP_CLOSE||event==27)break;
    }
    oxp_exit(0);
}
