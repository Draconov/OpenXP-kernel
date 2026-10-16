#ifndef OPENXP_CALCULATOR_LAYOUT_H
#define OPENXP_CALCULATOR_LAYOUT_H
/* Shared pure geometry for drawing and input. Native click events carry
 * (16,43) window decoration offsets, which oxp_hit() handles centrally. */
#include "ui_layout.h"
typedef struct {unsigned x,y,w,h;} oxp_calc_rect;
static inline unsigned oxp_calc_top(unsigned h){
    return h>=160u?76u:h>=100u?48u:31u;
}
static inline oxp_calc_rect oxp_calc_key(unsigned w,unsigned h,unsigned col,unsigned row){
    oxp_calc_rect empty={0,0,0,0};
    if(w<64u||h<65u||col>=4u||row>=4u)return empty;
    unsigned top=oxp_calc_top(h),sx=(w-20u)/4u;
    unsigned sy=h>top+8u?(h-top-8u)/4u:0u;
    if(sx<5u||sy<5u)return empty;
    oxp_calc_rect r={10u+col*sx,top+row*sy,sx-3u,sy-3u};
    return r;
}
static inline int oxp_calc_key_at(unsigned w,unsigned h,unsigned x,unsigned y){
    for(unsigned r=0;r<4u;++r)for(unsigned c=0;c<4u;++c){
        oxp_calc_rect b=oxp_calc_key(w,h,c,r);
        if(b.w&&b.h&&x>=b.x&&y>=b.y&&x-b.x<b.w&&y-b.y<b.h)
            return (int)(r*4u+c);
    }
    return -1;
}
#endif
