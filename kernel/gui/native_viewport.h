#ifndef OPENXP_GUI_NATIVE_VIEWPORT_H
#define OPENXP_GUI_NATIVE_VIEWPORT_H
#include <stdint.h>
#include "../user/shell_abi.h"
/* The protected surface remains a 320x180 RGB565 allocation, but each
 * window uses a canvas cropped to the client's aspect ratio. Each visible
 * logical pixel therefore has the same scale in X/Y without letterboxing.
 * These pure helpers are shared by rendering, hit testing and layout queries. */
typedef struct { uint32_t x,y,w,h; } oxp_native_viewport_t;
typedef struct { uint32_t w,h; } oxp_native_canvas_t;
static inline oxp_native_canvas_t oxp_native_canvas(uint32_t width,uint32_t height){
    oxp_native_canvas_t out={OXP_NATIVE_W,OXP_NATIVE_H};
    if(width<=22u || height<=49u)return out;
    uint32_t cw=width-22u,ch=height-49u;
    if((uint64_t)cw*OXP_NATIVE_H < (uint64_t)ch*OXP_NATIVE_W){
        out.w=(uint32_t)((uint64_t)OXP_NATIVE_H*cw/ch);
        if(!out.w)out.w=1u;
    }else{
        out.h=(uint32_t)((uint64_t)OXP_NATIVE_W*ch/cw);
        if(!out.h)out.h=1u;
    }
    return out;
}
static inline oxp_native_viewport_t oxp_native_viewport(uint32_t x,uint32_t y,
                                                        uint32_t width,uint32_t height){
    oxp_native_viewport_t out={0};
    if(width<=22u || height<=49u)return out;
    out.x=x+16u;out.y=y+43u;
    out.w=width-22u;out.h=height-49u;
    return out;
}
/* Click ABI retains window-relative (+16,+43) coordinates. Outside the
 * content rectangle clicks map to 1023,1023 and cannot activate a widget. */
static inline void oxp_native_pointer_to_canvas(uint32_t x,uint32_t y,
                                                uint32_t width,uint32_t height,
                                                uint32_t *out_x,uint32_t *out_y){
    oxp_native_viewport_t v=oxp_native_viewport(0,0,width,height);
    oxp_native_canvas_t c=oxp_native_canvas(width,height);
    *out_x=1023u;*out_y=1023u;
    if(!v.w || !v.h || x<v.x || y<v.y || x-v.x>=v.w || y-v.y>=v.h)return;
    *out_x=16u+(uint32_t)((uint64_t)(x-v.x)*c.w/v.w);
    *out_y=43u+(uint32_t)((uint64_t)(y-v.y)*c.h/v.h);
}
#endif
