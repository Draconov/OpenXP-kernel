#include "gui/shell_bridge.h"
#include "gui/desktop.h"
#include "input/input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t pixels[32*32+2];
int main(void){
    shell_bridge_reset();
    uint64_t scene_epoch=shell_bridge_scene_version();
    static desktop_t mock={0};
    for(unsigned i=0;i<DESKTOP_WINDOWS;++i)mock.order[i]=(int)i;
    mock.windows[DESKTOP_NOTEPAD]=(desktop_window_t){100,100,400,220,true,false,false,0,0,0,0};
    shell_bridge_attach(&mock);
    assert(!shell_bridge_active() && !shell_bridge_has_window_plan());
    oxp_shell_draw_t cmd={.op=OXP_SHELL_RECT,.x=0,.y=0,.w=3,.h=3,.color=0x345678};
    assert(shell_bridge_submit(7,&cmd,1,5));
    assert(shell_bridge_scene_version()>scene_epoch);
    scene_epoch=shell_bridge_scene_version();
    assert(shell_bridge_active() && shell_bridge_take_dirty());
    assert(!shell_bridge_action(OXP_SHELL_THEME,3,0));
    assert(mock.appearance==0);
    assert(shell_bridge_action(OXP_SHELL_THEME,2,0));
    assert(mock.appearance==2 && shell_bridge_take_dirty());
    assert(shell_bridge_action(OXP_SHELL_THEME,0,0));
    assert(mock.appearance==0 && shell_bridge_take_dirty());
    assert(!shell_bridge_take_dirty());
    assert(shell_bridge_submit(7,&cmd,1,6));
    assert(shell_bridge_scene_version()==scene_epoch);
    assert(!shell_bridge_take_dirty()); /* identical heartbeat is not a repaint */
    input_event_t key={.kind=INPUT_KEY,.code=KEY_WINDOWS};
    assert(shell_bridge_route(&key,false,768));
    oxp_shell_event_t event={0};
    assert(shell_bridge_pop(&event));
    assert(event.type==OXP_SHELL_EVENT_KEY && event.code==KEY_WINDOWS);
    assert(!shell_bridge_pop(&event));
    /* Title-bar action buttons must reach trusted desktop hit testing.
       The native window service must never capture these as drags. */
    input_event_t chrome={.kind=INPUT_BUTTON,.left=true,.x=455,.y=114};
    assert(!shell_bridge_route(&chrome,false,768)); /* middle: maximize */
    assert(!shell_bridge_pop(&event));
    chrome.x=429;
    assert(!shell_bridge_route(&chrome,false,768)); /* left: minimize */
    chrome.x=481;
    assert(!shell_bridge_route(&chrome,false,768)); /* right: close */
    /* Resize-corner capture goes to Ring 3, not an app's canvas. */
    input_event_t corner={.kind=INPUT_BUTTON,.left=true,.x=494,.y=315};
    assert(shell_bridge_route(&corner,false,768));
    assert(shell_bridge_pop(&event) && event.type==OXP_SHELL_EVENT_POINTER &&
           event.flags==OXP_SHELL_POINTER_DOWN);
    corner.kind=INPUT_MOVE;corner.x=510;corner.y=325;
    assert(shell_bridge_route(&corner,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_MOVE);
    corner.kind=INPUT_BUTTON;corner.left=false;
    assert(shell_bridge_route(&corner,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_UP);
    /* Full drag life-cycle is handed to Ring 3, including release. */
    input_event_t drag={.kind=INPUT_BUTTON,.left=true,.x=130,.y=115};
    assert(shell_bridge_route(&drag,false,768));
    assert(shell_bridge_pop(&event) && event.type==OXP_SHELL_EVENT_POINTER &&
           event.flags==OXP_SHELL_POINTER_DOWN);
    drag.kind=INPUT_MOVE;drag.x=150;drag.y=140;
    assert(shell_bridge_route(&drag,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_MOVE);
    drag.kind=INPUT_BUTTON;drag.left=false;
    assert(shell_bridge_route(&drag,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_UP);
    /* The shell owns desktop icon activation; application body input passes through. */
    input_event_t icon={.kind=INPUT_BUTTON,.left=true,.x=32,.y=46};
    assert(shell_bridge_route(&icon,false,768));
    assert(shell_bridge_pop(&event) && event.type==OXP_SHELL_EVENT_CLICK);
    icon.x=300;icon.y=300;
    assert(!shell_bridge_route(&icon,false,768));
    input_event_t mouse={.kind=INPUT_BUTTON,.left=true,.x=40,.y=740};
    assert(shell_bridge_route(&mouse,false,768));
    assert(shell_bridge_pop(&event) && event.type==OXP_SHELL_EVENT_CLICK && event.x==40);
    key.code=KEY_CHARACTER;key.key='z';
    assert(!shell_bridge_route(&key,false,768));
    assert(shell_bridge_action(OXP_SHELL_MENU,1,0));
    assert(shell_bridge_menu_open());
    assert(shell_bridge_route(&key,true,768));
    assert(shell_bridge_pop(&event));
    oxp_shell_draw_t invalid=cmd;invalid.w=1025;
    assert(!shell_bridge_submit(7,&invalid,1,6));
    invalid=cmd;invalid.op=OXP_SHELL_TEXT;
    memset(invalid.text,'Q',sizeof(invalid.text));
    assert(!shell_bridge_submit(7,&invalid,1,6));
    fb_surface_t s={(volatile uint8_t *)(pixels+1),32,32,32*4,8,8,8,16,8,0,false,0,0,0,0};
    pixels[0]=0x55aaff;pixels[32*32+1]=0x55aaff;
    shell_bridge_compose(&s);
    assert(pixels[1]==0x345678);
    /* Ring 3 controls window order, never the framebuffer memory itself. */
    oxp_shell_draw_t plan[4]={
        {.op=OXP_SHELL_BG_RECT,.x=0,.y=0,.w=3,.h=3,.color=0x123456},
        {.op=OXP_SHELL_COMPOSE_BEGIN},
        {.op=OXP_SHELL_COMPOSE_WINDOW,.x=DESKTOP_NOTEPAD},
        {.op=OXP_SHELL_COMPOSE_WINDOW,.x=DESKTOP_TERMINAL}
    };
    uint32_t order[DESKTOP_WINDOWS],length=99;
    assert(shell_bridge_submit(7,plan,4,6));
    assert(shell_bridge_window_order(order,DESKTOP_WINDOWS,&length));
    assert(shell_bridge_has_window_plan());
    assert(length==2 && order[0]==DESKTOP_NOTEPAD && order[1]==DESKTOP_TERMINAL);
    assert(!shell_bridge_window_order(order,1,&length));
    plan[3].x=DESKTOP_NOTEPAD; /* Duplicate indices could hide a window. */
    assert(!shell_bridge_submit(7,plan,4,6));
    plan[3].x=DESKTOP_TERMINAL;
    plan[1].op=OXP_SHELL_RECT;plan[1].w=1;plan[1].h=1;
    assert(!shell_bridge_submit(7,plan,4,6)); /* Missing BEGIN. */
    plan[1]=(oxp_shell_draw_t){.op=OXP_SHELL_COMPOSE_BEGIN};
    plan[2].text[0]='A';
    assert(!shell_bridge_submit(7,plan,4,6)); /* Reserved bytes. */
    plan[2].text[0]=0;
    assert(shell_bridge_window_order(order,DESKTOP_WINDOWS,&length));
    assert(length==2); /* Rejected frames never replace the active plan. */
    oxp_shell_draw_t empty={.op=OXP_SHELL_COMPOSE_BEGIN};
    assert(shell_bridge_submit(7,&empty,1,6));
    assert(shell_bridge_window_order(order,DESKTOP_WINDOWS,&length) && !length);
    cmd.op=OXP_SHELL_BG_RECT;cmd.x=4;
    assert(shell_bridge_submit(7,&cmd,1,7));
    shell_bridge_compose_background(&s);
    assert(pixels[5]==0x345678);
    cmd.op=OXP_SHELL_RECT;cmd.x=0;
    assert(shell_bridge_submit(7,&cmd,1,8));
    assert(pixels[1]==0x345678 && pixels[32*32+1]==0x55aaff);
    /* Two complete pixel layers are published atomically by windowd.
     * Incomplete updates must leave the previous scene unchanged. */
    static oxp_shell_raster_row_t row;
    row.layer=3;
    assert(!shell_bridge_raster_row(7,&row));
    row.layer=0;
    assert(!shell_bridge_raster_commit(7,OXP_SHELL_MASK_BACKGROUND));
    for(unsigned layer=0;layer<2;++layer){
        row.layer=layer;
        for(unsigned x=0;x<OXP_SHELL_RASTER_W;++x)
            row.pixels[x]=layer?OXP_SHELL_TRANSPARENT:0xf800u;
        for(unsigned y=0;y<OXP_SHELL_RASTER_H;++y){
            row.y=y;
            if(layer==1 && y==0)row.pixels[1]=0x07e0u;
            assert(shell_bridge_raster_row(7,&row));
            if(layer==1 && y==0)row.pixels[1]=OXP_SHELL_TRANSPARENT;
        }
    }
    assert(shell_bridge_raster_commit(7,OXP_SHELL_MASK_BACKGROUND|OXP_SHELL_MASK_OVERLAY));
    assert(shell_bridge_scene_version()>scene_epoch);
    assert(shell_bridge_raster_ready(0) && shell_bridge_raster_ready(1));
    shell_bridge_compose_background(&s);
    shell_bridge_compose(&s);
    assert(pixels[1]==0xff0000u && pixels[2]==0x00ff00u);
    pixels[1]=0xaabbccu;pixels[1+3+3*32]=0xaabbccu;
    fb_clip_set(&s,3,3,2,1);
    shell_bridge_compose_background(&s);
    fb_clip_clear(&s);
    assert(pixels[1]==0xaabbccu);
    assert(pixels[1+3+3*32]==0xff0000u);

    row.layer=1;row.y=0;
    row.pixels[1]=0x001fu;
    assert(shell_bridge_raster_row(7,&row));
    assert(!shell_bridge_raster_commit(7,OXP_SHELL_MASK_OVERLAY));
    shell_bridge_compose(&s);
    assert(pixels[2]==0x00ff00u); /* no tearing from incomplete back bank */
    for(unsigned y=1;y<OXP_SHELL_RASTER_H;++y){
        row.y=y;row.pixels[1]=OXP_SHELL_TRANSPARENT;
        assert(shell_bridge_raster_row(7,&row));
    }
    assert(shell_bridge_raster_commit(7,OXP_SHELL_MASK_OVERLAY));
    shell_bridge_compose(&s);
    assert(pixels[2]==0x0000ffu);
    /* An uncommitted native layer stays invisible. Full-window pixels come
       from the service; a failed service exposes the old kernel fallback. */
    row.layer=OXP_SHELL_LAYER_NATIVE;
    for(unsigned x=0;x<OXP_SHELL_RASTER_W;++x)row.pixels[x]=OXP_SHELL_TRANSPARENT;
    row.pixels[0]=0xf81fu;
    row.y=0;assert(shell_bridge_raster_row(7,&row));
    assert(!shell_bridge_raster_ready(OXP_SHELL_LAYER_NATIVE));
    assert(!shell_bridge_raster_commit(7,OXP_SHELL_MASK_NATIVE));
    for(unsigned y=1;y<OXP_SHELL_RASTER_H;++y){row.y=y;assert(shell_bridge_raster_row(7,&row));}
    assert(shell_bridge_raster_commit(7,OXP_SHELL_MASK_NATIVE));
    assert(shell_bridge_raster_ready(OXP_SHELL_LAYER_NATIVE));
    shell_bridge_compose_native(&s);
    assert(pixels[1]==0xff00ffu);
    assert(!shell_bridge_raster_commit(999u,OXP_SHELL_MASK_OVERLAY));
    assert(!shell_bridge_raster_commit(7,8u));
    shell_bridge_watchdog(307);
    assert(shell_bridge_active());
    shell_bridge_watchdog(309);
    assert(!shell_bridge_active() && !shell_bridge_has_window_plan() && shell_bridge_take_dirty());
    assert(!shell_bridge_raster_ready(0));
    assert(!shell_bridge_window_order(order,DESKTOP_WINDOWS,&length));
    assert(!shell_bridge_route(&mouse,false,768));
    assert(shell_bridge_submit(9,&cmd,1,400));
    shell_bridge_owner_exit(8);
    assert(shell_bridge_active());
    shell_bridge_owner_exit(9);
    assert(!shell_bridge_active());
    puts("PASS: Ring 3 shell frame validation, input routing, failure fallback, watchdog");
}
