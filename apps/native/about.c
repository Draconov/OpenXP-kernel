#include "oxp.h"
void oxp_main(void){
    if(oxp_open("About OpenXP - native"))oxp_exit(1);
    oxp_draw_cmd_t shapes[]={
        {OXP_DRAW_CLEAR,0,0,0,0,0xe9f2ff},
        {OXP_DRAW_FILL,12,12,296,45,0x195db4},
        {OXP_DRAW_FILL,12,66,296,89,0xffffff}
    };
    oxp_draw(shapes,3);
    oxp_label(30,27,"OPENXP",2,0xffffff);
    oxp_label(24,78,"OPEN SOURCE OS EXPERIMENT",1,0x214b7c);
    oxp_label(24,99,"NATIVE 64-BIT USER PROGRAM",1,0x214b7c);
    oxp_label(24,120,"RING 3 ISOLATED PROCESS",1,0x214b7c);
    oxp_label(24,143,"NOT WINDOWS COMPATIBLE",1,0x6e5163);
    for(;;){uint32_t e=oxp_wait(0);if(e==OXP_CLOSE||e==27)break;}
    oxp_exit(0);
}
