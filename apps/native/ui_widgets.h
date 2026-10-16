#ifndef OPENXP_UI_WIDGETS_H
#define OPENXP_UI_WIDGETS_H
/* Aurora 2 shared native widgets. All coordinates are logical client pixels;
 * drawing uses the bounded native GUI command API, never a framebuffer pointer.
 * The bitmap artwork is loaded per process; theme indices are persisted by
 * Settings. Focus, scrolling, text and click handling stay app-owned. */
#include "oxp.h"
#include "ui_atlas.h"
#include "ui_pointer.h"

typedef struct { unsigned x,y,w,h; } oxp_ui_rect;
typedef enum { OXP_UI_NORMAL=0, OXP_UI_PRIMARY=1, OXP_UI_DANGER=2 } oxp_ui_tone;

/* Every process owns its own registry, rebuilt during each render.
 * There are no cross-process pointers; widget targets are geometry only. */
static oxp_pointer_model_t oxp_ui_pointer;
static int oxp_ui_pointer_ready;
static inline void oxp_ui_pointer_init(void){
    if(oxp_ui_pointer_ready)return;
    oxp_pointer_reset(&oxp_ui_pointer);
    oxp_ui_pointer_ready=oxp_pointer_enable()==0?1:-1;
}
static inline int oxp_ui_record(oxp_ui_rect r){
    if(oxp_ui_pointer_ready!=1)return -1;
    return oxp_pointer_add(&oxp_ui_pointer,(oxp_pointer_rect_t){r.x,r.y,r.w,r.h});
}
static inline int oxp_ui_pointer_style(int index){
    return oxp_pointer_style(&oxp_ui_pointer,index);
}
/* Existing apps continue receiving legacy CLICK-shaped events, but only
 * after a real release over the original target. Motion stays coalesced and
 * hover does not repaint if the hovered control did not change. */
static inline uint32_t oxp_ui_wait_theme(void){
    for(;;){
        uint32_t e=oxp_wait(25u);
        int changed=0;
        int click=oxp_pointer_feed(&oxp_ui_pointer,e,&changed);
        if((e&OXP_POINTER_MASK)||e==OXP_POINTER_LEAVE){
            if(click!=-2){
                oxp_ui_pointer.count=0;
                return OXP_CLICK|(e&0xfffffu);
            }
            if(changed){oxp_ui_pointer.count=0;return OXP_RESIZE;}
            continue;
        }
        if((e==9u||e==OXP_SHIFT_TAB) && oxp_ui_pointer.count){
            unsigned n=oxp_ui_pointer.count;
            oxp_ui_pointer.focus=e==9u?
                (oxp_ui_pointer.focus+1)%(int)n:
                (oxp_ui_pointer.focus<0?0:
                 (oxp_ui_pointer.focus+(int)n-1)%(int)n);
            oxp_ui_pointer.count=0;return OXP_RESIZE;
        }
        if((e==10u||e==13u) && oxp_ui_pointer.focus>=0 &&
           (unsigned)oxp_ui_pointer.focus<oxp_ui_pointer.count){
            oxp_pointer_rect_t r=oxp_ui_pointer.items[oxp_ui_pointer.focus];
            oxp_ui_pointer.count=0;
            return OXP_CLICK|(((r.x+r.w/2u+16u)&1023u)<<10)|
                   ((r.y+r.h/2u+43u)&1023u);
        }
        if(e){oxp_ui_pointer.count=0;return e;}
        if(oxp_ui_refresh_theme()){oxp_ui_pointer.count=0;return OXP_RESIZE;}
    }
}
/* Aurora 12: gesture-aware opt-in for painting, document selection and
 * thumb dragging. Unlike oxp_ui_wait_theme it preserves raw DOWN/MOVE/UP
 * while still feeding the shared hover/focus state machine. Ordinary apps
 * keep release-activated legacy CLICK synthesis unchanged. */
