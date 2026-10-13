#include "oxp.h"
/* Small sample app: its pixels live on a private surface owned by this PID. */
void oxp_main(void) {
    if(oxp_open("Canvas - isolated Ring 3"))oxp_exit(1);
    unsigned bar=48;
    for(;;) {
        oxp_draw_cmd_t commands[]={
            {OXP_DRAW_CLEAR,0,0,0,0,0xeff6ff},
            {OXP_DRAW_FILL,8,8,304,26,0x1550a0},
            {OXP_DRAW_FILL,12,49,bar,26,0x19a45b},
            {OXP_DRAW_FILL,12,99,144,58,0xdceaff},
            {OXP_DRAW_FILL,164,99,144,58,0xffe1d6},
            {OXP_DRAW_FILL,13,100,142,4,0x2768ba},
            {OXP_DRAW_FILL,165,100,142,4,0xcb7544}
        };
        if(oxp_draw(commands,sizeof(commands)/sizeof(commands[0])))break;
        oxp_label(20,17,"OPENXP CANVAS",2,0xffffff);
        oxp_label(17,113,"LEFT CLICK",1,0x103b6b);
        oxp_label(170,113,"DRAG TEST",1,0x8a3424);
        oxp_label(17,147,"ESC TO CLOSE",1,0x103b6b);
        uint32_t event=oxp_wait(0);
        if(!event)continue;
        if(event==OXP_CLOSE || event==27)break;
        if(event==OXP_CLICK){bar=bar<290?bar+16:16;}
        else if(event& OXP_CLICK) {
            /* Click coordinates include title bar: adjust for client origin. */
            unsigned x=oxp_click_x(event),y=oxp_click_y(event);
            if(x>=16 && x<336 && y>=43 && y<223)
                bar=(x-16)%300+1;
        } else if(event=='+' && bar<290)bar+=10;
        else if(event=='-' && bar>10)bar-=10;
    }
    oxp_exit(0);
}
