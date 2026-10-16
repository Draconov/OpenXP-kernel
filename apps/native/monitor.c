#include "oxp.h"
#include "ui_widgets.h"
void oxp_main(void){
    if(oxp_open("System Monitor - native"))oxp_exit(1);
    oxp_ui_atlas_init_widgets();
    uint64_t last=UINT64_MAX;
    for(;;){
        uint64_t tick=oxp_ticks();
        if(tick/100u!=last){
            unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
            last=tick/100u;
            oxp_ui_clear();oxp_ui_appbar(w,"SYSTEM MONITOR");
            if(w>26u&&h>48u)oxp_ui_sheet((oxp_ui_rect){12u,34u,w-24u,h-48u});
            char uptime[38]="UPTIME SECONDS ";size_t n=15u;
            oxp_u64(uptime,&n,sizeof uptime,last);
            oxp_label(25u,53u,uptime,1u,oxp_ui_edit_ink());
            char memory[38]="FREE 4K PAGES ";n=14u;
            uint64_t pages=oxp_call(26,0,0);
            oxp_u64(memory,&n,sizeof memory,pages);
            oxp_label(25u,73u,memory,1u,oxp_ui_edit_ink());
            if(h>113u)oxp_label(25u,99u,"PROCESS ISOLATED: RING 3",1u,oxp_ui_edit_ink());
            if(h>143u)oxp_label(25u,h-44u,"LIVE KERNEL TELEMETRY",1u,oxp_ui_edit_ink());
            oxp_ui_statusbar(w,h,"READ-ONLY - ESC CLOSE");
        }
        uint32_t event=oxp_wait(25u);
        if(event==OXP_CLOSE||event==27)break;
        if(event==OXP_RESIZE||oxp_ui_refresh_theme())last=UINT64_MAX;
    }
    oxp_exit(0);
}