static inline uint32_t oxp_ui_wait_gesture(void){
    for(;;){
        uint32_t e=oxp_wait(25u);
        int changed=0;
        (void)oxp_pointer_feed(&oxp_ui_pointer,e,&changed);
        if((e&OXP_POINTER_MASK)||e==OXP_POINTER_LEAVE){
            if((e&OXP_POINTER_MASK)==OXP_POINTER_MOVE && !oxp_ui_pointer.down){
                if(changed){oxp_ui_pointer.count=0;return OXP_RESIZE;}
                continue; /* suppress idle motion; captured drags are lossless */
            }
            oxp_ui_pointer.count=0; /* render rebuilds geometry on every input */
            return e;
        }
        if((e==9u||e==OXP_SHIFT_TAB) && oxp_ui_pointer.count){
            unsigned n=oxp_ui_pointer.count;
            oxp_ui_pointer.focus=e==9u?
                (oxp_ui_pointer.focus+1)%(int)n:
                (oxp_ui_pointer.focus<0?0:
                 (oxp_ui_pointer.focus+(int)n-1)%(int)n);
            oxp_ui_pointer.count=0;return OXP_RESIZE;
        }
        if((e==10u||e==13u) && oxp_ui_pointer.focus>=0 &&
           (unsigned)oxp_ui_pointer.focus<oxp_ui_pointer.count){
            oxp_pointer_rect_t r=oxp_ui_pointer.items[oxp_ui_pointer.focus];
            oxp_ui_pointer.count=0;
            return OXP_CLICK|(((r.x+r.w/2u+16u)&1023u)<<10)|
                   ((r.y+r.h/2u+43u)&1023u);
        }
        if(e){oxp_ui_pointer.count=0;return e;}
        if(oxp_ui_refresh_theme()){oxp_ui_pointer.count=0;return OXP_RESIZE;}
    }
}
static inline int oxp_ui_contains(oxp_ui_rect r,unsigned x,unsigned y){
    return r.w && r.h && x>=r.x && y>=r.y && x-r.x<r.w && y-r.y<r.h;
}
static inline int oxp_ui_hit(uint32_t e,oxp_ui_rect r){
    return oxp_hit(e,r.x,r.y,r.w,r.h);
}
static inline unsigned oxp_ui_clamp(unsigned n,unsigned min,unsigned max){
    return n<min?min:n>max?max:n;
}
/* The viewport is the intersection of label rectangle, glyph row and
 * client canvas. Prior versions only bounded text by canvas dimensions,
 * permitting labels to overlap nearby toolbar controls. */
static inline int oxp_ui_bitmap_text(oxp_ui_rect r,const char *s,unsigned top,
                                     uint32_t color,int center,unsigned bold){
    if(!oxp_ui_font.alpha || !s || !r.w || !r.h)return 0;
    unsigned cw=oxp_canvas_width(),ch=oxp_canvas_height();
    if(r.x>=cw||r.y>=ch||top>=ch)return 1;
    unsigned clip=r.w<cw-r.x?r.w:cw-r.x;
    unsigned y0=top>r.y?top:r.y;
    unsigned y1=r.h>ch-r.y?ch:r.y+r.h;
    unsigned font_end=OXP_FONT_CH>ch-top?ch:top+OXP_FONT_CH;
    if(y1>font_end)y1=font_end;
    if(!clip || y1<=y0)return 1;
    oxp_font_fit_t run=oxp_font_fit(&oxp_ui_font,s,bold,clip,92u);
    unsigned inset=center && run.width<clip?(clip-run.width)/2u:0u;
    uint16_t rgb=(uint16_t)((((color>>19u)&31u)<<11u)|
        (((color>>10u)&63u)<<5u)|((color>>3u)&31u));
    for(unsigned tx=0;tx<clip;tx+=OXP_BITMAP_TILE){
        oxp_bitmap_req_t tile={0};
        tile.x=r.x+tx;tile.y=y0;
        tile.w=clip-tx;if(tile.w>OXP_BITMAP_TILE)tile.w=OXP_BITMAP_TILE;
        tile.h=y1-y0;
        unsigned pen=inset,visible=0;
        for(unsigned i=0;i<run.chars+run.dots;++i){
            unsigned char c=i<run.chars?(unsigned char)s[i]:(unsigned char)'.';
            unsigned w=oxp_font_advance(&oxp_ui_font,c,bold);
            if(pen>=tx+tile.w)break;
            if(pen+w>tx && c!=' '){
                unsigned start=tx>pen?tx-pen:0u;
                unsigned end=tx+tile.w-pen;
                if(end>w)end=w;
                if(end>OXP_FONT_CW)end=OXP_FONT_CW;
                for(unsigned py=0;py<tile.h;++py)for(unsigned px=start;px<end;++px){
                    uint8_t a=oxp_font_alpha(&oxp_ui_font,c,bold,px,y0-top+py);
                    if(!a)continue;
                    unsigned off=3u*(py*tile.w+pen+px-tx);
                    tile.pixels[off]=(uint8_t)rgb;
                    tile.pixels[off+1u]=(uint8_t)(rgb>>8);
                    tile.pixels[off+2u]=a;
                    visible=1;
                }
            }
            pen+=w;
        }
        if(visible && oxp_bitmap(&tile)!=0)return 0;
    }
    return 1;
}
static inline void oxp_ui_text(oxp_ui_rect r,const char *s,unsigned y,uint32_t color,int center){
    if(oxp_ui_bitmap_text(r,s,y>=3u?y-3u:0u,color,center,0u))return;
    if(!s||r.w<6u||!r.h)return;
    /* 6-pixel fallback also gets an ellipsis instead of abrupt truncation. */
    char label[92];unsigned count=0,limit=r.w/6u;
    if(limit>=sizeof label)limit=sizeof label-1u;
    while(s[count] && count<limit){label[count]=s[count];++count;}
    if(count==limit && count && s[count]){
        unsigned n=count<3u?count:3u;
        for(unsigned i=0;i<n;++i)label[count-n+i]='.';
    }
    label[count]=0;
    unsigned x=r.x+(center && count*6u<r.w?(r.w-count*6u)/2u:0u);
    oxp_label(x,y,label,1u,color);
}
/* Aurora 6: shared application chrome. All palette values are selected by
 * the current appearance resource; applications no longer bake Blue into
 * toolbars/status strips. Geometry is independent of skin availability. */
