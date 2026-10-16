#ifndef OPENXP_NATIVE_PAINT_HISTORY_H
#define OPENXP_NATIVE_PAINT_HISTORY_H
#include <stddef.h>
#include <stdint.h>
#define OXP_PAINT_UNDO_LEVELS 8u
/* One history step per completed paint stroke, bounded in RAM. */
typedef struct {
    uint8_t frame[OXP_PAINT_UNDO_LEVELS][OXP_PAINT_SIZE];
    unsigned count,index;
} oxp_paint_history_t;
static inline void oxp_paint_hist_init(oxp_paint_history_t *h,const uint8_t *p){
    h->count=1;h->index=0;
    for(size_t i=0;i<OXP_PAINT_SIZE;++i)h->frame[0][i]=p[i];
}
static inline void oxp_paint_hist_record(oxp_paint_history_t *h,const uint8_t *p){
    if(h->count==0){oxp_paint_hist_init(h,p);return;}
    if(h->index+1<h->count)h->count=h->index+1;
    if(h->count==OXP_PAINT_UNDO_LEVELS){
        for(unsigned j=1;j<h->count;++j)
            for(size_t i=0;i<OXP_PAINT_SIZE;++i)h->frame[j-1][i]=h->frame[j][i];
        --h->count;
        if(h->index)--h->index;
    }
    ++h->index;h->count=h->index+1;
    for(size_t i=0;i<OXP_PAINT_SIZE;++i)h->frame[h->index][i]=p[i];
}
static inline int oxp_paint_hist_step(oxp_paint_history_t *h,int direction,uint8_t *p){
    if(!h->count||(direction<0&&h->index==0)||
       (direction>0&&h->index+1>=h->count))return 0;
    h->index=direction<0?h->index-1:h->index+1;
    for(size_t i=0;i<OXP_PAINT_SIZE;++i)p[i]=h->frame[h->index][i];
    return 1;
}
#endif
