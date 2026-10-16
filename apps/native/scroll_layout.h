#ifndef OPENXP_NATIVE_SCROLL_LAYOUT_H
#define OPENXP_NATIVE_SCROLL_LAYOUT_H
/* Pure scroll math shared by editors and list views. Caller owns the logical
 * position, so resizing a viewport does not reset the scroll offset. */
typedef struct { unsigned y, h; } oxp_scroll_thumb_t;
static inline unsigned oxp_scroll_limit(unsigned count,unsigned visible){
    return count>visible?count-visible:0u;
}
static inline unsigned oxp_scroll_clamp(unsigned offset,unsigned count,unsigned visible){
    unsigned max=oxp_scroll_limit(count,visible);
    return offset>max?max:offset;
}
static inline unsigned oxp_scroll_step(unsigned offset,int delta,unsigned count,unsigned visible){
    unsigned max=oxp_scroll_limit(count,visible);
    if(offset>max)offset=max;
    if(delta<0)return offset?offset-1u:0u;
    return delta>0&&offset<max?offset+1u:offset;
}
static inline unsigned oxp_scroll_follow(unsigned offset,unsigned focus,unsigned visible){
    if(!visible)return offset;
    if(focus<offset)return focus;
    if(focus-offset>=visible)return focus-visible+1u;
    return offset;
}
static inline oxp_scroll_thumb_t oxp_scroll_thumb(unsigned offset,unsigned count,
                                                   unsigned visible,unsigned rail_height){
    oxp_scroll_thumb_t thumb={0,rail_height};
    if(!rail_height||!visible||count<=visible)return thumb;
    unsigned height=(unsigned)((unsigned long long)rail_height*visible/count);
    if(height<10u)height=rail_height<10u?rail_height:10u;
    if(height>rail_height)height=rail_height;
    unsigned max=count-visible;
    if(offset>max)offset=max;
    thumb.h=height;
    thumb.y=(unsigned)((unsigned long long)offset*(rail_height-height)/max);
    return thumb;
}
#endif
