#include "../apps/native/native_scene.h"
#include "../kernel/gui/native_viewport.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static oxp_shell_window_t views[OXP_NATIVE_SLOTS];
static oxp_native_info_t infos[OXP_NATIVE_SLOTS];
static uint16_t contents[OXP_NATIVE_SLOTS][OXP_NATIVE_W * OXP_NATIVE_H];
static uint16_t line[OXP_SHELL_RASTER_W + 2];
int main(void){
    views[0]=(oxp_shell_window_t){.x=80,.y=30,.width=360,.height=260,.z=1,.visible=1,.active=1};
    views[1]=(oxp_shell_window_t){.x=180,.y=60,.width=360,.height=260,.z=2,.visible=1};
    infos[0].active=infos[1].active=1;
    infos[0].painted=infos[1].painted=1;
    memcpy(infos[0].title,"Calculator",11);
    memcpy(infos[1].title,"Paint",6);
    for(size_t k=0;k<OXP_NATIVE_W*OXP_NATIVE_H;++k){contents[0][k]=0xf800;contents[1][k]=0x07e0;}
    line[0]=0xbeef;line[OXP_SHELL_RASTER_W+1]=0xbeef;
    oxp_native_scene_row(views,infos,contents,120,line+1);
    assert(line[1+80+16]==0xf800); /* uncovered lower red surface */
    assert(line[1+180+16]==0x07e0); /* upper window obscures lower */
    /* Caption buttons are painted by windowd, not the kernel compositor. */
    oxp_native_scene_row(views,infos,contents,40,line+1);
    assert(line[1+80+360-70]!=OXP_SHELL_TRANSPARENT); /* Minimize */
    assert(line[1+80+360-44]!=OXP_SHELL_TRANSPARENT); /* Maximize */
    oxp_native_scene_row(views,infos,contents,74,line+1);
    assert(line[1+180+360-17]!=OXP_SHELL_TRANSPARENT); /* Close */
    views[1].maximized=1;
    oxp_native_scene_row(views,infos,contents,74,line+1);
    assert(line[1+180+360-44]!=OXP_SHELL_TRANSPARENT); /* Restore */
    assert(line[1+5]==OXP_SHELL_TRANSPARENT);
    assert(line[0]==0xbeef && line[OXP_SHELL_RASTER_W+1]==0xbeef);
    views[1].minimized=1;
    oxp_native_scene_row(views,infos,contents,120,line+1);
    assert(line[1+180+16]==0xf800); /* uncover lower surface */
    views[0].minimized=1;
    oxp_native_scene_row(views,infos,contents,120,line+1);
    assert(line[1+196]==OXP_SHELL_TRANSPARENT);
    /* The full content rectangle is now used, with a logical canvas whose
       aspect ratio follows the client instead of fixed 16:9 letterboxes. */
    views[0]=(oxp_shell_window_t){.x=0,.y=0,.width=1024,.height=726,
                                   .z=1,.visible=1,.active=1,.maximized=1};
    oxp_native_viewport_t full=oxp_native_viewport(0,0,1024,726);
    oxp_native_canvas_t logical=oxp_native_canvas(1024,726);
    assert(full.x==16u && full.y==43u && full.w==1002u && full.h==677u);
    assert(logical.w<320u && logical.h==180u);
    assert((uint64_t)logical.w*full.h <= (uint64_t)logical.h*full.w);
    for(unsigned y=0;y<OXP_NATIVE_H;++y)
        for(unsigned x=0;x<OXP_NATIVE_W;++x)
            contents[0][y*OXP_NATIVE_W+x]=x<logical.w/2u?0xf800u:0x001fu;
    oxp_native_scene_row(views,infos,contents,full.y+10u,line+1);
    assert(line[1+full.x+10u]==0xf800u);
    assert(line[1+full.x+full.w-10u]==0x001fu);
    assert(line[1+full.x+full.w/2u-40u]==0xf800u);
    assert(line[1+full.x+full.w/2u+40u]==0x001fu);
    oxp_native_scene_row(views,infos,contents,47u,line+1);
    assert(line[1+full.x+30u]==0xf800u); /* no top letterbox */
    uint32_t cx=0,cy=0;
    oxp_native_pointer_to_canvas(full.x+full.w-1u,full.y+full.h-1u,
                                 1024,726,&cx,&cy);
    assert(cx>=16u && cx<16u+logical.w && cy>=43u && cy<=222u);
    oxp_native_pointer_to_canvas(full.x,full.y,1024,726,&cx,&cy);
    assert(cx==16u && cy==43u);
    oxp_native_pointer_to_canvas(14u,44u,1024,726,&cx,&cy);
    assert(cx==1023u && cy==1023u);
    oxp_native_viewport_t narrow=oxp_native_viewport(10,20,280,200);
    assert(narrow.w==258u && narrow.h==151u && narrow.x==26u && narrow.y==63u);
    oxp_native_canvas_t slim=oxp_native_canvas(280,200);
    assert(slim.w<320u && slim.h==180u);
    oxp_native_canvas_t wide=oxp_native_canvas(640,190);
    assert(wide.w==320u && wide.h<180u);
    oxp_native_pointer_to_canvas(0,0,20,20,&cx,&cy);
    assert(cx==1023u && cy==1023u);
    puts("PASS: Ring 3 native-window chrome, proportional scaling, viewport input and clipping");
}
