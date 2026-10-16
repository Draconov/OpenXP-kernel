#include "oxp.h"
#include "ui_widgets.h"
void oxp_main(void){
    if(oxp_open("About OpenXP - native"))oxp_exit(1);
    oxp_ui_atlas_init_widgets();
    for(;;){
        unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
        oxp_ui_clear();oxp_ui_appbar(w,"ABOUT OPENXP");
        if(w>26u&&h>75u){
            oxp_ui_panel((oxp_ui_rect){12u,32u,w-24u,43u});
            oxp_label(28u,44u,"OPENXP",2u,oxp_ui_ink());
        }
        if(w>26u&&h>85u){
            oxp_ui_sheet((oxp_ui_rect){12u,80u,w-24u,h-91u});
            oxp_label(23u,90u,"OPEN SOURCE OS EXPERIMENT",1u,oxp_ui_edit_ink());
            if(h>116u)oxp_label(23u,111u,"NATIVE 64-BIT USER PROGRAM",1u,oxp_ui_edit_ink());
            if(h>142u)oxp_label(23u,132u,"RING 3 ISOLATED PROCESS",1u,oxp_ui_edit_ink());
            if(h>166u)oxp_label(23u,153u,"NOT WINDOWS COMPATIBLE",1u,oxp_ui_edit_ink());
        }
        oxp_ui_statusbar(w,h,"AURORA APPEARANCE ENGINE");
        uint32_t e=oxp_ui_wait_theme();
        if(e==OXP_CLOSE||e==27)break;
    }
    oxp_exit(0);
}
