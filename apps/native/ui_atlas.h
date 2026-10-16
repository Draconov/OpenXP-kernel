#ifndef OPENXP_UI_ATLAS_H
#define OPENXP_UI_ATLAS_H
/* Aurora 3 bitmap controls. OXUA resources are copied from read-only initrd
 * with the existing bounded FS API; drawing is via syscall 34, which accepts
 * fixed 32x32 RGB565/A8 tiles, never pointers into a framebuffer.
 * The parser checks the whole format before enabling a resource. */
#include "oxp.h"
#include "theme.h"
#include "font_engine.h"
#define OXP_ATLAS_WIDGET_W 192u
#define OXP_ATLAS_WIDGET_H 128u
#define OXP_ATLAS_ICON_W 192u
#define OXP_ATLAS_ICON_H 64u
#define OXP_ATLAS_CHROME_W 192u
#define OXP_ATLAS_CHROME_H 64u
#define OXP_ATLAS_TOOLBAR_W 192u
#define OXP_ATLAS_TOOLBAR_H 64u
typedef struct {unsigned w,h;uint8_t *pixels;unsigned enabled;} oxp_ui_atlas_t;
static uint8_t oxp_ui_widget_bytes[OXP_ATLAS_WIDGET_W*OXP_ATLAS_WIDGET_H*3u];
static uint8_t oxp_ui_icon_bytes[OXP_ATLAS_ICON_W*OXP_ATLAS_ICON_H*3u];
static uint8_t oxp_ui_chrome_bytes[OXP_ATLAS_CHROME_W*OXP_ATLAS_CHROME_H*3u];
static uint8_t oxp_ui_toolbar_bytes[OXP_ATLAS_TOOLBAR_W*OXP_ATLAS_TOOLBAR_H*3u];
static uint8_t oxp_ui_font_blob[OXP_FONT_BYTES];
static oxp_font_t oxp_ui_font;
static int oxp_ui_font_attempted;
/* Initialized at launch, not .data: native flat ELF maps .data read-only.
 * A compile-time pointer initializer would silently reintroduce a PIC/RW
 * page fault on the real Ring 3 loader. */
static oxp_ui_atlas_t oxp_ui_widgets;
static oxp_ui_atlas_t oxp_ui_icons;
static oxp_ui_atlas_t oxp_ui_chrome;
static oxp_ui_atlas_t oxp_ui_toolbar_art;
static unsigned oxp_ui_theme;
static int oxp_ui_initialized;
static inline unsigned oxp_ui_u16(const uint8_t *b){return (unsigned)b[0]|(unsigned)b[1]<<8;}
static inline unsigned oxp_ui_u32(const uint8_t *b){return (unsigned)b[0]|(unsigned)b[1]<<8| (unsigned)b[2]<<16|(unsigned)b[3]<<24;}
static inline int oxp_ui_header_ok(const uint8_t b[16],unsigned w,unsigned h){
    return b[0]=='O'&&b[1]=='X'&&b[2]=='U'&&b[3]=='A'&&
       oxp_ui_u16(b+4)==1u && oxp_ui_u16(b+6)==w &&
       oxp_ui_u16(b+8)==h && oxp_ui_u16(b+10)==0u &&
       oxp_ui_u32(b+12)==w*h*3u;
}
static inline int oxp_ui_load_atlas(oxp_ui_atlas_t *a,const char *path){
    if(!a || !a->pixels)return 0;
    a->enabled=0;
    app_file_request_t req={0};oxp_copy(req.path,path,sizeof req.path);
    req.size=16;
    if(oxp_file(APP_FS_READ_SLICE,&req)!=0||req.size!=16||
       !oxp_ui_header_ok(req.data,a->w,a->h))return 0;
    unsigned total=a->w*a->h*3u;
    for(unsigned off=0;off<total;){
        unsigned n=total-off;if(n>APP_FILE_DATA)n=APP_FILE_DATA;
        req.index=16u+off;req.size=n;
        if(oxp_file(APP_FS_READ_SLICE,&req)!=0||req.size!=n)return 0;
        for(unsigned i=0;i<n;++i)a->pixels[off+i]=req.data[i];
        off+=n;
    }
    a->enabled=1;return 1;
}
/* Read just the stable one-byte preference. Absence is Aurora Blue. */
static inline void oxp_ui_load_font_once(void){
    if(oxp_ui_font_attempted)return;
    oxp_ui_font_attempted=1;
    app_file_request_t req={0};
    oxp_copy(req.path,"windows/resources/aurora-font.oxft",sizeof req.path);
    /* Never trust the size or header alone; parse after reading exactly
     * the entire bounded resource. Corrupt packs use kernel glyph fallback. */
    for(unsigned off=0;off<OXP_FONT_BYTES;){
        unsigned n=OXP_FONT_BYTES-off;
        if(n>APP_FILE_DATA)n=APP_FILE_DATA;
        req.index=off;req.size=n;
        if(oxp_file(APP_FS_READ_SLICE,&req)!=0||req.size!=n)return;
        for(unsigned i=0;i<n;++i)oxp_ui_font_blob[off+i]=req.data[i];
        off+=n;
    }
    (void)oxp_font_parse(&oxp_ui_font,oxp_ui_font_blob,OXP_FONT_BYTES);
}
static inline unsigned oxp_ui_preferred_theme(void){
    app_file_request_t req={0};
    oxp_copy(req.path,"home/appearance.cfg",sizeof req.path);
    if(oxp_file(APP_FS_READ,&req)==0 && req.size==1u &&
       oxp_theme_valid(req.data[0]))return req.data[0];
    return OXP_THEME_BLUE;
}
static inline void oxp_ui_setup(void){
    oxp_ui_widgets.w=OXP_ATLAS_WIDGET_W; oxp_ui_widgets.h=OXP_ATLAS_WIDGET_H;
    oxp_ui_widgets.pixels=oxp_ui_widget_bytes;
    oxp_ui_icons.w=OXP_ATLAS_ICON_W; oxp_ui_icons.h=OXP_ATLAS_ICON_H;
    oxp_ui_icons.pixels=oxp_ui_icon_bytes;
    oxp_ui_chrome.w=OXP_ATLAS_CHROME_W; oxp_ui_chrome.h=OXP_ATLAS_CHROME_H;
    oxp_ui_chrome.pixels=oxp_ui_chrome_bytes;
    oxp_ui_toolbar_art.w=OXP_ATLAS_TOOLBAR_W;
    oxp_ui_toolbar_art.h=OXP_ATLAS_TOOLBAR_H;
    oxp_ui_toolbar_art.pixels=oxp_ui_toolbar_bytes;
}
static inline void oxp_ui_load_theme(unsigned theme){
    oxp_ui_setup();
    (void)oxp_ui_load_atlas(&oxp_ui_widgets,oxp_theme_widgets_path(theme));
    (void)oxp_ui_load_atlas(&oxp_ui_icons,oxp_theme_icons_path(theme));
    (void)oxp_ui_load_atlas(&oxp_ui_chrome,oxp_theme_chrome_path(theme));
    (void)oxp_ui_load_atlas(&oxp_ui_toolbar_art,oxp_theme_toolbar_path(theme));
    oxp_ui_load_font_once();
    oxp_ui_theme=theme; oxp_ui_initialized=1;
}
static inline int oxp_ui_refresh_theme(void){
    unsigned theme=oxp_ui_preferred_theme();
    if(oxp_ui_initialized && theme==oxp_ui_theme)return 0;
    oxp_ui_load_theme(theme);
    return 1;
}
static inline void oxp_ui_pointer_init(void);
static inline void oxp_ui_atlas_init_widgets(void){
    (void)oxp_ui_refresh_theme();
    oxp_ui_pointer_init();
}
static inline void oxp_ui_atlas_init_icons(void){
    (void)oxp_ui_refresh_theme();
}
/* Deterministic 9-slice sampling; shrinking below border sum scales entire
 * source, avoiding underflow in ultra-small windows. */
