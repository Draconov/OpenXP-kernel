#include "oxp.h"
void oxp_main(void){
    if(oxp_open("Clock - native ELF64"))oxp_exit(1);
    uint64_t previous=UINT64_MAX;
    for(;;){
        uint64_t seconds=oxp_ticks()/100;
        if(seconds!=previous){
            unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
            unsigned bar=h>42u?h-34u:30u;
            if(bar>h-8u)bar=h-8u;
            unsigned main_h=bar>35u?bar-32u:1u;
            oxp_draw_cmd_t shapes[]={
                {OXP_DRAW_CLEAR,0,0,0,0,0xeaf3ff},
                {OXP_DRAW_FILL,10,32,w-20u,main_h,0xffffff},
                {OXP_DRAW_FILL,16,bar,w-32u,16u,0xb3d5fa},
                {OXP_DRAW_FILL,16,bar,1u+(unsigned)(seconds%55)*(w-33u)/54u,16u,0x298dba}
            };
            oxp_draw(shapes,4);
            oxp_label(16,14,"SYSTEM UPTIME",2,0x165398);
            char digits[32]={0};size_t n=0;
            oxp_u64(digits,&n,sizeof(digits),seconds);
            oxp_label(30,bar>66u?50u:34u,digits,bar>66u?2u:1u,0x1a4a7c);
            if(h>140u)oxp_label(16,h-14u,"SECONDS SINCE BOOT",1,0x165398);
            previous=seconds;
        }
        /* Periodic deadline for the uptime display; mouse/keyboard wakes
         * the clock before expiry without burning a core. */
        uint32_t event=oxp_wait(10);
        if(event==OXP_CLOSE||event==27)break;
        if(event==OXP_RESIZE)previous=UINT64_MAX;
    }
    oxp_exit(0);
}
