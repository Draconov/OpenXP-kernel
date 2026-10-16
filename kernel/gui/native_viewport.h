#ifndef OPENXP_GUI_NATIVE_VIEWPORT_H
#define OPENXP_GUI_NATIVE_VIEWPORT_H
#include <stdint.h>
#include "../user/shell_abi.h"
/* Per-window logical surface dimensions, bounded by OXP_NATIVE_W/H.
 * A normal window stays approximately 320x180, but larger windows acquire
 * more real drawing area rather than magnifying the old 320x180 bitmap.
 * The viewport always fills the client area; aspect ratio is kept within
 * one logical pixel. The compositor scales only the remaining fraction. */
typedef struct { uint32_t x,y,w,h; } oxp_native_viewport_t;
typedef struct { uint32_t w,h; } oxp_native_canvas_t;
static inline oxp_native_canvas_t oxp_native_canvas(uint32_t width,uint32_t height){
    oxp_native_canvas_t out={320u,180u};
    if(width<=22u || height<=49u)return out;
    uint32_t cw=width-22u,ch=height-49u;
    /* 448x251 client pixels -> 320x180. Large clients grow to max 360
     * logical rows, about 1.9 physical pixels per logical sample. */
    uint32_t logical_h=(uint32_t)((uint64_t)ch*180u/251u);
    if(logical_h<180u)logical_h=180u;
    if(logical_h>OXP_NATIVE_H)logical_h=OXP_NATIVE_H;
    uint32_t logical_w=(uint32_t)((uint64_t)cw*logical_h/ch);
    if(logical_w>OXP_NATIVE_W){
        logical_w=OXP_NATIVE_W;
        logical_h=(uint32_t)((uint64_t)ch*logical_w/cw);
    }
    if(!logical_w)logical_w=1u;
    if(!logical_h)logical_h=1u;
    out.w=logical_w;out.h=logical_h;
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