static inline void oxp_ui_text_strong(oxp_ui_rect r,const char *s,unsigned y,
                                      uint32_t color,int center){
    if(oxp_ui_bitmap_text(r,s,y>=3u?y-3u:0u,color,center,1u))return;
    oxp_ui_text(r,s,y,color,center);
}
static inline void oxp_ui_clear(void){
    oxp_draw_cmd_t cmd={OXP_DRAW_CLEAR,0,0,0,0,oxp_theme_panel(oxp_ui_theme)};
    (void)oxp_draw(&cmd,1u);
}
static inline uint32_t oxp_ui_ink(void){return oxp_theme_ink(oxp_ui_theme);}
static inline uint32_t oxp_ui_soft_ink(void){
    return oxp_ui_theme==OXP_THEME_MIDNIGHT?0xc3d5f2u:
           oxp_ui_theme==OXP_THEME_OLIVE?0x526745u:0x4d6989u;
}
static inline uint32_t oxp_ui_edit_ink(void){return 0x172d46u;}
static inline void oxp_ui_appbar(unsigned width,const char *title){
    if(width<4u)return;
    unsigned height=26u;
    if(!oxp_ui_blit(&oxp_ui_chrome,0u,0u,192u,26u,0u,0u,width,height,9u,9u,7u,7u)){
        oxp_fill(0,0,width,height,oxp_theme_toolbar(oxp_ui_theme));
        oxp_fill(0,0,width,2u,oxp_theme_accent(oxp_ui_theme));
        oxp_fill(0,height-1u,width,1u,oxp_theme_accent(oxp_ui_theme));
    }
    if(width>18u)oxp_ui_text_strong((oxp_ui_rect){10u,0u,width-18u,height},title,9u,
                             oxp_ui_ink(),0);
}
static inline void oxp_ui_statusbar(unsigned width,unsigned height,const char *status){
    if(width<14u||height<20u)return;
    unsigned top=height-19u;
    if(!oxp_ui_blit(&oxp_ui_chrome,0u,28u,192u,18u,0u,top,width,19u,7u,7u,4u,4u)){
        oxp_fill(0,top,width,19u,oxp_theme_toolbar(oxp_ui_theme));
        oxp_fill(0,top,width,1u,oxp_theme_accent(oxp_ui_theme));
    }
    oxp_ui_text((oxp_ui_rect){8u,top,width-16u,18u},status,top+6u,
                oxp_ui_soft_ink(),0);
}
static inline void oxp_ui_sheet(oxp_ui_rect r){
    if(r.w<4u||r.h<4u)return;
    oxp_fill(r.x,r.y,r.w,r.h,oxp_theme_accent(oxp_ui_theme));
    oxp_fill(r.x+1u,r.y+1u,r.w-2u,r.h-2u,0xffffff);
}
static inline void oxp_ui_panel(oxp_ui_rect r){
    if(r.w<4||r.h<4)return;
    oxp_fill(r.x,r.y,r.w,r.h,oxp_theme_accent(oxp_ui_theme));
    oxp_fill(r.x+1u,r.y+1u,r.w-2u,r.h-2u,0xffffff);
    oxp_fill(r.x+2u,r.y+2u,r.w-4u,r.h-4u,oxp_theme_panel(oxp_ui_theme));
}
static inline void oxp_ui_toolbar(oxp_ui_rect r){
    if(r.w<4||r.h<4)return;
    if(oxp_ui_blit(&oxp_ui_toolbar_art,0u,0u,192u,22u,
                    r.x,r.y,r.w,r.h,7u,7u,4u,4u))return;
    oxp_fill(r.x,r.y,r.w,r.h,oxp_theme_accent(oxp_ui_theme));
    oxp_fill(r.x,r.y,r.w,r.h-2u,oxp_theme_toolbar(oxp_ui_theme));
    oxp_fill(r.x+1u,r.y+1u,r.w-2u,2u,0xffffff);
    oxp_fill(r.x,r.y+r.h-2u,r.w,1u,0xb7c8e0);
}
/* Aurora 8 shared toolbar commands. Geometry and hit testing remain owned
 * by each app; widget artwork is entirely swappable through its PNG atlas. */
