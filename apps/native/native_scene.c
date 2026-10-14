#include "native_scene.h"
#include "shell_raster.h"
#include "../../kernel/gui/native_viewport.h"
#include <stddef.h>
#define W OXP_SHELL_RASTER_W
#define H OXP_SHELL_RASTER_H
#define KEY OXP_SHELL_TRANSPARENT
static void cmd(oxp_shell_draw_t *c,uint32_t op,unsigned x,unsigned y,unsigned w,unsigned h,uint32_t rgb){
    *c=(oxp_shell_draw_t){.op=op,.x=x,.y=y,.w=w,.h=h,.color=rgb};
}
static void caption(oxp_shell_draw_t *c,unsigned x,unsigned y,const char *s){
    cmd(c,OXP_SHELL_TEXT,x,y,1,0,0xffffffu);
    unsigned i=0;
    while(i<OXP_SHELL_TEXT_MAX-1u && s[i] && (unsigned char)s[i]>=32u && (unsigned char)s[i]<=126u){c->text[i]=s[i];++i;}
}
static void draw_window_row(const oxp_shell_window_t *w,const oxp_native_info_t *info,
                            const uint16_t *surface,unsigned y,uint16_t *dst){
    if(!w->visible || w->minimized || !info->active ||
       !w->width || !w->height || w->x>=W || w->y>=H)return;
    oxp_shell_draw_t cmds[18]={0};
    unsigned n=0;
    cmd(&cmds[n++],OXP_SHELL_RECT,w->x+4u,w->y+5u,w->width,w->height,0x12405au);
    cmd(&cmds[n++],OXP_SHELL_RECT,w->x,w->y,w->width,w->height,0x2a518fu);
    if(w->width>2u && w->height>2u)
        cmd(&cmds[n++],OXP_SHELL_RECT,w->x+1u,w->y+1u,w->width-2u,w->height-2u,0xdee7f3u);
    if(w->width>6u && w->height>33u)
        cmd(&cmds[n++],OXP_SHELL_RECT,w->x+3u,w->y+3u,w->width-6u,28u,
            w->active?0x1158c3u:0x617695u);
    caption(&cmds[n++],w->x+13u,w->y+11u,info->title);
    if(w->width>60u){
        cmd(&cmds[n++],OXP_SHELL_RECT,w->x+w->width-28u,w->y+6u,21u,21u,0xc03f3du);
        caption(&cmds[n++],w->x+w->width-22u,w->y+13u,"X");
    }
    if(w->width>88u){
        cmd(&cmds[n++],OXP_SHELL_RECT,w->x+w->width-80u,w->y+6u,21u,21u,0x4f85cbu);
        caption(&cmds[n++],w->x+w->width-74u,w->y+13u,"-");
        cmd(&cmds[n++],OXP_SHELL_RECT,w->x+w->width-54u,w->y+6u,21u,21u,0x4f85cbu);
        /* Draw familiar outline squares instead of text glyphs. */
        if(w->maximized){
            cmd(&cmds[n++],OXP_SHELL_RECT,w->x+w->width-45u,w->y+11u,10u,9u,0xffffffu);
            cmd(&cmds[n++],OXP_SHELL_RECT,w->x+w->width-44u,w->y+13u,8u,6u,0x4f85cbu);
            cmd(&cmds[n++],OXP_SHELL_RECT,w->x+w->width-49u,w->y+14u,10u,9u,0xffffffu);
            cmd(&cmds[n++],OXP_SHELL_RECT,w->x+w->width-48u,w->y+16u,8u,6u,0x4f85cbu);
        }else{
            cmd(&cmds[n++],OXP_SHELL_RECT,w->x+w->width-48u,w->y+12u,11u,10u,0xffffffu);
            cmd(&cmds[n++],OXP_SHELL_RECT,w->x+w->width-47u,w->y+14u,9u,7u,0x4f85cbu);
        }
    }
    if(w->width>12u && w->height>40u)
        cmd(&cmds[n++],OXP_SHELL_RECT,w->x+6u,w->y+34u,w->width-12u,w->height-40u,0xfafcffu);
    uint16_t row[W];
    oxp_raster_row(cmds,n,OXP_SHELL_LAYER_OVERLAY,y,row);
    for(unsigned x=0;x<W;++x)if(row[x]!=KEY)dst[x]=row[x];
    /* Framebuffer scanout performs nearest-neighbour scaling of the native
     * logical canvas. The viewport is centred with undistorted pixels, while
     * unused client space retains the themed background. */
    oxp_native_viewport_t view=oxp_native_viewport(w->x,w->y,w->width,w->height);
    if(view.w && y>=view.y && y-view.y<view.h && view.x<W){
        oxp_native_canvas_t canvas=oxp_native_canvas(w->width,w->height);
        unsigned sy=(unsigned)((uint64_t)(y-view.y)*canvas.h/view.h);
        unsigned columns=view.w;
        if(columns>W-view.x)columns=W-view.x;
        if(info->painted){
            const uint16_t *src=surface+sy*OXP_NATIVE_W;
            unsigned sx=0,remainder=0;
            for(unsigned dx=0;dx<columns;++dx){
                dst[view.x+dx]=src[sx];
                remainder+=canvas.w;
                while(remainder>=view.w){remainder-=view.w;++sx;}
            }
        }
    }
    if(!info->painted && y>=w->y+50u && y<w->y+50u+18u*13u &&
       y+6u<w->y+w->height && w->x+16u<W){
        /* Preserve the text-only client's original readable glyph layout.
         * Unlike a pixel surface, these clients do not own a virtual canvas. */
        unsigned rowindex=(y-w->y-50u)/13u;
        const char *cursor=info->text;
        for(unsigned k=0;k<rowindex && *cursor;++k){
            while(*cursor && *cursor!='\n')++cursor;
            if(*cursor=='\n')++cursor;
        }
        oxp_shell_draw_t line={0};
        char txt[OXP_SHELL_TEXT_MAX]={0};unsigned j=0;
        while(cursor[j] && cursor[j]!='\n' && j<OXP_SHELL_TEXT_MAX-1u){txt[j]=cursor[j];++j;}
        caption(&line,w->x+16u,w->y+50u+rowindex*13u,txt);
        line.color=0x1b3159u;
        oxp_raster_row(&line,1,OXP_SHELL_LAYER_OVERLAY,y,row);
        for(unsigned x=0;x<W;++x)if(row[x]!=KEY)dst[x]=row[x];
    }
    if(!w->maximized && w->width>=30u && w->height>=30u &&
       y>=w->y+w->height-12u && y<w->y+w->height-5u){
        /* Conventional bottom-right resize grip: stays independent of the
         * application's letterboxed drawing surface. */
        unsigned dx=w->x+w->width-13u;
        if(dx<W)for(unsigned i=0;i<3u;++i){
            unsigned px=dx+i*4u;
            if(px<W && y>=w->y+w->height-11u+i*3u)dst[px]=0x4a69u;
        }
    }
}
void oxp_native_scene_row(const oxp_shell_window_t windows[OXP_NATIVE_SLOTS],
                          const oxp_native_info_t infos[OXP_NATIVE_SLOTS],
                          const uint16_t surfaces[OXP_NATIVE_SLOTS][OXP_NATIVE_W*OXP_NATIVE_H],
                          unsigned y,uint16_t dst[W]){
    if(!dst || y>=H)return;
    for(unsigned x=0;x<W;++x)dst[x]=KEY;
    if(!windows || !infos || !surfaces)return;
    /* Z-order metadata from the kernel shell policy is immutable to apps. */
    for(unsigned order=0;order<OXP_NATIVE_SLOTS;++order){
        unsigned pick=OXP_NATIVE_SLOTS;
        for(unsigned i=0;i<OXP_NATIVE_SLOTS;++i){
            unsigned rank=0;
            for(unsigned j=0;j<OXP_NATIVE_SLOTS;++j)
                if(windows[j].z < windows[i].z ||
                   (windows[j].z==windows[i].z && j<i))++rank;
            if(rank==order){pick=i;break;}
        }
        if(pick<OXP_NATIVE_SLOTS)
            draw_window_row(&windows[pick],&infos[pick],surfaces[pick],y,dst);
    }
}
