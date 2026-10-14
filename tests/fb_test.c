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
