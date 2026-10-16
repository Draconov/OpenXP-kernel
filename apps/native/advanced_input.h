#ifndef OPENXP_NATIVE_ADVANCED_INPUT_H
#define OPENXP_NATIVE_ADVANCED_INPUT_H
/* Pure pointer gesture helpers shared by editor and scrollbar clients.
 * Native pointer coordinates include the 16x43 window-decoration origin.
 * A packed (1023,1023) signifies a captured pointer outside the client.
 * No host pointers, syscalls, or non-bounded allocations are used here. */
#include <stdint.h>
#define OXP_GESTURE_MOVE 0x20000000u
#define OXP_GESTURE_DOWN 0x10000000u
#define OXP_GESTURE_UP 0x08000000u
#define OXP_GESTURE_MASK 0x38000000u
#define OXP_GESTURE_LEAVE 0x10008u

typedef struct { unsigned x,y; int valid; } oxp_gesture_point_t;
typedef struct { int active; oxp_gesture_point_t start,previous; } oxp_gesture_t;
static inline oxp_gesture_point_t oxp_gesture_point(uint32_t e){
    unsigned x=(e>>10)&1023u,y=e&1023u;
    if(x<16u||y<43u||x==1023u||y==1023u)
        return (oxp_gesture_point_t){0,0,0};
    return (oxp_gesture_point_t){x-16u,y-43u,1};
}
static inline int oxp_gesture_inside(oxp_gesture_point_t p,
                                     unsigned x,unsigned y,unsigned w,unsigned h){
    return p.valid&&w&&h&&p.x>=x&&p.y>=y&&p.x-x<w&&p.y-y<h;
}
/* Return DOWN/MOVE/UP when a gesture is present. LEAVE does not cancel a
 * captured gesture: the kernel must still deliver its corresponding UP. */
static inline unsigned oxp_gesture_feed(oxp_gesture_t *g,uint32_t e){
    unsigned k=e&OXP_GESTURE_MASK;
    if(k==OXP_GESTURE_DOWN){
        g->active=1;g->start=g->previous=oxp_gesture_point(e);
        return OXP_GESTURE_DOWN;
    }
    if(k==OXP_GESTURE_MOVE && g->active)return OXP_GESTURE_MOVE;
    if(k==OXP_GESTURE_UP && g->active){g->active=0;return OXP_GESTURE_UP;}
    return 0;
}
static inline int oxp_gesture_click(const oxp_gesture_t *g,oxp_gesture_point_t end,
                                    unsigned x,unsigned y,unsigned w,unsigned h){
    return oxp_gesture_inside(g->start,x,y,w,h)&&
           oxp_gesture_inside(end,x,y,w,h);
}
/* Drag the thumb, preserving initial grab position. A track click may instead
 * center the thumb on the pointer. Uses 64-bit intermediates to avoid wrap. */
static inline unsigned oxp_gesture_scroll_at(unsigned y,unsigned rail_y,
          unsigned rail_h,unsigned thumb_h,unsigned grab,unsigned maximum){
    if(!maximum||thumb_h>=rail_h)return 0;
    unsigned range=rail_h-thumb_h;
    unsigned pos=y>rail_y?y-rail_y:0u;
    pos=pos>grab?pos-grab:0u;
    if(pos>range)pos=range;
    return (unsigned)(((uint64_t)pos*maximum+range/2u)/range);
}
#endif
