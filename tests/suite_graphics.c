/* OpenXP host test suite: graphics.
 * Select exactly one original test per compilation using -DOPENXP_TEST_NAME.
 * Individual test binaries and sanitizers remain unchanged. */
#if defined(OPENXP_TEST_FB)
#include "video/fb.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint32_t area[16*16+4];
int main(void){
    for(int i=0;i<260;++i)area[i]=0x76543210;
    fb_surface_t s={ (volatile uint8_t*)(area+2), 16,16,16*4,8,8,8,16,8,0,false,0,0,0,0 };
    fb_rect(&s,-8,-8,12,12,0x112233);
    assert(area[2]==0x112233 && area[2+3+3*16]==0x112233 && area[2+4]==0x76543210);
    fb_rect(&s,15,15,2147483647,2147483647,0xabcdef);
    assert(area[2+15+15*16]==0xabcdef);
    fb_text(&s,0,0,"Start!",1,0xff0000);
    assert(area[0]==0x76543210 && area[1]==0x76543210 && area[258]==0x76543210 && area[259]==0x76543210);
    assert(fb_rgb(&s,1,2,3)==0x010203);
    uint32_t outside=area[2+1+1*16];
    fb_clip_set(&s,6,6,3,3);
    fb_rect(&s,0,0,16,16,0x445566);
    assert(area[2+1+1*16]==outside);
    assert(area[2+6+6*16]==0x445566);
    assert(area[2+9+9*16]!=0x445566);
    uint16_t row[]={0xf800,0x07e0,0x001f,0xffff};
    fb_blit565_row(&s,6,7,row,4);
    assert(area[2+6+7*16]==0xff0000);
    assert(area[2+7+7*16]==0x00ff00);
    assert(area[2+8+7*16]==0x0000ff);
    assert(area[2+9+7*16]!=0xffffff); /* clipped right edge */
    fb_clip_clear(&s);
    fb_blit565_row(&s,-2,0,row,4);
    assert(area[2+0]==0x0000ff && area[2+1]==0xffffff);
    fb_rect(&s,0,0,1,1,0x778899);
    assert(area[2]==0x778899);
    /* Differential scanout: static frames write zero device pixels; changing
     * one interior pixel writes exactly one; clipping and pitch are safe. */
    uint32_t desired[16*16];
    for(int y=0;y<16;++y)for(int x=0;x<16;++x)
        desired[y*16+x]=area[2+y*16+x];
    assert(fb_present_changed(&s,desired,16,0,0,16,16)==0);
    desired[5*16+6]^=0x00010000u;
    assert(fb_present_changed(&s,desired,16,0,0,16,16)==1);
    assert(area[2+5*16+6]==desired[5*16+6]);
    desired[8*16+8]^=0x100u;
    assert(fb_present_changed(&s,desired,16,-2147483647,8,2147483647,1)==0);
    assert(fb_present_changed(&s,desired,16,8,8,1,1)==1);
    assert(fb_present_changed(&s,desired,15,0,0,16,16)==0);
    assert(fb_present_changed(&s,desired,16,0,0,16,16)==0);
    assert(area[0]==0x76543210 && area[1]==0x76543210 &&
           area[258]==0x76543210 && area[259]==0x76543210);
    puts("PASS: framebuffer clipping, differential presentation, RGB and sentinels");
}
#elif defined(OPENXP_TEST_FONT_ENGINE)
#include "../apps/native/font_engine.h"
#include "../apps/native/shell_raster.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t data[OXP_FONT_BYTES];
static uint16_t line[OXP_SHELL_RASTER_W+2];
int main(int argc,char **argv){
    assert(argc==2);
    FILE *fp=fopen(argv[1],"rb");assert(fp);
    assert(fread(data,1,sizeof data,fp)==sizeof data);
    assert(fgetc(fp)==EOF);fclose(fp);
    oxp_font_t f={0};
    assert(!oxp_font_parse(&f,data,0));
    assert(!oxp_font_parse(&f,data,sizeof data-1u));
    assert(oxp_font_parse(&f,data,sizeof data));
    assert(f.advance && f.alpha);
    assert(oxp_font_width(&f,"Hello 123!",0)>0);
    assert(oxp_font_width(&f,"iiii",0)<oxp_font_width(&f,"WWWW",0));
    assert(oxp_font_advance(&f,' ',0)==4u);
    assert(oxp_font_advance(&f,0xff,0)==oxp_font_advance(&f,'?',0));
    assert(oxp_font_alpha(&f,'A',0,0,OXP_FONT_CH)==0);
    unsigned nonzero=0;
    for(unsigned y=0;y<OXP_FONT_CH;++y)for(unsigned x=0;x<OXP_FONT_CW;++x)
        if(oxp_font_alpha(&f,'A',0,x,y))++nonzero;
    assert(nonzero>5u);
    /* Text compositor shares the exact font with native GUI bitmap labels. */
    oxp_raster_set_font(&f);
    assert(oxp_raster_text_width("iiii",1)<oxp_raster_text_width("WWWW",1));
    oxp_shell_draw_t cmd={.op=OXP_SHELL_TEXT,.x=12,.y=7,.w=1,.color=0xffffff,.text="Aa"};
    unsigned lit=0;
    for(unsigned y=7;y<7u+OXP_FONT_CH;++y){
        line[0]=0xaaaa;line[OXP_SHELL_RASTER_W+1]=0xbbbb;
        oxp_raster_row(&cmd,1,1,y,line+1);
        assert(line[0]==0xaaaa && line[OXP_SHELL_RASTER_W+1]==0xbbbb);
        for(unsigned x=0;x<OXP_SHELL_RASTER_W;++x)
            if(line[x+1]!=OXP_SHELL_TRANSPARENT)++lit;
    }
    assert(lit>20u);
    oxp_raster_set_font(0);
    assert(oxp_raster_text_width("AAA",1)==18u);
    /* Malformed control bytes and advances are rejected. */
    data[5]=0xff;
    assert(!oxp_font_parse(&f,data,sizeof data));
    data[5]=OXP_FONT_CW;
    data[16]=0;
    assert(!oxp_font_parse(&f,data,sizeof data));
    puts("PASS: OXFT validation, proportional text, alpha glyphs and shell fallback");
    return 0;
}
#elif defined(OPENXP_TEST_FONT_FIT)
#include "../apps/native/font_engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    /* Proportional advances exercise pixel fitting, not character counts. */
    static uint8_t advances[OXP_FONT_COUNT*OXP_FONT_STYLES];
    for(unsigned i=0;i<sizeof advances;++i)advances[i]=6;
    advances['i'-OXP_FONT_FIRST]=2;
    advances['W'-OXP_FONT_FIRST]=9;
    advances['.'-OXP_FONT_FIRST]=3;
    oxp_font_t font={advances,0};
    oxp_font_fit_t fit=oxp_font_fit(&font,"iiWW",0,22,92);
    assert(fit.chars==4 && fit.width==22 && fit.dots==0);
    fit=oxp_font_fit(&font,"iiWWx",0,22,92);
    assert(fit.chars==3 && fit.dots==3 && fit.width==22);
    fit=oxp_font_fit(&font,"WWWWW",0,8,92);
    assert(fit.chars==0 && fit.dots==0 && fit.width==0);
    fit=oxp_font_fit(&font,"WWWWW",0,9,92);
    assert(fit.chars==0 && fit.dots==3 && fit.width==9);
    fit=oxp_font_fit(&font,"",0,200,92);
    assert(fit.chars==0 && fit.dots==0 && fit.width==0);
    fit=oxp_font_fit(&font,"WWWW",0,100,2);
    assert(fit.chars==2 && fit.dots==3 && fit.width==27);
    fit=oxp_font_fit(&font,"WWWW",0,0,92);
    assert(fit.chars==0 && fit.dots==0);
    fit=oxp_font_fit(&font,0,0,10,92);
    assert(fit.chars==0 && fit.dots==0);
    fit=oxp_font_fit(&font,"WWWW",0,100,0);
    assert(fit.chars==0 && fit.dots==0);
    char very_long[600];memset(very_long,'W',sizeof very_long-1);
    very_long[sizeof very_long-1]=0;
    fit=oxp_font_fit(&font,very_long,0,200,999);
    assert(fit.chars<=256u && fit.width<=200u && fit.dots==3u);
    /* An unterminated buffer is safe with an explicit byte cap. */
    char raw[2]={'W','W'};
    fit=oxp_font_fit(&font,raw,0,100,2);
    assert(fit.chars==2 && fit.dots==3 && fit.width==27);
    puts("PASS: proportional clipping, ellipses, caps and tiny viewports");
    return 0;
}
#elif defined(OPENXP_TEST_SKIN)
#include "../apps/native/skin.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char blob[OXP_SKIN_MAX_BYTES];
static size_t size;
static int memreader(void *unused,uint32_t off,void *dst,uint32_t n){
    (void)unused;
    if(off>size||n>size-off)return 0;
    memcpy(dst,blob+off,n);return 1;
}
static void put32(unsigned char *p,unsigned n){
    p[0]=(unsigned char)n;p[1]=(unsigned char)(n>>8);
    p[2]=(unsigned char)(n>>16);p[3]=(unsigned char)(n>>24);
}
int main(int argc,char **argv){
    assert(argc==2);
    FILE *f=fopen(argv[1],"rb");assert(f);
    size=fread(blob,1,sizeof(blob),f);
    assert(!ferror(f)&&feof(f));fclose(f);
    assert(size>=16u);
    oxp_skin_t skin;
    assert(oxp_skin_parse(&skin,blob,size));
    assert(skin.count==5u);
    for(unsigned i=0;i<skin.count;++i){
        assert(skin.sprites[i].id==i);
        assert(skin.sprites[i].offset<size);
    }
    unsigned char copy[OXP_SKIN_MAX_BYTES];
    oxp_skin_t loaded;
    assert(oxp_skin_load(&loaded,copy,memreader,0));
    uint16_t pixels[1024];
    for(unsigned i=0;i<1024;++i)pixels[i]=0xffffu;
    assert(oxp_skin_nine_row(&loaded,OXP_SKIN_FRAME_ACTIVE,20u,10u,
                             470u,300u,10u,pixels,1024u));
    assert(pixels[20u]==0xffffu); /* transparent curved corner */
    assert(pixels[27u]!=0xffffu); /* painted top highlight */
    assert(pixels[0u]==0xffffu); /* correct scissor */
    assert(!oxp_skin_nine_row(&loaded,0u,20u,10u,470u,300u,310u,pixels,1024u));
    assert(oxp_skin_nine_row(&loaded,OXP_SKIN_FRAME_INACTIVE,1019u,0u,
                             470u,300u,40u,pixels,1024u)); /* clip to last five */
    assert(oxp_skin_nine_row(&loaded,OXP_SKIN_FRAME_ACTIVE,2u,2u,
                             6u,4u,3u,pixels,1024u)); /* tiny 9-slice */
    assert(oxp_skin_region_row(&loaded,OXP_SKIN_CAPTION_ATLAS,48u,48u,24u,24u,
                               100u,10u,21u,21u,13u,pixels,1024u));
    assert(!oxp_skin_region_row(&loaded,OXP_SKIN_CAPTION_ATLAS,70u,70u,24u,24u,
                                100u,10u,21u,21u,13u,pixels,1024u));
    assert(!oxp_skin_nine_row(&loaded,6u,2u,2u,300u,200u,42u,pixels,1024u));
    unsigned char header_backup[16];memcpy(header_backup,blob,16);
    blob[0]='?';assert(!oxp_skin_parse(&skin,blob,size));blob[0]='O';
    put32(blob+8,OXP_SKIN_MAX_BYTES+1u);
    assert(!oxp_skin_load(&skin,copy,memreader,0));
    memcpy(blob,header_backup,16);
    /* Reject pixel ranges outside the resource and duplicate IDs. */
    unsigned char row_backup[20];memcpy(row_backup,blob+16,20);
    put32(blob+16+16,(unsigned)size-2u);
    assert(!oxp_skin_parse(&skin,blob,size));
    memcpy(blob+16,row_backup,20);
    blob[16+20]=0;blob[16+21]=0;
    assert(!oxp_skin_parse(&skin,blob,size));
    puts("PASS: OXSK parser, streamed load, 9-slice, atlas, bounds and fallback");
    return 0;
}
#elif defined(OPENXP_TEST_NATIVE_SCENE)
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
    assert(logical.w>320u && logical.h==OXP_NATIVE_H);
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
    assert(cx>=16u && cx<16u+logical.w && cy>=43u && cy<43u+logical.h);
    oxp_native_pointer_to_canvas(full.x,full.y,1024,726,&cx,&cy);
    assert(cx==16u && cy==43u);
    oxp_native_pointer_to_canvas(14u,44u,1024,726,&cx,&cy);
    assert(cx==1023u && cy==1023u);
    oxp_native_viewport_t narrow=oxp_native_viewport(10,20,280,200);
    assert(narrow.w==258u && narrow.h==151u && narrow.x==26u && narrow.y==63u);
    oxp_native_canvas_t slim=oxp_native_canvas(280,200);
    assert(slim.w<320u && slim.h==180u);
    oxp_native_canvas_t wide=oxp_native_canvas(640,190);
    assert(wide.w==OXP_NATIVE_W && wide.h<180u);
    oxp_native_pointer_to_canvas(0,0,20,20,&cx,&cy);
    assert(cx==1023u && cy==1023u);
    puts("PASS: Ring 3 native-window chrome, proportional scaling, viewport input and clipping");
}
#elif defined(OPENXP_TEST_NATIVE_SCENE_SKIN)
#include "../apps/native/native_scene.h"
#include "../apps/native/skin.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
static unsigned char blob[OXP_SKIN_MAX_BYTES];
static oxp_shell_window_t windows[OXP_NATIVE_SLOTS];
static oxp_native_info_t infos[OXP_NATIVE_SLOTS];
static uint16_t surfaces[OXP_NATIVE_SLOTS][OXP_NATIVE_W*OXP_NATIVE_H];
static uint16_t row_a[OXP_SHELL_RASTER_W],row_b[OXP_SHELL_RASTER_W],row_c[OXP_SHELL_RASTER_W];
int main(int argc,char **argv){
    assert(argc==2);
    FILE *f=fopen(argv[1],"rb");assert(f);
    size_t n=fread(blob,1,sizeof(blob),f);fclose(f);
    oxp_skin_t skin;assert(oxp_skin_parse(&skin,blob,n));
    windows[0]=(oxp_shell_window_t){.visible=1,.active=1,.x=30,.y=40,
        .width=470,.height=300};
    infos[0].active=1;
    memcpy(infos[0].title,"TEST WINDOW",12);
    /* The caption frame is the same code path consumed by windowd. */
    oxp_native_scene_row(windows,infos,surfaces,43,row_a);
    oxp_native_scene_row_skinned(windows,infos,surfaces,43,row_b,&skin,-1,-1,0);
    assert(memcmp(row_a,row_b,sizeof(row_a))!=0);
    assert(row_b[0]==OXP_SHELL_TRANSPARENT && row_b[900]==OXP_SHELL_TRANSPARENT);
    /* Inactive theme uses a distinct atlas, with identical layout. */
    windows[0].active=0;
    oxp_native_scene_row_skinned(windows,infos,surfaces,43,row_c,&skin,-1,-1,0);
    assert(memcmp(row_b,row_c,sizeof(row_b))!=0);
    windows[0].active=1;
    /* Hover/press paths must visibly change close button. */
    unsigned yy=55;
    oxp_native_scene_row_skinned(windows,infos,surfaces,yy,row_a,&skin,-1,-1,0);
    oxp_native_scene_row_skinned(windows,infos,surfaces,yy,row_b,&skin,480,56,0);
    oxp_native_scene_row_skinned(windows,infos,surfaces,yy,row_c,&skin,480,56,1);
    assert(memcmp(row_a,row_b,sizeof(row_a))!=0);
    assert(memcmp(row_b,row_c,sizeof(row_a))!=0);
    /* Same atlas renders on a maximized client without memory overrun. */
    windows[0].x=0;windows[0].y=0;windows[0].width=1024;
    windows[0].height=726;windows[0].maximized=1;
    oxp_native_scene_row_skinned(windows,infos,surfaces,720,row_a,&skin,-1,-1,0);
    assert(row_a[0]!=OXP_SHELL_TRANSPARENT);
    puts("PASS: native compositor uses skinned active/inactive chrome, button states and max window");
    return 0;
}
#elif defined(OPENXP_TEST_NATIVE_THEME)
#include "../apps/native/theme.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    assert(oxp_theme_valid(0) && oxp_theme_valid(2));
    assert(!oxp_theme_valid(3) && !oxp_theme_valid(0xffffffffu));
    for(unsigned i=0;i<3u;++i){
        assert(oxp_theme_wallpaper(i)<=0xffffff);
        assert(oxp_theme_taskbar(i)<=0xffffff);
        assert(oxp_theme_start(i)<=0xffffff);
        for(unsigned j=i+1;j<3u;++j)
            assert(oxp_theme_wallpaper(i)!=oxp_theme_wallpaper(j));
    }
    for(unsigned i=0;i<OXP_THEME_COUNT;++i){
        assert(oxp_theme_name(i)[0]);
        assert(oxp_theme_skin_path(i)[0]);
        assert(oxp_theme_widgets_path(i)[0]);
        assert(oxp_theme_icons_path(i)[0]);
        assert(oxp_theme_panel(i)!=0);
        assert(oxp_theme_accent(i)!=0);
        assert(oxp_theme_toolbar(i)!=0);
    }
    assert(oxp_theme_name(1)[0]=='O' && oxp_theme_name(2)[0]=='M');
    puts("PASS: bounded three-theme palette and resource routing");
}
#elif defined(OPENXP_TEST_SHELL_RASTER)
#include "../apps/native/shell_raster.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint16_t pixels[OXP_SHELL_RASTER_W+2];
int main(void){
    oxp_shell_draw_t cmds[5]={
        {.op=OXP_SHELL_BG_RECT,.x=0,.y=0,.w=1024,.h=20,.color=0xff0000},
        {.op=OXP_SHELL_BG_TEXT,.x=2,.y=1,.w=1,.color=0xffffff,.text="A"},
        {.op=OXP_SHELL_COMPOSE_BEGIN},
        {.op=OXP_SHELL_RECT,.x=0,.y=0,.w=20,.h=5,.color=0x0000ff},
        {.op=OXP_SHELL_TEXT,.x=1,.y=7,.w=2,.color=0x00ff00,.text="B"}
    };
    pixels[0]=0x1234;pixels[OXP_SHELL_RASTER_W+1]=0x4321;
    oxp_raster_row(cmds,5,0,0,pixels+1);
    assert(pixels[1]==0xf800u && pixels[1024]==0xf800u);
    oxp_raster_row(cmds,5,0,2,pixels+1);
    assert(pixels[3]==0xffffu); /* A's top-left glyph pixel */
    oxp_raster_row(cmds,5,1,0,pixels+1);
    assert(pixels[1]==0x001fu && pixels[24]==OXP_SHELL_TRANSPARENT);
    oxp_raster_row(cmds,5,1,7,pixels+1);
    assert(pixels[2]==0x07e0u); /* B top-left lit at 2x scale */
    assert(pixels[10]==OXP_SHELL_TRANSPARENT); /* B outside glyph stays clear */
    assert(pixels[0]==0x1234 && pixels[OXP_SHELL_RASTER_W+1]==0x4321);
    /* Bitmap backdrop remains visible, but the icon tile is drawn over it. */
    oxp_shell_draw_t overlay[]={
        {.op=OXP_SHELL_BG_RECT,.x=0,.y=0,.w=1024,.h=40,.color=0xff0000},
        {.op=OXP_SHELL_BG_RECT,.x=10,.y=0,.w=16,.h=40,.color=0x00ff00},
    };
    for(unsigned x=1;x<=1024;++x)pixels[x]=0x1357;
    oxp_raster_overlay_background(overlay,2,0,pixels+1);
    assert(pixels[1]==0x1357 && pixels[11]==0x07e0 && pixels[200]==0x1357);
    assert(pixels[0]==0x1234 && pixels[1025]==0x4321);
    puts("PASS: Ring 3 RGB565 rasterizer, bitmap overlay, glyphs and bounds");
}
#elif defined(OPENXP_TEST_COMPOSITOR_INTEGRATION)
/* Host-side pixel integration test for the real desktop renderer using a
 * Ring 3 supplied window stacking plan. Hardware interrupt paths are not
 * exercised; this verifies that the compositor uses the submitted order. */