static inline void oxp_ui_toolbutton(oxp_ui_rect r,const char *label,int state){
    if(r.w<8u||r.h<8u)return;
    int ui_state=oxp_ui_pointer_style(oxp_ui_record(r));
    if(ui_state>state)state=ui_state;
    unsigned stage=state<=0?0u:state==1?1u:2u;
    if(!oxp_ui_blit(&oxp_ui_toolbar_art,64u*stage,23u,64u,25u,
                    r.x,r.y,r.w,r.h,6u,6u,6u,6u)){
        oxp_fill(r.x,r.y,r.w,r.h,oxp_theme_accent(oxp_ui_theme));
        oxp_fill(r.x+1u,r.y+1u,r.w-2u,r.h-2u,oxp_theme_toolbar(oxp_ui_theme));
    }
    oxp_ui_text_strong((oxp_ui_rect){r.x+3u,r.y,r.w-6u,r.h},label,
                r.y+(r.h>13u?(r.h-7u)/2u:0u),oxp_ui_ink(),1);
}
static inline void oxp_ui_button(oxp_ui_rect r,const char *label,oxp_ui_tone tone,int pressed){
    if(r.w<7||r.h<7)return;
    int interact=oxp_ui_pointer_style(oxp_ui_record(r));
    if(interact==2)pressed=1;
    unsigned state=pressed?2u:interact==1?1u:
                   tone==OXP_UI_NORMAL?0u:1u;
    if(tone!=OXP_UI_DANGER && oxp_ui_blit(&oxp_ui_widgets,state*64u,0u,63u,28u,
                   r.x,r.y,r.w,r.h,6u,6u,6u,6u)){
        if(label)oxp_ui_text((oxp_ui_rect){r.x+3u,r.y,r.w-6u,r.h},label,
            r.y+(r.h-7u)/2u+(pressed?1u:0u),state==0u?0x18365d:0xffffff,1);
        return;
    }
    uint32_t edge=tone==OXP_UI_DANGER?0x9b3249:tone==OXP_UI_PRIMARY?0x215c98:0x7a93b4;
    uint32_t face=tone==OXP_UI_DANGER?0xd55361:tone==OXP_UI_PRIMARY?0x377fc4:0xd7e6f7;
    uint32_t shine=tone==OXP_UI_DANGER?0xff98a0:tone==OXP_UI_PRIMARY?0x85c7f6:0xffffff;
    if(pressed){uint32_t t=face;face=edge;edge=t;}
    oxp_fill(r.x,r.y,r.w,r.h,edge);
    oxp_fill(r.x+1u,r.y+1u,r.w-2u,r.h-2u,face);
    oxp_fill(r.x+2u,r.y+2u,r.w-4u,2u,shine);
    oxp_fill(r.x+2u,r.y+r.h-3u,r.w-4u,1u,tone==OXP_UI_NORMAL?0xb1c8e1:edge);
    if(label)oxp_ui_text((oxp_ui_rect){r.x+3u,r.y,r.w-6u,r.h},label,
        r.y+(r.h-7u)/2u+(pressed?1u:0u),tone==OXP_UI_NORMAL?0x18365d:0xffffff,1);
}
static inline void oxp_ui_textbox(oxp_ui_rect r,const char *value,int focused){
    if(r.w<8||r.h<8)return;
    if(oxp_ui_blit(&oxp_ui_widgets,focused?64u:0u,32u,63u,27u,
                  r.x,r.y,r.w,r.h,4u,4u,4u,4u)){
        if(value)oxp_ui_text((oxp_ui_rect){r.x+5u,r.y,r.w-10u,r.h},value,
                               r.y+(r.h-7u)/2u,0x19334f,0);
        return;
    }
    oxp_fill(r.x,r.y,r.w,r.h,focused?0x2678c6:0x8ba5c1);
    oxp_fill(r.x+1u,r.y+1u,r.w-2u,r.h-2u,0xfdfefe);
    oxp_fill(r.x+2u,r.y+2u,r.w-4u,1u,0xd8e3f3);
    if(value)oxp_ui_text((oxp_ui_rect){r.x+5u,r.y,r.w-10u,r.h},value,
                           r.y+(r.h-7u)/2u,0x19334f,0);
}
static inline void oxp_ui_checkbox(oxp_ui_rect r,const char *label,int checked){
    if(r.w<14||r.h<13)return;
    int hovered=oxp_ui_pointer_style(oxp_ui_record(r));
    if(hovered)oxp_fill(r.x,r.y,r.w,r.h,oxp_theme_toolbar(oxp_ui_theme));
    oxp_fill(r.x,r.y+1u,12u,12u,oxp_theme_accent(oxp_ui_theme));
    oxp_fill(r.x+1u,r.y+2u,10u,10u,0xffffff);
    if(checked){
        oxp_fill(r.x+3u,r.y+7u,3u,2u,oxp_theme_accent(oxp_ui_theme));
        oxp_fill(r.x+5u,r.y+8u,2u,2u,oxp_theme_accent(oxp_ui_theme));
        oxp_fill(r.x+7u,r.y+4u,2u,5u,oxp_theme_accent(oxp_ui_theme));
    }
    if(label && r.w>22u)oxp_ui_text((oxp_ui_rect){r.x+19u,r.y,r.w-19u,r.h},label,r.y+4u,oxp_ui_ink(),0);
}
static inline void oxp_ui_tab(oxp_ui_rect r,const char *label,int selected){
    if(r.w<7||r.h<7)return;
    int hovered=oxp_ui_pointer_style(oxp_ui_record(r));
    if(hovered)selected=1;
    if(oxp_ui_blit(&oxp_ui_widgets,selected?0u:64u,64u,63u,27u,
                  r.x,r.y,r.w,r.h,6u,6u,5u,3u)){
        oxp_ui_text((oxp_ui_rect){r.x+3u,r.y,r.w-6u,r.h},label,
                    r.y+(r.h-7u)/2u,0x254a76,1);
        return;
    }
    oxp_fill(r.x,r.y,r.w,r.h,selected?0x558cc2:0xadc5dd);
    oxp_fill(r.x+1u,r.y+1u,r.w-2u,r.h-1u,selected?0xf7fbff:0xddeaf8);
    oxp_fill(r.x+2u,r.y+1u,r.w-4u,2u,selected?0xffffff:0xeaf3fb);
    oxp_ui_text((oxp_ui_rect){r.x+3u,r.y,r.w-6u,r.h},label,r.y+(r.h-7u)/2u,0x254a76,1);
}
static inline void oxp_ui_progress(oxp_ui_rect r,unsigned value,unsigned max){
    if(r.w<6||r.h<5)return;
    oxp_fill(r.x,r.y,r.w,r.h,0x9ab1cb);
    oxp_fill(r.x+1u,r.y+1u,r.w-2u,r.h-2u,0xffffff);
    unsigned fill=max?((uint64_t)value*(r.w-4u)/max):0u;
    if(fill>r.w-4u)fill=r.w-4u;
    if(fill)oxp_fill(r.x+2u,r.y+2u,fill,r.h-4u,oxp_theme_accent(oxp_ui_theme));
}
static inline void oxp_ui_scrollbar(oxp_ui_rect r,unsigned start,unsigned count,unsigned total){
    if(r.w<8u||r.h<14u)return;
    int pointer=oxp_ui_pointer_style(oxp_ui_record(r));
    if(oxp_ui_blit(&oxp_ui_widgets,55u,97u,8u,27u,
                   r.x,r.y,r.w,r.h,2u,2u,3u,3u)){
        if(!total||!count||total<=count)return;
        unsigned track=r.h-4u;
        unsigned thumb=oxp_ui_clamp((unsigned)((uint64_t)track*count/total),10u,track);
        unsigned range=total-count;
        if(start>range)start=range;
        unsigned y=r.y+2u+(unsigned)((uint64_t)(track-thumb)*start/range);
        (void)oxp_ui_blit(&oxp_ui_widgets,8u,99u,22u,23u,
                          r.x+2u,y,r.w-4u,thumb,3u,3u,4u,4u);
        if(pointer){
            oxp_fill(r.x,r.y,1u,r.h,oxp_theme_accent(oxp_ui_theme));
            if(pointer==2)oxp_fill(r.x+r.w-1u,r.y,1u,r.h,oxp_theme_accent(oxp_ui_theme));
        }
        return;
    }
    oxp_fill(r.x,r.y,r.w,r.h,0xc4d6eb);
    oxp_fill(r.x+1u,r.y+1u,r.w-2u,r.h-2u,0xe8f1fa);
    if(!total || !count || total<=count)return;
    unsigned track=r.h-4u;
    unsigned thumb=oxp_ui_clamp((unsigned)((uint64_t)track*count/total),10u,track);
    unsigned range=total-count;
    if(start>range)start=range;
    unsigned y=r.y+2u+(unsigned)((uint64_t)(track-thumb)*start/range);
    oxp_fill(r.x+2u,y,r.w-4u,thumb,0x6b9fd0);
    oxp_fill(r.x+3u,y+1u,r.w-6u,thumb>2u?2u:1u,0xb4d8f4);
    if(pointer)oxp_fill(r.x,r.y,1u,r.h,oxp_theme_accent(oxp_ui_theme));
}
/* A menu row is an actual input target with hover/press artwork,
 * while the existing application still owns command dispatch. */
static inline void oxp_ui_menu_entry(oxp_ui_rect r,const char *text){
    int state=oxp_ui_pointer_style(oxp_ui_record(r));
    if(state){
        oxp_fill(r.x,r.y,r.w,r.h,oxp_theme_selection(oxp_ui_theme));
        if(state==2 && r.w>2u)oxp_fill(r.x,r.y,r.w,2u,oxp_theme_accent(oxp_ui_theme));
    }
    oxp_ui_text((oxp_ui_rect){r.x+4u,r.y,r.w>8u?r.w-8u:1u,r.h},
                text,r.y+4u,state?0xffffff:oxp_ui_ink(),0);
}
static inline void oxp_ui_menu(oxp_ui_rect r){
    if(r.w<6||r.h<6)return;
    oxp_fill(r.x+2u,r.y+2u,r.w-2u,r.h-2u,0x7992a9);
    oxp_fill(r.x,r.y,r.w-2u,r.h-2u,0x729bc3);
    oxp_fill(r.x+1u,r.y+1u,r.w-4u,r.h-4u,0xffffff);
    oxp_fill(r.x+2u,r.y+2u,20u,r.h-6u,0xd9e9f8);
}
#endif
