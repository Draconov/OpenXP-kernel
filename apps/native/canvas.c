#include "oxp.h"
/* Small sample app: its pixels live on a private surface owned by this PID. */
void oxp_main(void) {
    if(oxp_open("Canvas - isolated Ring 3"))oxp_exit(1);
    unsigned bar=48;
    for(;;) {
        unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
        unsigned half=(w-24u)/2u;
        unsigned by=h>=120u? h-72u : 48u;
        unsigned bh=h>by+12u?h-by-12u:1u;
        unsigned maxbar=w>24u?w-24u:1u;
        if(bar>maxbar)bar=maxbar;
        oxp_draw_cmd_t commands[]={
            {OXP_DRAW_CLEAR,0,0,0,0,0xeff6ff},
            {OXP_DRAW_FILL,8,8,w-16u,26,0x1550a0},
            {OXP_DRAW_FILL,12,42,bar,16,0x19a45b},
            {OXP_DRAW_FILL,12,by,half-4u,bh,0xdceaff},
            {OXP_DRAW_FILL,16+half,by,half-4u,bh,0xffe1d6},
            {OXP_DRAW_FILL,13,by+1u,half-6u,4,0x2768ba},
            {OXP_DRAW_FILL,17+half,by+1u,half-6u,4,0xcb7544}
        };
        if(oxp_draw(commands,sizeof(commands)/sizeof(commands[0])))break;
        oxp_label(20,17,"OPENXP CANVAS",2,0xffffff);
        oxp_label(17,by+13u,"LEFT CLICK",1,0x103b6b);
        oxp_label(20+half,by+13u,"DRAG TEST",1,0x8a3424);
        if(h>150u)oxp_label(17,h-17u,"ESC TO CLOSE",1,0x103b6b);
        uint32_t event=oxp_wait(0);
        if(!event)continue;
        if(event==OXP_CLOSE || event==27)break;
        if(event==OXP_CLICK){bar=bar<290?bar+16:16;}
        else if(event& OXP_CLICK) {
            /* Click coordinates include title bar: adjust for client origin. */
            unsigned x=oxp_click_x(event),y=oxp_click_y(event);
            if(x>=16 && x<16+oxp_canvas_width() && y>=43 && y<43+oxp_canvas_height())
                bar=(x-16)%(oxp_canvas_width()-24u)+1u;
        } else if(event=='+' && bar<290)bar+=10;
        else if(event=='-' && bar>10)bar-=10;
    }
    oxp_exit(0);
}
