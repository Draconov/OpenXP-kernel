#include "../apps/native/paint_codec.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    uint8_t p[OXP_PAINT_SIZE]={0};
    for(unsigned y=0;y<OXP_PAINT_H;++y)for(unsigned x=0;x<OXP_PAINT_W;++x)
        oxp_paint_put(p,x,y,(x+y)%8u);
    for(unsigned y=0;y<OXP_PAINT_H;++y)for(unsigned x=0;x<OXP_PAINT_W;++x)
        assert(oxp_paint_get(p,x,y)==(x+y)%8u);
    uint8_t first=p[0],last=p[OXP_PAINT_SIZE-1];
    oxp_paint_put(p,OXP_PAINT_W,0,7);
    oxp_paint_put(p,0,OXP_PAINT_H,7);
    oxp_paint_put(p,0,0,8);
    assert(p[0]==first&&p[OXP_PAINT_SIZE-1]==last);
    for(unsigned c=0;c<8;++c){oxp_paint_put(p,0,0,c);assert(oxp_paint_get(p,0,0)==c);}
    puts("PASS: 3-bit packed native Paint data, byte boundaries and bounds");
    return 0;
}
