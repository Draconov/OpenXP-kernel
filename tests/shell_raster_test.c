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
