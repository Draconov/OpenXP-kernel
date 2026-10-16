#ifndef OPENXP_PAINT_CODEC_H
#define OPENXP_PAINT_CODEC_H
#include <stdint.h>
/* Exactly 64x40, three bits/pixel, 960 bytes: fits a 1024-byte FS syscall.
 * Bits crossing byte boundaries are deliberately handled without UB. */
#define OXP_PAINT_W 64u
#define OXP_PAINT_H 40u
#define OXP_PAINT_SIZE (OXP_PAINT_W*OXP_PAINT_H*3u/8u)
static inline unsigned oxp_paint_get(const uint8_t *pixels,unsigned x,unsigned y){
    if(x>=OXP_PAINT_W || y>=OXP_PAINT_H)return 0;
    unsigned bit=(y*OXP_PAINT_W+x)*3u, byte=bit/8u,shift=bit%8u;
    unsigned value=(unsigned)pixels[byte]>>shift;
    if(shift>5u)value|=(unsigned)pixels[byte+1]<<(8u-shift);
    return value&7u;
}
static inline void oxp_paint_put(uint8_t *pixels,unsigned x,unsigned y,unsigned color){
    if(x>=OXP_PAINT_W || y>=OXP_PAINT_H || color>7u)return;
    unsigned bit=(y*OXP_PAINT_W+x)*3u,byte=bit/8u,shift=bit%8u;
    uint16_t word=pixels[byte];
    if(shift>5u)word|=(uint16_t)((uint16_t)pixels[byte+1]<<8u);
    word=(uint16_t)((word&~((uint16_t)7u<<shift))|((uint16_t)color<<shift));
    pixels[byte]=(uint8_t)word;
    if(shift>5u)pixels[byte+1]=(uint8_t)(word>>8u);
}
/* Inclusive 8-connected Bresenham segment. Calls a bounded plotter and
 * includes both endpoints, so fast pointer motion cannot leave gaps. */
typedef void (*oxp_paint_plot_fn)(unsigned x,unsigned y,void *context);
static inline void oxp_paint_stroke(unsigned x0,unsigned y0,unsigned x1,unsigned y1,
                                     oxp_paint_plot_fn plot,void *context){
    if(x0>=OXP_PAINT_W||x1>=OXP_PAINT_W||y0>=OXP_PAINT_H||y1>=OXP_PAINT_H||!plot)return;
    int x=(int)x0,y=(int)y0,tx=(int)x1,ty=(int)y1;
    int dx=tx>x?tx-x:x-tx,dy=ty>y?ty-y:y-ty;
    int sx=x<tx?1:-1,sy=y<ty?1:-1,err=dx-dy;
    for(;;){
        plot((unsigned)x,(unsigned)y,context);
        if(x==tx&&y==ty)break;
        int twice=err*2;
        if(twice>-dy){err-=dy;x+=sx;}
        if(twice<dx){err+=dx;y+=sy;}
    }
}
#endif