#include "gui/desktop.h"
#include "gui/shell_bridge.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t framebuffer[640u*480u];
static uint32_t first[640u*480u];
void serial_puts(const char *message){(void)message;}
size_t initrd_count(void){return 0;}
bool initrd_name_at(size_t i,char *dst,size_t cap){(void)i;(void)dst;(void)cap;return false;}
int main(void){
    static desktop_t d;
    fb_surface_t s={(volatile uint8_t *)framebuffer,640,480,640*4,
        8,8,8,16,8,0,false,0,0,0,0};
    desktop_init(&d,&s,(shell_services_t){0});
    for(unsigned i=0;i<DESKTOP_WINDOWS;++i){d.windows[i].open=false;d.windows[i].minimized=false;}
    d.windows[DESKTOP_ABOUT]=(desktop_window_t){45,45,370,255,true,false,false,0,0,0,0};
    d.windows[DESKTOP_NOTEPAD]=(desktop_window_t){45,45,370,255,true,false,false,0,0,0,0};
    shell_bridge_reset();shell_bridge_attach(&d);
    oxp_shell_draw_t cmds[3]={{.op=OXP_SHELL_COMPOSE_BEGIN},
        {.op=OXP_SHELL_COMPOSE_WINDOW,.x=DESKTOP_ABOUT},
        {.op=OXP_SHELL_COMPOSE_WINDOW,.x=DESKTOP_NOTEPAD}};
    assert(shell_bridge_submit(77,cmds,3,100));
    desktop_draw(&d);
    memcpy(first,framebuffer,sizeof(first));
    cmds[1].x=DESKTOP_NOTEPAD;cmds[2].x=DESKTOP_ABOUT;
    assert(shell_bridge_submit(77,cmds,3,101));
    memset(framebuffer,0,sizeof(framebuffer));
    desktop_draw(&d);
    size_t different=0;
    for(size_t i=0;i<640u*480u;++i)different+=(first[i]!=framebuffer[i]);
    assert(different>100 && "Ring 3 ordering should change composed pixels");
    /* Header-button dispatch must agree with the Ring 3 z-order plan,
       even when it differs from the legacy kernel focus stack. */
    assert(!d.windows[DESKTOP_ABOUT].maximized);
    assert(!d.windows[DESKTOP_NOTEPAD].maximized);
    desktop_mouse(&d,45+370-45,56,true);
    assert(d.windows[DESKTOP_ABOUT].maximized);
    assert(!d.windows[DESKTOP_NOTEPAD].maximized);
    desktop_mouse(&d,45+370-45,56,false);
    shell_bridge_shutdown();
    assert(!shell_bridge_active());
    memset(framebuffer,0,sizeof(framebuffer));
    desktop_draw(&d); /* Legacy safety path still rasterizes the desktop. */
    assert(framebuffer[3*640+3]!=0);
    puts("PASS: windowd composition ordering affects actual pixels; fallback renders");
    return 0;
}
#elif defined(OPENXP_TEST_SHELL_MENU_ART)
#include "../apps/native/shell_menu_art.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char data[OXP_MENU_ART_BYTES];
int main(int argc,char **argv){
    assert(argc==2);
    FILE *fp=fopen(argv[1],"rb");assert(fp);
    size_t n=fread(data,1,sizeof data,fp);assert(!ferror(fp));
    assert(fgetc(fp)==EOF);fclose(fp);
    oxp_menu_art_t atlas={0};
    assert(n==sizeof data);
    assert(oxp_menu_art_parse(&atlas,data,n));
    uint16_t row[1024];for(unsigned i=0;i<1024;++i)row[i]=0x1234u;
    assert(oxp_menu_art_row(&atlas,OXP_MENU_ART_PANEL,4,356,514,370,356,row,1024));
    assert(row[3]==0x1234u && row[32]!=0x1234u);
    assert(oxp_menu_art_row(&atlas,OXP_MENU_ART_SELECTED,13,410,234,37,435,row,1024));
    assert(oxp_menu_art_row(&atlas,OXP_MENU_ART_DESKTOP_LABEL,9,60,90,22,67,row,1024));
    assert(!oxp_menu_art_row(&atlas,6u,0,0,100,30,15,row,1024));
    assert(!oxp_menu_art_row(&atlas,0,0,0,100,30,30,row,1024));
    assert(oxp_menu_art_row(&atlas,0,1020,0,100,30,5,row,1024));
    assert(oxp_menu_art_row(&atlas,0,0,0,3,4,0,row,1024));
    unsigned char old=data[0];data[0]='X';
    assert(!oxp_menu_art_parse(&atlas,data,n));assert(!atlas.pixels);
    data[0]=old;
    assert(!oxp_menu_art_parse(&atlas,data,n-1));
    data[6]=0xff;assert(!oxp_menu_art_parse(&atlas,data,n));
    puts("PASS: Aurora 7 OXUA menu artwork parsing, clipping, 9-slice and fallback");
    return 0;
}
#elif defined(OPENXP_TEST_SHELL_ICONS)
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../apps/native/shell_icons.h"
#include "../apps/native/shell_layout.h"
#include "../kernel/user/shell_abi.h"
int main(void){
    static uint8_t atlas[OXP_SHELL_ICON_BYTES];
    oxp_shell_icons_t icons={0};
    assert(!oxp_shell_icons_parse(&icons,atlas,sizeof atlas));
    atlas[0]='O';atlas[1]='X';atlas[2]='U';atlas[3]='A';
    atlas[4]=1;atlas[6]=192;atlas[8]=64;
    unsigned count=OXP_SHELL_ICON_W*OXP_SHELL_ICON_H*3u;
    atlas[12]=(uint8_t)count;atlas[13]=(uint8_t)(count>>8u);
    atlas[14]=(uint8_t)(count>>16u);
    assert(!oxp_shell_icons_parse(&icons,atlas,sizeof atlas-1u));
    assert(oxp_shell_icons_parse(&icons,atlas,sizeof atlas));
    assert(icons.pixels==atlas+16);
    uint16_t pixels[128];
    for(unsigned i=0;i<128;++i)pixels[i]=0x07e0u;
    /* First pixel of atlas icon 0, fully opaque magenta. */
    atlas[16]=0x1f;atlas[17]=0xf8;atlas[18]=255;
    oxp_shell_icon_row(&icons,0,10,3,32,3,pixels,128);
    assert(pixels[10]==0xf81fu);
    assert(pixels[11]==0x07e0u);
    /* Alpha zero leaves the transparent sentinel as-is. */
    pixels[10]=OXP_SHELL_TRANSPARENT;atlas[18]=0;
    oxp_shell_icon_row(&icons,0,10,3,32,3,pixels,128);
    assert(pixels[10]==OXP_SHELL_TRANSPARENT);
    /* Blend half against black must neither equal black nor magenta. */
    pixels[10]=0;atlas[18]=128;
    oxp_shell_icon_row(&icons,0,10,3,32,3,pixels,128);
    assert(pixels[10]!=0u && pixels[10]!=0xf81fu);
    /* Clip drawing past the right border. */
    pixels[127]=0x1234;
    oxp_shell_icon_row(&icons,0,127,3,32,3,pixels,128);
    assert(pixels[126]==0x07e0u);
    oxp_shell_icon_row(&icons,12,0,0,32,0,pixels,128);
    assert(oxp_desktop_hit(OXP_DESKTOP_X+10,OXP_DESKTOP_TOP+1)==0);
    assert(oxp_desktop_hit(OXP_DESKTOP_X,OXP_DESKTOP_TOP+OXP_DESKTOP_SLOT_H)==-1);
    assert(oxp_desktop_hit(OXP_DESKTOP_X,OXP_DESKTOP_TOP+4u*OXP_DESKTOP_STEP)==4);
    assert(oxp_desktop_hit(OXP_DESKTOP_X,725)==-1);
    assert(oxp_menu_hit(OXP_MENU_ROW_X+4,OXP_MENU_ROW_Y+3)==0);
    assert(oxp_menu_hit(OXP_MENU_ROW_X+OXP_MENU_COLUMN_W+4,OXP_MENU_ROW_Y+6u*OXP_MENU_ROW_STEP)==13);
    assert(oxp_menu_hit(0,OXP_MENU_ROW_Y)==-1);
    assert(oxp_menu_hit(OXP_MENU_ROW_X+234u,OXP_MENU_ROW_Y)==-1);
    assert(oxp_desktop_app(0)==1u && oxp_desktop_app(4)==12u);
    assert(oxp_menu_icon(9)==8u);
    puts("PASS: Aurora 4 shell icons, alpha, clipping, hit targets");
    return 0;
}
#elif defined(OPENXP_TEST_WALLPAPER)
#include "../apps/native/wallpaper.h"
#include "../apps/native/filetypes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* 2x2 padded 24-bit BMP: bottom row red/green; top row blue/white. */
static unsigned char bmp[70];
static int read_chunk(void *ctx,uint32_t offset,void *out,uint32_t n){
    const unsigned char *p=ctx;
    if(offset>sizeof bmp||n>sizeof bmp-offset)return 0;
    memcpy(out,p+offset,n);return 1;
}
static void set32(unsigned pos,unsigned n){for(unsigned i=0;i<4;++i)bmp[pos+i]=(unsigned char)(n>>(8*i));}
static void set16(unsigned pos,unsigned n){bmp[pos]=(unsigned char)n;bmp[pos+1]=(unsigned char)(n>>8);}
int main(void){
    assert(oxp_filetype("windows/system32/notepad.exe",0)==OXP_FT_EXE);
    assert(oxp_filetype("windows/web/wallpaper/azure.BMP",0)==OXP_FT_BITMAP);
    assert(oxp_filetype("home/note.txt",0)==OXP_FT_TEXT);
    assert(oxp_filetype("home/picture.oxp",0)==OXP_FT_DRAWING);
    assert(oxp_filetype("docs/config.ini",0)==OXP_FT_CONFIG);
    assert(oxp_filetype("program files",1)==OXP_FT_DIR);
    assert(oxp_filetype_app(OXP_FT_TEXT) && oxp_filetype_app(OXP_FT_DRAWING));
    assert(!oxp_filetype_app(OXP_FT_EXE));
    bmp[0]='B';bmp[1]='M';set32(2,sizeof bmp);set32(10,54);
    set32(14,40);set32(18,2);set32(22,2);set16(26,1);set16(28,24);
    /* bottom row */
    bmp[54+2]=255;bmp[57+1]=255;
    /* top row */
    bmp[62]=255;bmp[65]=255;bmp[66]=255;bmp[67]=255;
    static uint16_t pixels[OXP_WALL_W*OXP_WALL_H];
    assert(oxp_wall_decode(read_chunk,bmp,pixels));
    assert(pixels[0]==oxp_wall_rgb565(0,0,255));
    assert(pixels[OXP_WALL_W-1]==oxp_wall_rgb565(255,255,255));
    assert(pixels[(OXP_WALL_H-1)*OXP_WALL_W]==oxp_wall_rgb565(255,0,0));
    assert(pixels[OXP_WALL_W*OXP_WALL_H-1]==oxp_wall_rgb565(0,255,0));
    bmp[28]=32;assert(!oxp_wall_decode(read_chunk,bmp,pixels));
    bmp[28]=24;set32(18,2048);assert(!oxp_wall_decode(read_chunk,bmp,pixels));
    set32(18,2);set32(2,69);assert(!oxp_wall_decode(read_chunk,bmp,pixels));
    puts("PASS: file associations and strict BMP decoding / scaling");
}
#else
#error "Compile this suite with an OPENXP_TEST_* selector"
#endif
