#include "oxp.h"
#include "ui_widgets.h"
void oxp_main(void){
    if(oxp_open("Clock - native ELF64"))oxp_exit(1);
    oxp_ui_atlas_init_widgets();
    uint64_t previous=UINT64_MAX;
    for(;;){
        uint64_t seconds=oxp_ticks()/100u;
        if(seconds!=previous){
            unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
            unsigned bar=h>52u?h-42u:32u;
            if(bar>h-8u)bar=h-8u;
            oxp_ui_clear();oxp_ui_appbar(w,"SYSTEM UPTIME");
            if(w>32u&&bar>34u)oxp_ui_sheet((oxp_ui_rect){10u,31u,w-20u,bar-33u});
            char digits[32]={0};size_t n=0;oxp_u64(digits,&n,sizeof digits,seconds);
            oxp_label(30u,bar>66u?50u:34u,digits,bar>66u?2u:1u,oxp_ui_edit_ink());
            oxp_ui_progress((oxp_ui_rect){16u,bar,w>32u?w-32u:1u,16u},
                            (unsigned)(seconds%60u),60u);
            oxp_ui_statusbar(w,h,"SECONDS SINCE BOOT");
            previous=seconds;
        }
        uint32_t event=oxp_wait(10u);
        if(event==OXP_CLOSE||event==27)break;
        if(event==OXP_RESIZE||oxp_ui_refresh_theme())previous=UINT64_MAX;
    }
    oxp_exit(0);
}