static inline unsigned oxp_ui_axis(unsigned at,unsigned target,unsigned source,
                                    unsigned left,unsigned right){
    if(target<=left+right)return (unsigned)((uint64_t)at*source/target);
    if(at<left)return at;
    if(at>=target-right)return source-(target-at);
    return left+(unsigned)((uint64_t)(at-left)*(source-left-right)/(target-left-right));
}
static inline int oxp_ui_blit(const oxp_ui_atlas_t *a,
       unsigned sx,unsigned sy,unsigned sw,unsigned sh,
       unsigned dx,unsigned dy,unsigned dw,unsigned dh,
       unsigned left,unsigned right,unsigned top,unsigned bottom){
    if(!a||!a->enabled||!sw||!sh||!dw||!dh||
       sx>=a->w||sy>=a->h||sw>a->w-sx||sh>a->h-sy||
       left+right>=sw||top+bottom>=sh)return 0;
    unsigned cw=oxp_canvas_width(),ch=oxp_canvas_height();
    if(dx>=cw||dy>=ch)return 1;
    unsigned visw=dw>cw-dx?cw-dx:dw;
    unsigned vish=dh>ch-dy?ch-dy:dh;
    for(unsigned yy=0;yy<vish;yy+=OXP_BITMAP_TILE){
        unsigned th=vish-yy;if(th>OXP_BITMAP_TILE)th=OXP_BITMAP_TILE;
        for(unsigned xx=0;xx<visw;xx+=OXP_BITMAP_TILE){
            unsigned tw=visw-xx;if(tw>OXP_BITMAP_TILE)tw=OXP_BITMAP_TILE;
            /* Bounded ~3KiB user stack allocation. Every byte that the
             * kernel copies is initialized, including unused tail. */
            oxp_bitmap_req_t tile={0};
            tile.x=dx+xx;tile.y=dy+yy;tile.w=tw;tile.h=th;
            for(unsigned py=0;py<th;++py){
                unsigned iy=oxp_ui_axis(yy+py,dh,sh,top,bottom);
                for(unsigned px=0;px<tw;++px){
                    unsigned ix=oxp_ui_axis(xx+px,dw,sw,left,right);
                    unsigned si=3u*((sy+iy)*a->w+sx+ix);
                    unsigned di=3u*(py*tw+px);
                    tile.pixels[di]=a->pixels[si];
                    tile.pixels[di+1]=a->pixels[si+1];
                    tile.pixels[di+2]=a->pixels[si+2];
                }
            }
            if(oxp_bitmap(&tile)!=0)return 0;
        }
    }
    return 1;
}
static inline int oxp_ui_icon(unsigned which,unsigned x,unsigned y,unsigned pixels){
    if(which>=12u||!pixels)return 0;
    return oxp_ui_blit(&oxp_ui_icons,(which%6u)*32u,(which/6u)*32u,32u,32u,
                        x,y,pixels,pixels,0,0,0,0);
}
#endif
