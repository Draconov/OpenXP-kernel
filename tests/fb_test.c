#include "video/fb.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint32_t area[16*16+4];
int main(void){
    for(int i=0;i<260;++i)area[i]=0x76543210;
    fb_surface_t s={ (volatile uint8_t*)(area+2), 16,16,16*4,8,8,8,16,8,0 };
    fb_rect(&s,-8,-8,12,12,0x112233);
    assert(area[2]==0x112233 && area[2+3+3*16]==0x112233 && area[2+4]==0x76543210);
    fb_rect(&s,15,15,2147483647,2147483647,0xabcdef);
    assert(area[2+15+15*16]==0xabcdef);
    fb_text(&s,0,0,"Start!",1,0xff0000);
    assert(area[0]==0x76543210 && area[1]==0x76543210 && area[258]==0x76543210 && area[259]==0x76543210);
    assert(fb_rgb(&s,1,2,3)==0x010203);
    puts("PASS: framebuffer clipping, text, RGB and sentinels");
}
