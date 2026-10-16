#ifndef OPENXP_UI_POINTER_H
#define OPENXP_UI_POINTER_H
/* Pure native-app pointer FSM. No kernel pointers or privileged mappings.
 * Kernel events keep legacy window-relative, decoration-inclusive coords. */
#include <stdint.h>
#define OXP_UI_POINTER_LIMIT 64u
#define OXP_UI_POINTER_NONE (-1)
typedef struct {unsigned x,y,w,h;} oxp_pointer_rect_t;
typedef struct {
    oxp_pointer_rect_t items[OXP_UI_POINTER_LIMIT];
    unsigned count;
    int hover,armed,focus;
    unsigned down_x,down_y;
    int down;
} oxp_pointer_model_t;
static inline void oxp_pointer_reset(oxp_pointer_model_t *m){
    *m=(oxp_pointer_model_t){0};m->hover=-1;m->armed=-1;m->focus=-1;
}
static inline int oxp_pointer_at(const oxp_pointer_model_t *m,unsigned x,unsigned y){
    /* X/Y from input include the window frame's 16/43-pixel offsets. */
    if(x<16u||y<43u||x==1023u||y==1023u)return -1;
    x-=16u;y-=43u;
    for(unsigned n=m->count;n>0;--n){
        const oxp_pointer_rect_t *r=&m->items[n-1u];
        if(r->w&&r->h&&x>=r->x&&y>=r->y&&x-r->x<r->w&&y-r->y<r->h)
            return (int)(n-1u);
    }
    return -1;
}
static inline int oxp_pointer_add(oxp_pointer_model_t *m,oxp_pointer_rect_t r){
    if(m->count>=OXP_UI_POINTER_LIMIT)return -1;
    int index=(int)m->count;m->items[m->count++]=r;return index;
}
static inline int oxp_pointer_style(const oxp_pointer_model_t *m,int id){
    return id>=0 && id==m->hover?(m->down && m->armed==id?2:1):
           id>=0 && id==m->focus?1:0;
}
/* Returns -2: no click, otherwise click target ID (or -1 for a generic
 * content-area click). Pointer updates set *changed for a cheap repaint. */
static inline int oxp_pointer_feed(oxp_pointer_model_t *m,uint32_t event,int *changed){
    unsigned kind=event & OXP_POINTER_MASK;
    if(kind!=OXP_POINTER_MOVE&&kind!=OXP_POINTER_DOWN&&kind!=OXP_POINTER_UP&&
       event!=OXP_POINTER_LEAVE)return -2;
    int before_h=m->hover,before_d=m->down;
    if(event==OXP_POINTER_LEAVE){m->hover=-1;*changed|=before_h!=-1;return -2;}
    unsigned x=(event>>10)&1023u,y=event&1023u;
    m->hover=oxp_pointer_at(m,x,y);
    if(kind==OXP_POINTER_DOWN){
        m->down=1;m->armed=m->hover;m->down_x=x;m->down_y=y;
        m->focus=m->hover;
    }else if(kind==OXP_POINTER_UP){
        int click=-2;
        if(m->down){
            if(m->armed>=0 && m->armed==m->hover)click=m->armed;
            else if(m->armed==-1 && m->hover==-1 && x!=1023u && y!=1023u &&
                    x>=16u && y>=43u &&
                    (x>m->down_x?x-m->down_x:m->down_x-x)<=5u &&
                    (y>m->down_y?y-m->down_y:m->down_y-y)<=5u)click=-1;
        }
        m->down=0;m->armed=-1;
        *changed|=before_h!=m->hover||before_d!=m->down;
        return click;
    }
    *changed|=before_h!=m->hover||before_d!=m->down;
    return -2;
}
#endif
