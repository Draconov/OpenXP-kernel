#include "oxp.h"
#include "wm_protocol.h"
#include "file_workflow.h"
#include "theme.h"
#include "wallpaper.h"
#include "shell_raster.h"
#include "native_scene.h"
#include "skin.h"
#include "shell_icons.h"
#include "shell_menu_art.h"
#include "shell_layout.h"
#include "shell_interaction.h"
#include "../../kernel/user/wm_display.h"
#include "../../kernel/user/shell_abi.h"
#include "../../kernel/input/input.h"

/* Persistent, single-threaded userspace WM policy server. The kernel handles
 * per-PID surfaces and low-level composition; no raw framebuffer is exposed.
 * A connection owns at most one logical window until it closes. */
#define WM_SERVICE_CHANNELS WM_MAX_WINDOWS
static wm_state_t state;
static uint64_t channels[WM_SERVICE_CHANNELS];
static uint32_t shell_theme;
static uint8_t shell_font_blob[OXP_FONT_BYTES];
static oxp_font_t shell_font;
static int previous_valid;
static int native_scene_valid;
/* Decoded, downsampled RGB565 image; no raw framebuffer mapping in Ring 3. */
static uint16_t wallpaper_pixels[OXP_WALL_W*OXP_WALL_H];
static int wallpaper_active;
static unsigned last_wallpaper_generation,wallpaper_generation;
static unsigned wallpaper_ordinal;
static char wallpaper_path[96];
/* Built-in OXSK assets live in the read-only initrd.  Missing/corrupt
 * resources fall back to the old procedural UI; never prevent shell boot. */
static uint8_t skin_blob[OXP_SKIN_MAX_BYTES];
static oxp_skin_t skin;
static int skin_loaded;
static int32_t skin_mouse_x,skin_mouse_y;
static int skin_pressed;
static int skin_input_changed;
static int skin_last_hover;
/* The 36 KiB OXUA icon atlas is kept read-only by convention after loading.
 * It lives in the WM's bounded RW memory, not in privileged kernel memory. */
static uint8_t desktop_icons_blob[OXP_SHELL_ICON_BYTES];
static oxp_shell_icons_t desktop_icons;
static int icons_loaded;
static uint8_t menu_art_blob[OXP_MENU_ART_BYTES];
static oxp_menu_art_t menu_art;
static int menu_art_loaded;
static oxp_shell_window_t shell_windows[17];
static int menu_visible;
static uint32_t menu_cursor;
static int desktop_hover;
static unsigned desktop_icons_enabled;
static char dragged_document[52];
static uint64_t drag_document_tick;
static int drag_document_x,drag_document_y,drag_document_active;
static int menu_hover;
/* Hover/press states are transient and never persisted with appearance packs.
 * A release on another control cancels the original press. */
static int start_hover,task_hover;
static int shell_press_target;
static unsigned start_glow;
static int background_visual_dirty,overlay_visual_dirty;

static int selected_wallpaper_path(unsigned index,char *path){
    for(unsigned i=0,match=0;i<32u;++i){
        app_file_request_t request={0};
        oxp_copy(request.path,"windows/web/wallpaper",sizeof request.path);
        request.dest[0]='D';request.index=i;
        if(oxp_file(APP_FS_LIST,&request))break;
        const char *str=request.path;unsigned n=0;
        while(str[n])++n;
        if(n>=4 && str[n-4]=='.' && str[n-3]=='b' && str[n-2]=='m' && str[n-1]=='p'){
            if(match++==index){oxp_copy(path,str,96);return 1;}
        }
    }
    return 0;
}
static int bmp_reader(void *context,uint32_t offset,void *out,uint32_t count){
    const char *path=(const char *)context;
    uint8_t *dst=out;
    while(count){
        app_file_request_t req={0};oxp_copy(req.path,path,sizeof req.path);
        uint32_t amount=count>APP_FILE_DATA?APP_FILE_DATA:count;
        req.index=offset;req.size=amount;
        if(oxp_file(APP_FS_READ_SLICE,&req)||req.size!=amount)return 0;
        for(uint32_t i=0;i<amount;++i)dst[i]=req.data[i];
        offset+=amount;dst+=amount;count-=amount;
    }
    return 1;
}
static void load_shell_font(void){
    const char *path="windows/resources/aurora-font.oxft";
    if(bmp_reader((void *)path,0,shell_font_blob,sizeof shell_font_blob) &&
       oxp_font_parse(&shell_font,shell_font_blob,sizeof shell_font_blob))
        oxp_raster_set_font(&shell_font);
    else oxp_raster_set_font(0);
}
static int load_shell_icons(unsigned theme){
    const char *path=oxp_theme_icons_path(theme);
    icons_loaded=bmp_reader((void *)path,0,desktop_icons_blob,OXP_SHELL_ICON_BYTES) &&
                 oxp_shell_icons_parse(&desktop_icons,desktop_icons_blob,
                                       sizeof desktop_icons_blob);
    return icons_loaded;
}
static int load_menu_art(unsigned theme){
    const char *path=oxp_theme_menu_path(theme);
    menu_art_loaded=bmp_reader((void *)path,0,menu_art_blob,OXP_MENU_ART_BYTES) &&
        oxp_menu_art_parse(&menu_art,menu_art_blob,sizeof menu_art_blob);
    if(!menu_art_loaded)menu_art.pixels=0;
    return menu_art_loaded;
}
static int load_aurora_skin(unsigned theme){
    const char *path=oxp_theme_skin_path(theme);
    skin_loaded=oxp_skin_load(&skin,skin_blob,bmp_reader,(void *)path);
    return skin_loaded;
}
/* Only mutate current pack after checking requested replacement. If a
 * resource is unexpectedly broken, restore the previous good selection. */
static int use_theme_art(unsigned requested){
    unsigned original=shell_theme;
    if(!load_aurora_skin(requested) || !load_shell_icons(requested)){
        (void)load_aurora_skin(original);
        (void)load_shell_icons(original);
        return 0;
    }
    /* Menu artwork is optional: corruption never blocks theme changes. */
    (void)load_menu_art(requested);
    shell_theme=requested;
    /* Same window geometry, but new pixel colors; invalidate both caches. */
    previous_valid=0;native_scene_valid=0;
    return 1;
}
static int apply_wallpaper(unsigned ordinal){
    char path[96];if(!selected_wallpaper_path(ordinal,path))return 0;
    /* On failure do not change selected wallpaper. */
    if(!oxp_wall_decode(bmp_reader,path,wallpaper_pixels))return 0;
    wallpaper_active=1;wallpaper_ordinal=ordinal;
    ++wallpaper_generation;
    oxp_copy(wallpaper_path,path,sizeof wallpaper_path);
    return 1;
}


static int apply_window(uint64_t channel,const wm_packet_t *request,
                        const wm_packet_t *reply){
    openxp_wm_display_cmd_t command={0};
    switch(request->operation){
    case WM_CREATE: case WM_MOVE:
        command=(openxp_wm_display_cmd_t){OPENXP_WM_SET_RECT,
            reply->x,reply->y,reply->width,reply->height};break;
    case WM_FOCUS:command.action=OPENXP_WM_RAISE;break;
    case WM_CLOSE:command.action=OPENXP_WM_REQUEST_CLOSE;break;
    default:return 0;
    }
    return (int64_t)oxp_call3(20,channel,(uint64_t)&command,sizeof(command))==0?0:-1;
}
static void drop_channel(unsigned slot){
    uint64_t channel=channels[slot];
    if(!channel)return;
    wm_state_disconnect(&state,channel);
    (void)oxp_close(channel);
    channels[slot]=0;
}
static int accept_pending(void){
    int accepted=0;
    for(;;){
        int64_t handle=oxp_accept();
        if(handle==-11)return accepted;
        if(handle<=0)return -1;
        unsigned slot=0;
        while(slot<WM_SERVICE_CHANNELS && channels[slot])++slot;
        if(slot==WM_SERVICE_CHANNELS){
            /* Reject unserviceable connections without retaining endpoints. */
            (void)oxp_close((uint64_t)handle);
            ++accepted;
            continue;
        }
        channels[slot]=(uint64_t)handle;
        ++accepted;
    }
}
/* At most one request per client per iteration: a talkative app cannot starve
 * another one. The kernel's any-channel wait handles both messages and close. */
static int service_one(unsigned slot){
    uint64_t channel=channels[slot];
    if(!channel)return 0;
    uint8_t wire[64]={0};
    int64_t count=oxp_recv(channel,wire,sizeof wire);
    wm_packet_t request={0};
    if(count==(int64_t)sizeof request){
        for(unsigned i=0;i<sizeof request;++i)((uint8_t *)&request)[i]=wire[i];
    }
    if(count==-11)return 0;
    if(count<=0){drop_channel(slot);return 1;}
    wm_packet_t reply={0};
    if(count==(int64_t)sizeof(wm_drag_packet_t)){
        wm_drag_packet_t packet={0};
        for(unsigned i=0;i<sizeof packet;++i)((uint8_t *)&packet)[i]=wire[i];
        reply=(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,
            .operation=WM_DRAG_BEGIN|WM_REPLY_FLAG,.status=WM_BAD_REQUEST};
        int terminated=0;
        for(unsigned i=0;i<sizeof packet.path;++i)if(!packet.path[i])terminated=1;
        if(terminated && packet.magic==WM_MAGIC && packet.version==WM_VERSION &&
           packet.operation==WM_DRAG_BEGIN){
            app_file_request_t check={0};
            oxp_copy(check.path,packet.path,sizeof check.path);
            int64_t kind=oxp_file(APP_FS_KIND,&check);
            if((kind==1 || kind==2) && oxp_doc_program(packet.path)){
                oxp_copy(dragged_document,packet.path,sizeof dragged_document);
                drag_document_tick=oxp_ticks();
                drag_document_x=skin_mouse_x;drag_document_y=skin_mouse_y;
                drag_document_active=1;
                reply.status=WM_OK;
            }
        }
    }else if(count==(int64_t)sizeof(request) && request.magic==WM_MAGIC &&
       request.version==WM_VERSION && request.operation==WM_THEME){
        reply=(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,
            .operation=WM_THEME|WM_REPLY_FLAG,.status=WM_BAD_REQUEST};
        unsigned prior=shell_theme;
        if(oxp_theme_valid(request.x) && use_theme_art(request.x)){
            if((int64_t)oxp_call(23,OXP_SHELL_THEME,request.x)==0){
                wallpaper_active=0;
                ++wallpaper_generation;
                reply.status=WM_OK;
            }else (void)use_theme_art(prior);
        }
    }else if(count==(int64_t)sizeof(request) && request.magic==WM_MAGIC &&
             request.version==WM_VERSION && request.operation==WM_WALLPAPER){
        reply=(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,
            .operation=WM_WALLPAPER|WM_REPLY_FLAG,.status=WM_BAD_REQUEST};
        if(request.x<32u && apply_wallpaper(request.x))reply.status=WM_OK;
    }else if(count==(int64_t)sizeof(request) && request.magic==WM_MAGIC &&
             request.version==WM_VERSION && request.operation==WM_DESKTOP){
        reply=(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,
            .operation=WM_DESKTOP|WM_REPLY_FLAG,.status=WM_BAD_REQUEST};
        if(request.x<=1u){
            desktop_icons_enabled=request.x;
            desktop_hover=-1;
            background_visual_dirty=1;previous_valid=0;native_scene_valid=0;
            reply.status=WM_OK;
        }
    }else if(count==(int64_t)sizeof(request)) {
        wm_packet_t before={.magic=WM_MAGIC,.version=WM_VERSION,
            .operation=WM_QUERY,.window_id=request.window_id};
        wm_packet_t prior={0};
        /* Save the prior rectangle for MOVE rollback if the kernel rejects
         * the update (e.g. the actual client window disappeared). */
        if(request.operation==WM_MOVE)
            wm_state_process(&state,channel,&before,&prior);
        wm_state_process(&state,channel,&request,&reply);
        if(reply.status==WM_OK && apply_window(channel,&request,&reply)) {
            if(request.operation==WM_CREATE) {
                wm_packet_t rollback={.magic=WM_MAGIC,.version=WM_VERSION,
                    .operation=WM_CLOSE,.window_id=reply.window_id},ignored={0};
                wm_state_process(&state,channel,&rollback,&ignored);
            }else if(request.operation==WM_MOVE && prior.status==WM_OK){
                wm_packet_t rollback={.magic=WM_MAGIC,.version=WM_VERSION,
                    .operation=WM_MOVE,.window_id=reply.window_id,
                    .x=prior.x,.y=prior.y},ignored={0};
                wm_state_process(&state,channel,&rollback,&ignored);
            }
            reply.status=WM_UNAUTHORIZED;
        }
    }else{
        /* Malformed packet must not be interpreted as a valid syscall. */
        reply=(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,
            .operation=WM_REPLY_FLAG,.status=WM_BAD_REQUEST};
    }
    int64_t sent=oxp_send(channel,&reply,sizeof(reply));
    if(sent!=(int64_t)sizeof(reply))drop_channel(slot);
    return 1;
}
/* The native shell, including Start and taskbar layout, lives in Ring 3.
 * The privileged kernel validates the display list and only blits pixels.
 * Legacy desktop windows remain available while their apps are ported. */
#define SHELL_W 1024u
#define SHELL_H 768u
#define SHELL_TASK_TOP 726u
static oxp_shell_draw_t shell_frame[OXP_SHELL_DRAW_MAX];
static unsigned drawn;
static int frame_overflow;
static oxp_shell_raster_row_t raster_row;
static oxp_shell_draw_t previous_frame[OXP_SHELL_DRAW_MAX];
static unsigned previous_count;
static int previous_valid;
/* Compare each drawing layer independently: dragging windows should not
 * upload a new wallpaper or taskbar if their pixels have not changed. */
static int layer_changed(unsigned layer){
    if(layer==0 && wallpaper_generation!=last_wallpaper_generation)return 1;
    if(!previous_valid)return 1;
    unsigned a=0,b=0;
    for(;;){
        while(a<drawn){
            unsigned op=shell_frame[a].op;
            if((layer==0 && (op==OXP_SHELL_BG_RECT || op==OXP_SHELL_BG_TEXT)) ||
               (layer==1 && (op==OXP_SHELL_RECT || op==OXP_SHELL_TEXT)))break;
            ++a;
        }
        while(b<previous_count){
            unsigned op=previous_frame[b].op;
            if((layer==0 && (op==OXP_SHELL_BG_RECT || op==OXP_SHELL_BG_TEXT)) ||
               (layer==1 && (op==OXP_SHELL_RECT || op==OXP_SHELL_TEXT)))break;
            ++b;
        }
        if(a>=drawn || b>=previous_count)return (a<drawn)!=(b<previous_count);
        const uint8_t *x=(const uint8_t *)&shell_frame[a];
        const uint8_t *y=(const uint8_t *)&previous_frame[b];
        for(unsigned k=0;k<sizeof(oxp_shell_draw_t);++k)
            if(x[k]!=y[k])return 1;
        ++a;++b;
    }
}
/* The same placements are used for artwork and hit-testing, so swapping an
 * icon sprite never changes which action the user activates. */
static void draw_shell_icon_rows(unsigned layer,unsigned y,uint16_t *pixels){
    if(!icons_loaded)return;
    if(layer==0){
        if(!desktop_icons_enabled)return;
        for(unsigned i=0;i<OXP_DESKTOP_COUNT;++i)
            oxp_shell_icon_row(&desktop_icons,oxp_desktop_icon(i),
                OXP_DESKTOP_X+32u,OXP_DESKTOP_TOP+i*OXP_DESKTOP_STEP+10u,
                36u,y,pixels,OXP_SHELL_RASTER_W);
    }else{
        if(menu_visible){
            for(unsigned i=0;i<OXP_MENU_ITEMS;++i){
                unsigned xx=OXP_MENU_ROW_X+(i/7u)*OXP_MENU_COLUMN_W;
                unsigned yy=OXP_MENU_ROW_Y+(i%7u)*OXP_MENU_ROW_STEP;
                oxp_shell_icon_row(&desktop_icons,oxp_menu_icon(i),
                                   xx+7u,yy+3u,31u,y,pixels,OXP_SHELL_RASTER_W);
            }
        }
        unsigned slot=0,count=0;
        for(unsigned i=0;i<17u;++i)if(shell_windows[i].visible)++count;
        unsigned bw=count?(SHELL_W-216u)/count:1u;
        if(bw>142u)bw=142u;
        if(!bw)bw=1u;
        for(unsigned i=0;i<17u;++i){
            if(!shell_windows[i].visible)continue;
            unsigned bx=115u+slot++*bw;
            if(bw>=31u)
                oxp_shell_icon_row(&desktop_icons,(i<14u?oxp_menu_icon(i):11u),
                                   bx+5u,SHELL_TASK_TOP+12u,18u,y,pixels,
                                   OXP_SHELL_RASTER_W);
        }
    }
}
static int publish_pixel_layers(unsigned mask){
    for(unsigned layer=0;layer<2;++layer){
        if(!(mask&(1u<<layer)))continue;
        raster_row.layer=layer;
        for(unsigned y=0;y<OXP_SHELL_RASTER_H;++y){
            raster_row.y=y;
            if(layer==0 && wallpaper_active && y<SHELL_TASK_TOP){
                unsigned sy=(y*OXP_WALL_H)/SHELL_TASK_TOP;
                for(unsigned x=0;x<OXP_SHELL_RASTER_W;++x)
                    raster_row.pixels[x]=wallpaper_pixels[sy*OXP_WALL_W+
                                                       x*OXP_WALL_W/OXP_SHELL_RASTER_W];
                oxp_raster_overlay_background(shell_frame,drawn,y,raster_row.pixels);
            }else if(layer==1 && skin_loaded && y>=SHELL_TASK_TOP){
                for(unsigned x=0;x<OXP_SHELL_RASTER_W;++x)
                    raster_row.pixels[x]=OXP_SHELL_TRANSPARENT;
                (void)oxp_skin_nine_row(&skin,OXP_SKIN_TASKBAR,0u,SHELL_TASK_TOP,
                    SHELL_W,42u,y,raster_row.pixels,OXP_SHELL_RASTER_W);
                (void)oxp_skin_nine_row(&skin,OXP_SKIN_START,2u,SHELL_TASK_TOP+4u,
                    105u,35u,y,raster_row.pixels,OXP_SHELL_RASTER_W);
                oxp_raster_over_row(shell_frame,drawn,layer,y,raster_row.pixels);
            }else if(layer==1u && menu_visible && menu_art_loaded &&
                     y>=OXP_MENU_Y && y<OXP_MENU_Y+OXP_MENU_H){
                for(unsigned x=0;x<OXP_SHELL_RASTER_W;++x)
                    raster_row.pixels[x]=OXP_SHELL_TRANSPARENT;
                (void)oxp_menu_art_row(&menu_art,OXP_MENU_ART_PANEL,
                      OXP_MENU_X,OXP_MENU_Y,OXP_MENU_W,OXP_MENU_H,y,
                      raster_row.pixels,OXP_SHELL_RASTER_W);
                (void)oxp_menu_art_row(&menu_art,OXP_MENU_ART_HEADER,
                      OXP_MENU_X+2u,OXP_MENU_Y+2u,OXP_MENU_W-4u,47u,y,
                      raster_row.pixels,OXP_SHELL_RASTER_W);
                (void)oxp_menu_art_row(&menu_art,OXP_MENU_ART_FOOTER,
                      OXP_MENU_X+2u,OXP_MENU_Y+338u,OXP_MENU_W-4u,29u,y,
                      raster_row.pixels,OXP_SHELL_RASTER_W);
                for(unsigned i=0;i<OXP_MENU_ITEMS;++i){
                    unsigned xx=OXP_MENU_ROW_X+(i/7u)*OXP_MENU_COLUMN_W;
                    unsigned yy=OXP_MENU_ROW_Y+(i%7u)*OXP_MENU_ROW_STEP;
                    if(menu_cursor==i || menu_hover==(int)i)
                        (void)oxp_menu_art_row(&menu_art,menu_cursor==i?
                            OXP_MENU_ART_SELECTED:OXP_MENU_ART_HOVER,
                            xx,yy,234u,37u,y,raster_row.pixels,OXP_SHELL_RASTER_W);
                }
                oxp_raster_over_row(shell_frame,drawn,layer,y,raster_row.pixels);
            }else oxp_raster_row(shell_frame,drawn,layer,y,raster_row.pixels);
            if(layer==0u && menu_art_loaded && desktop_hover>=0){
                unsigned yy=OXP_DESKTOP_TOP+(unsigned)desktop_hover*OXP_DESKTOP_STEP;
                if(y>=yy+54u && y<yy+83u){
                    (void)oxp_menu_art_row(&menu_art,OXP_MENU_ART_DESKTOP_LABEL,
                          OXP_DESKTOP_X+3u,yy+54u,OXP_DESKTOP_SLOT_W-6u,29u,y,
                          raster_row.pixels,OXP_SHELL_RASTER_W);
                    oxp_raster_background_text_row(shell_frame,drawn,y,raster_row.pixels);
                }
            }
            draw_shell_icon_rows(layer,y,raster_row.pixels);
            if((int64_t)oxp_call3(29,(uint64_t)&raster_row,sizeof(raster_row),0)!=0)
                return -1;
        }
    }
    return (int64_t)oxp_call(30,mask,0)==0?0:-1;
}

/* Windowd now constructs native window pixels, not just their order.
 * Static data lives in the service's private writable ELF segment. */
static oxp_native_info_t native_infos[OXP_NATIVE_SLOTS];
static oxp_native_info_t old_native_infos[OXP_NATIVE_SLOTS];
static oxp_shell_window_t old_native_windows[OXP_NATIVE_SLOTS];
static uint16_t native_buffers[OXP_NATIVE_SLOTS][OXP_NATIVE_W*OXP_NATIVE_H];
static oxp_native_row_t native_scanline;
static int native_scene_valid;
static int bytes_differ(const void *left,const void *right,size_t size){
    const uint8_t *a=left,*b=right;
    for(size_t i=0;i<size;++i)if(a[i]!=b[i])return 1;
    return 0;
}
static int publish_native_scene(void){
    int changed=!native_scene_valid || skin_input_changed;
    oxp_shell_window_t views[OXP_NATIVE_SLOTS];
    for(unsigned i=0;i<OXP_NATIVE_SLOTS;++i){
        int64_t ret=(int64_t)oxp_call3(31,i,(uint64_t)&native_infos[i],sizeof(native_infos[i]));
        if(ret)return -1;
        views[i]=shell_windows[14u+i];
        if(bytes_differ(&native_infos[i],&old_native_infos[i],sizeof(native_infos[i])) ||
           bytes_differ(&views[i],&old_native_windows[i],sizeof(views[i])))changed=1;
        if(native_infos[i].active && native_infos[i].painted &&
           (!native_scene_valid || old_native_infos[i].generation!=native_infos[i].generation ||
            !old_native_infos[i].painted)){
            for(unsigned y=0;y<native_infos[i].surface_h && y<OXP_NATIVE_H;++y){
                /* An app can exit while its pixels are being captured. This
                 * is a normal lifecycle race, not a reason to kill windowd. */
                if((int64_t)oxp_call3(32,i,(uint64_t)&native_scanline,y)){
                    native_infos[i].active=0;
                    native_infos[i].painted=0;
                    native_infos[i].generation=0;
                    changed=1;
                    break;
                }
                for(unsigned x=0;x<OXP_NATIVE_W;++x)
                    native_buffers[i][y*OXP_NATIVE_W+x]=native_scanline.pixels[x];
            }
        }
    }
    if(!native_scene_valid){
        int has_clients=0;
        for(unsigned i=0;i<OXP_NATIVE_SLOTS;++i)has_clients|=native_infos[i].active!=0;
        if(!has_clients)return 0; /* no expensive blank scene at boot */
    }
    if(!changed)return 0;
    raster_row.layer=OXP_SHELL_LAYER_NATIVE;
    for(unsigned y=0;y<OXP_SHELL_RASTER_H;++y){
        raster_row.y=y;
        oxp_native_scene_row_skinned(views,native_infos,native_buffers,y,
                                    raster_row.pixels,skin_loaded?&skin:0,
                                    skin_mouse_x,skin_mouse_y,skin_pressed);
        if((int64_t)oxp_call3(29,(uint64_t)&raster_row,sizeof(raster_row),0))return -1;
    }
    if((int64_t)oxp_call(30,OXP_SHELL_MASK_NATIVE,0))return -1;
    for(unsigned i=0;i<OXP_NATIVE_SLOTS;++i){
        old_native_infos[i]=native_infos[i];
        old_native_windows[i]=views[i];
    }
    native_scene_valid=1;
    skin_input_changed=0;
    return 0;
}

static int32_t drag_index;
static int32_t resize_index;
static int32_t resize_start_w,resize_start_h;
static int32_t drag_anchor_x,drag_anchor_y,drag_start_x,drag_start_y;


__attribute__((noinline)) static const char *desktop_name(unsigned i){
    switch(i){
    case 0:return "MY COMPUTER";
    case 1:return "TERMINAL";
    case 2:return "PAINT";
    case 3:return "SETTINGS";
    case 4:return "WORDPAD";
    default:return "";
    }
}
static const char *menu_name(unsigned i){
    switch(i){
    case 0:return "TERMINAL";
    case 1:return "MY COMPUTER";
    case 2:return "ABOUT OPENXP";
    case 3:return "NOTEPAD";
    case 4:return "CALCULATOR";
    case 5:return "PAINT";
    case 6:return "SYSTEM MONITOR";
    case 7:return "APPEARANCE";
    case 8:return "MINES";
    case 9:return "CLOCK";
    case 10:return "SNAKE";
    case 11:return "RUN";
    case 12:return "WORDPAD";
    case 13:return "CALENDAR";
    default:return "";
    }
}
static void add_background_rect(unsigned x,unsigned y,unsigned w,unsigned h,uint32_t color){
    if(drawn>=OXP_SHELL_DRAW_MAX){frame_overflow=1;return;}
    shell_frame[drawn++]=(oxp_shell_draw_t){.op=OXP_SHELL_BG_RECT,
        .x=x,.y=y,.w=w,.h=h,.color=color};
}
static void add_background_text(unsigned x,unsigned y,const char *s,unsigned scale,uint32_t color){
    if(drawn>=OXP_SHELL_DRAW_MAX){frame_overflow=1;return;}
    oxp_shell_draw_t *cmd=&shell_frame[drawn++];
    *cmd=(oxp_shell_draw_t){.op=OXP_SHELL_BG_TEXT,.x=x,.y=y,.w=scale,.color=color};
    unsigned i=0;while(s[i]&&i<OXP_SHELL_TEXT_MAX-1){cmd->text[i]=s[i];++i;}
    cmd->text[i]=0;
}
static void add_rect(unsigned x,unsigned y,unsigned w,unsigned h,uint32_t color){
    if(drawn>=OXP_SHELL_DRAW_MAX){frame_overflow=1;return;}
    shell_frame[drawn++]=(oxp_shell_draw_t){.op=OXP_SHELL_RECT,
        .x=x,.y=y,.w=w,.h=h,.color=color};
}
static void add_text(unsigned x,unsigned y,const char *s,unsigned scale,uint32_t color){
    if(drawn>=OXP_SHELL_DRAW_MAX){frame_overflow=1;return;}
    oxp_shell_draw_t *cmd=&shell_frame[drawn++];
    *cmd=(oxp_shell_draw_t){.op=OXP_SHELL_TEXT,.x=x,.y=y,.w=scale,.color=color};
    unsigned i=0;while(s[i]&&i<OXP_SHELL_TEXT_MAX-1){cmd->text[i]=s[i];++i;}
    cmd->text[i]=0;
}
static void add_window_composition(void){
    if(drawn>=OXP_SHELL_DRAW_MAX){frame_overflow=1;return;}
    shell_frame[drawn++]=(oxp_shell_draw_t){.op=OXP_SHELL_COMPOSE_BEGIN};
    /* Only the userspace shell chooses draw order. The kernel validates
     * the indices, maps them to existing windows and blits owned surfaces. */
    for(unsigned z=0;z<17u;++z){
        for(unsigned i=0;i<17u;++i){
            const oxp_shell_window_t *win=&shell_windows[i];
            if(win->visible && !win->minimized && win->z==z){
                if(drawn>=OXP_SHELL_DRAW_MAX){frame_overflow=1;return;}
                shell_frame[drawn++]=(oxp_shell_draw_t){
                    .op=OXP_SHELL_COMPOSE_WINDOW,.x=i};
            }
        }
    }
}
static void refresh_windows(void){
    for(unsigned i=0;i<17u;++i){
        shell_windows[i]=(oxp_shell_window_t){0};
        (void)oxp_call3(24,i,(uint64_t)&shell_windows[i],sizeof(shell_windows[i]));
    }
}
static void decimal(char *buffer,uint64_t value){
    char temp[22];unsigned n=0,i=0;
    do{temp[n++]=(char)('0'+value%10u);value/=10u;}while(value&&n<21u);
    while(n)buffer[i++]=temp[--n];buffer[i]=0;
}
static int paint_shell(void){
    refresh_windows();drawn=0;frame_overflow=0;
    /* Desktop wallpaper and icon placement are composed in Ring 3. The
       privileged layer only rasterizes validated commands behind windows. */
    add_background_rect(0,0,SHELL_W,SHELL_TASK_TOP,oxp_theme_wallpaper(shell_theme));
    for(unsigned i=0;i<8u;++i){
        unsigned yy=i*(SHELL_TASK_TOP/8u);
        add_background_rect(0,yy,SHELL_W,SHELL_TASK_TOP/8u+1u,
            /* One stable palette per theme, with faint horizontal bands. */
            oxp_theme_wallpaper(shell_theme) + i*0x00020100u);
    }
    /* Desktop shortcut tiles now use the same icons as the applications.
     * Hover creates a lightweight selection highlight, not a giant block. */
    for(unsigned i=0;i<(desktop_icons_enabled?OXP_DESKTOP_COUNT:0u);++i){
        unsigned yy=OXP_DESKTOP_TOP+i*OXP_DESKTOP_STEP;
        if(!menu_art_loaded && desktop_hover==(int)i){
            add_background_rect(OXP_DESKTOP_X,yy,OXP_DESKTOP_SLOT_W,
                                OXP_DESKTOP_SLOT_H,0x396fb2);
            add_background_rect(OXP_DESKTOP_X+1u,yy+1u,
                                OXP_DESKTOP_SLOT_W-2u,OXP_DESKTOP_SLOT_H-2u,
                                0x4a86c1);
        }
        /* Dark shadow improves legibility on custom wallpapers. */
        /* Center text below the icon; 1px shadow works over bright wallpaper.
         * Exact glyph advance is 6 logical pixels at this font size. */
        const char *label=desktop_name(i);unsigned letters=0;
        while(label[letters])++letters;
        unsigned textw=oxp_raster_text_width(label,1u);
        unsigned xx=OXP_DESKTOP_X+(textw<OXP_DESKTOP_SLOT_W?
                             (OXP_DESKTOP_SLOT_W-textw)/2u:0u);
        add_background_text(xx+1u,yy+65u,label,1,0x10243c);
        add_background_text(xx,yy+64u,label,1,0xffffff);
    }
    add_window_composition();
    if(!skin_loaded){
        add_rect(0,SHELL_TASK_TOP,SHELL_W,42,oxp_theme_taskbar(shell_theme));
        add_rect(0,SHELL_TASK_TOP,1024,3,0x78ace6);
        add_rect(2,SHELL_TASK_TOP+4,105,35,oxp_theme_start(shell_theme));
    }
    /* Short ramp provides a crisp hover response without a continuous
     * animation loop or uploading unchanged desktop pixels. */
    if(start_hover && start_glow<2u)++start_glow;
    if(!start_hover && start_glow>0u)--start_glow;
    if(start_glow){
        add_rect(8u,SHELL_TASK_TOP+5u,90u,1u,
                 start_glow==2u?0xe0faff:0x86c7e7);
        add_rect(8u,SHELL_TASK_TOP+36u,90u,1u,0x134676);
    }
    if(shell_press_target==1){
        add_rect(8u,SHELL_TASK_TOP+7u,90u,2u,0x174a80);
    }
    add_text(19,SHELL_TASK_TOP+16u,"START",2,0xffffff);
    unsigned count=0,slot=0;
    for(unsigned i=0;i<17u;++i)if(shell_windows[i].visible)++count;
    unsigned bw=count?(SHELL_W-216u)/count:1u;
    if(bw>142u)bw=142u;
    if(!bw)bw=1u;
    for(unsigned i=0;i<17u;++i){
        if(!shell_windows[i].visible)continue;
        unsigned bx=115u+slot++*bw;
        unsigned wide=bw>3u?bw-3u:1u;
        add_rect(bx,SHELL_TASK_TOP+7u,wide,29u,
                 shell_windows[i].active?oxp_theme_accent(shell_theme):
                 task_hover==(int)i?oxp_theme_selection(shell_theme):
                 oxp_theme_taskbar(shell_theme));
        if(shell_press_target==(int)(100u+i) && wide>4u)
            add_rect(bx+2u,SHELL_TASK_TOP+9u,wide-4u,2u,0x143f72);
        if(wide>9u){
            add_rect(bx+2u,SHELL_TASK_TOP+8u,wide-4u,1u,
                shell_windows[i].active?0xb6def7:oxp_theme_accent(shell_theme));
        }
        unsigned tx=bx+(icons_loaded&&bw>=31u?28u:6u);
        unsigned characters=(bx+wide>tx+4u)?(bx+wide-tx-4u)/6u:0u;
        if(characters>OXP_SHELL_TEXT_MAX-1u)characters=OXP_SHELL_TEXT_MAX-1u;
        char label[OXP_SHELL_TEXT_MAX];unsigned j=0;
        while(j<characters && shell_windows[i].title[j]){
            label[j]=shell_windows[i].title[j];++j;
        }
        label[j]=0;
        if(j)add_text(tx,SHELL_TASK_TOP+17u,label,1,0xffffff);
    }
    add_rect(934,SHELL_TASK_TOP+5u,85u,32u,0x246cbc);
    add_rect(935,SHELL_TASK_TOP+6u,83u,1u,0x81c5f1);
    char seconds[24]="T+";decimal(seconds+2,oxp_ticks()/100u);
    add_text(944,SHELL_TASK_TOP+17u,seconds,1,0xffffff);
    if(menu_visible){
        if(!menu_art_loaded){
            add_rect(OXP_MENU_X,OXP_MENU_Y,OXP_MENU_W,OXP_MENU_H,0x163e78);
            add_rect(OXP_MENU_X+2u,OXP_MENU_Y+2u,OXP_MENU_W-4u,OXP_MENU_H-4u,
                     oxp_theme_panel(shell_theme));
            add_rect(OXP_MENU_X+2u,OXP_MENU_Y+2u,OXP_MENU_W-4u,47u,oxp_theme_accent(shell_theme));
            add_rect(OXP_MENU_X+2u,OXP_MENU_Y+2u,OXP_MENU_W-4u,2u,0x8fcaff);
        }
        add_rect(OXP_MENU_X+7u,OXP_MENU_Y+9u,35u,33u,oxp_theme_accent(shell_theme));
        add_text(OXP_MENU_X+51u,OXP_MENU_Y+14u,"OPENXP",2,0xffffff);
        add_text(OXP_MENU_X+51u,OXP_MENU_Y+33u,"AURORA DESKTOP",1,0xe6f5ff);
        add_rect(255u,OXP_MENU_ROW_Y,1u,278u,
                 shell_theme==OXP_THEME_MIDNIGHT?0x627092:0xb4c6db);
        for(unsigned i=0;i<OXP_MENU_ITEMS;++i){
            unsigned xx=OXP_MENU_ROW_X+(i/7u)*OXP_MENU_COLUMN_W;
            unsigned yy=OXP_MENU_ROW_Y+(i%7u)*OXP_MENU_ROW_STEP;
            int selected=(menu_cursor==i);
            if(!menu_art_loaded){
                if(selected){
                    add_rect(xx,yy,234u,37u,0x87b5e9);
                    add_rect(xx+1u,yy+1u,232u,35u,oxp_theme_selection(shell_theme));
                }else if(menu_hover==(int)i){
                    add_rect(xx,yy,234u,37u,0xd7eafb);
                }
            }
            add_text(xx+46u,yy+14u,menu_name(i),1,
                     shell_theme==OXP_THEME_MIDNIGHT?0xecf2ff:0x193755);
        }
        if(!menu_art_loaded){
            add_rect(OXP_MENU_X+2u,OXP_MENU_Y+338u,OXP_MENU_W-4u,29u,0xd5e5f7);
            add_rect(OXP_MENU_X+2u,OXP_MENU_Y+338u,OXP_MENU_W-4u,1u,0x8cb9e4);
        }
        add_text(OXP_MENU_X+17u,OXP_MENU_Y+351u,"OPENXP  -  EXPLORE MORE",1,
                 shell_theme==OXP_THEME_MIDNIGHT?0xe0eafa:0x315b88);
    }
    if(frame_overflow)return -1; /* Do not submit a partial composition. */
    unsigned layers=((background_visual_dirty||layer_changed(0))?
                     OXP_SHELL_MASK_BACKGROUND:0u) |
                    ((overlay_visual_dirty||layer_changed(1))?
                     OXP_SHELL_MASK_OVERLAY:0u);
    /* Motion inside a single hot zone frequently leaves every draw command
     * unchanged. In that case no compositor syscall or raster upload is
     * needed. Metadata-only window-order changes still get published. */
    int command_changed=!previous_valid || drawn!=previous_count;
    if(!command_changed)for(unsigned i=0;i<drawn;++i){
        if(bytes_differ(&shell_frame[i],&previous_frame[i],
                        sizeof(shell_frame[i]))){command_changed=1;break;}
    }
    if(!command_changed && !layers)return 0;
    /* Publish metadata first; if raster upload fails, the validated old
     * command renderer remains an immediately usable fallback. */
    if((int64_t)oxp_call(22,(uint64_t)shell_frame,drawn)!=0)return -1;
    if(layers && publish_pixel_layers(layers))return -1;
    for(unsigned i=0;i<drawn;++i)previous_frame[i]=shell_frame[i];
    previous_count=drawn;previous_valid=1;
    last_wallpaper_generation=wallpaper_generation;
    background_visual_dirty=0;overlay_visual_dirty=0;
    return 0;
}
static void shell_menu(int on){
    menu_visible=on?1:0;
    if(menu_visible){menu_cursor=0;menu_hover=-1;}
    else menu_hover=-1;
    overlay_visual_dirty=1;
    (void)oxp_call(23,OXP_SHELL_MENU,(uint64_t)menu_visible);
}
static void activate(unsigned index){
    if(index<14u){
        /* Prefer an isolated ELF process for programs already ported;
           retain the full legacy implementation if process creation fails. */
        int native=(index==0u || index==1u || index==3u ||
                    index==4u || index==5u || index==7u ||
                    index==8u || index==10u || index==11u ||
                    index==12u || index==13u);
        if(!native || (int64_t)oxp_call(23,OXP_SHELL_LAUNCH_NATIVE,index)!=0)
            (void)oxp_call(23,OXP_SHELL_OPEN,index);
    }
    shell_menu(0);
}
static void taskbar_click(unsigned x){
    unsigned count=0;
    for(unsigned i=0;i<17u;++i)if(shell_windows[i].visible)++count;
    unsigned bw=count?(SHELL_W-216u)/count:1u;
    if(bw>142u)bw=142u;
    if(!bw)bw=1u;
    unsigned slot=0;
    for(unsigned i=0;i<17u;++i){
        if(!shell_windows[i].visible)continue;
        unsigned bx=115u+slot++*bw;
        if(x>=bx && x<bx+(bw>3u?bw-3u:1u)){
            (void)oxp_call(23,OXP_SHELL_TASK,i);return;
        }
    }
}
static void handle_key(const oxp_shell_event_t *e){
    unsigned mods=e->modifiers;
    char c=(char)e->character;
    if(c>='A'&&c<='Z')c=(char)(c+32);
    if(e->code==KEY_WINDOWS || (e->code==KEY_ESCAPE && (mods&KEYMOD_CTRL))){
        shell_menu(!menu_visible);return;
    }
    if((mods&KEYMOD_ALT) && e->code==KEY_TAB){
        refresh_windows();
        /* Focus policy now lives in the unprivileged shell. */
        unsigned top=17u;
        for(unsigned i=0;i<17u;++i)if(shell_windows[i].active)top=i;
        for(unsigned pass=1;pass<=17u;++pass){
            unsigned target=(top+pass)%17u;
            if(shell_windows[target].visible && !shell_windows[target].minimized && target!=top){
                (void)oxp_call(23,OXP_SHELL_FOCUS,target);break;
            }
        }
        shell_menu(0);return;
    }
    /* Windows-style window management: Win+Up maximizes; Win+Down
       restores, or minimizes an already normal window. */
    if((mods&KEYMOD_WIN) && (e->code==KEY_UP || e->code==KEY_DOWN)){
        refresh_windows();
        for(unsigned i=0;i<17u;++i)if(shell_windows[i].active){
            if(e->code==KEY_UP && !shell_windows[i].maximized)
                (void)oxp_call(23,OXP_SHELL_MAXIMIZE,i);
            if(e->code==KEY_DOWN){
                if(shell_windows[i].maximized)(void)oxp_call(23,OXP_SHELL_MAXIMIZE,i);
                else (void)oxp_call(23,OXP_SHELL_TASK,i);
            }
            break;
        }
        return;
    }
    if((mods&KEYMOD_ALT) && e->code==KEY_F4){
        (void)oxp_call(23,OXP_SHELL_CLOSE,0);shell_menu(0);return;
    }
    if(mods&KEYMOD_WIN){
        switch(c){
        case 'd':(void)oxp_call(23,OXP_SHELL_DESKTOP,0);shell_menu(0);return;
        case 'e':activate(1);return;
        case 'r':activate(11);return;
        case 't':activate(0);return;
        case 'n':activate(3);return;
        case 'c':activate(4);return;
        case 'p':activate(5);return;
        case 'm':activate(6);return;
        case 's':activate(7);return;
        case 'g':activate(8);return;
        case 'k':activate(9);return;
        case 'y':activate(10);return;
        case 'w':activate(12);return;
        case 'a':activate(13);return;
        default:break;
        }
    }
    if(menu_visible){
        unsigned earlier=menu_cursor;
        if(e->code==KEY_ESCAPE){shell_menu(0);return;}
        if(e->code==KEY_HOME)menu_cursor=0u;
        if(e->code==KEY_END)menu_cursor=13u;
        if(e->code==KEY_TAB){
            if(mods&KEYMOD_SHIFT)
                menu_cursor=(menu_cursor+OXP_MENU_ITEMS-1u)%OXP_MENU_ITEMS;
            else menu_cursor=(menu_cursor+1u)%OXP_MENU_ITEMS;
        }
        if(e->code==KEY_UP && menu_cursor>0)--menu_cursor;
        if(e->code==KEY_DOWN && menu_cursor<13u)++menu_cursor;
        if(e->code==KEY_LEFT && menu_cursor>=7u)menu_cursor-=7u;
        if(e->code==KEY_RIGHT && menu_cursor<7u)menu_cursor+=7u;
        if(earlier!=menu_cursor){
            /* Keyboard selection wins until the pointer enters another row. */
            menu_hover=-1;
            overlay_visual_dirty=1;
        }
        if(e->code==KEY_ENTER)activate(menu_cursor);
    }
}
static int skin_hover_index(int32_t x,int32_t y){
    int best=-1;uint32_t best_z=0;
    /* An overlapped native caption must not glow through a legacy window. */
    for(unsigned i=0;i<17u;++i){
        const oxp_shell_window_t *w=&shell_windows[i];
        if(w->visible && !w->minimized && x>=(int32_t)w->x &&
           y>=(int32_t)w->y && x<(int32_t)(w->x+w->width) &&
           y<(int32_t)(w->y+w->height) && w->z>=best_z){
            best_z=w->z;
            best=(int)i;
        }
    }
    if(best<14)return -1;
    best=-1;best_z=0;
    for(unsigned i=14;i<17u;++i){
        const oxp_shell_window_t *w=&shell_windows[i];
        if(!w->visible || w->minimized || w->width<=60u)continue;
        unsigned buttons=w->width>88u?3u:1u;
        for(unsigned b=0;b<buttons;++b){
            unsigned pos=buttons==3u?(80u-26u*b):28u;
            int32_t bx=(int32_t)(w->x+w->width-pos);
            int32_t by=(int32_t)(w->y+6u);
            if(x>=bx && x<bx+21 && y>=by && y<by+21 &&
               (best<0 || w->z>=best_z)){
                best=(int)(i*3u+b);best_z=w->z;
            }
        }
    }
    return best;
}
/* Stable targets are used for hover and release activation alike.
 * Encoding: Start=1, task=100+slot, desktop=200+slot, menu=300+slot,
 * outside-menu dismissal=400. No application canvas is a shell target. */
static int task_target(unsigned x){
    unsigned count=0,slot=0;
    for(unsigned i=0;i<17u;++i)if(shell_windows[i].visible)++count;
    unsigned bw=count?(SHELL_W-216u)/count:1u;
    if(bw>142u)bw=142u;
    if(!bw)bw=1u;
    for(unsigned i=0;i<17u;++i){
        if(!shell_windows[i].visible)continue;
        unsigned bx=115u+slot++*bw;
        if(x>=bx && x<bx+(bw>3u?bw-3u:1u))return (int)(100u+i);
    }
    return -1;
}
static int shell_target_at(int32_t x,int32_t y){
    if(x<0 || y<0 || x>=(int32_t)SHELL_W || y>=(int32_t)SHELL_H)
        return -1;
    if(y>=(int32_t)SHELL_TASK_TOP){
        if(x<110)return 1;
        return task_target((unsigned)x);
    }
    if(menu_visible){
        int hit=oxp_menu_hit((unsigned)x,(unsigned)y);
        return hit>=0?300+hit:400;
    }
    if(!desktop_icons_enabled)return -1;
    int hit=oxp_desktop_hit((unsigned)x,(unsigned)y);
    if(hit<0)return -1;
    for(unsigned i=0;i<17u;++i){
        const oxp_shell_window_t *w=&shell_windows[i];
        if(w->visible && !w->minimized && x>=(int32_t)w->x &&
           y>=(int32_t)w->y && x<(int32_t)(w->x+w->width) &&
           y<(int32_t)(w->y+w->height))return -1;
    }
    return 200+hit;
}
static void shell_activate_target(int target){
    if(target==1){shell_menu(!menu_visible);return;}
    if(target>=100 && target<117){
        (void)oxp_call(23,OXP_SHELL_TASK,(unsigned)(target-100));
        shell_menu(0);return;
    }
    if(target>=200 && target<200+(int)OXP_DESKTOP_COUNT){
        activate(oxp_desktop_app((unsigned)(target-200)));return;
    }
    if(target>=300 && target<300+(int)OXP_MENU_ITEMS){
        activate((unsigned)(target-300));return;
    }
    if(target==400)shell_menu(0);
}
static void handle_pointer(const oxp_shell_event_t *e){
    int32_t x=(int32_t)e->x,y=(int32_t)e->y;
    if(drag_document_active){
        if(oxp_ticks()-drag_document_tick>300u)drag_document_active=0;
        else if(e->flags==OXP_SHELL_POINTER_UP){
            unsigned dx=x>drag_document_x?(unsigned)(x-drag_document_x):
                (unsigned)(drag_document_x-x);
            unsigned dy=y>drag_document_y?(unsigned)(y-drag_document_y):
                (unsigned)(drag_document_y-y);
            if(dx>5u || dy>5u){
                int target=-1;uint32_t top=0;
                for(unsigned i=0;i<17u;++i){
                    const oxp_shell_window_t *w=&shell_windows[i];
                    if(!w->visible || w->minimized || x<(int32_t)w->x ||
                       y<(int32_t)w->y || x>=(int32_t)(w->x+w->width) ||
                       y>=(int32_t)(w->y+w->height))continue;
                    if(target<0 || w->z>=top){target=(int)i;top=w->z;}
                }
                if(target>=0){
                    const char *program=oxp_drop_editor(dragged_document,
                                                        shell_windows[target].title);
                    if(program){
                        /* Fresh process-scoped launch; never mutate or inject
                         * content into an existing editor document. */
                        (void)oxp_launch_document(program,dragged_document);
                    }
                }
            }
            drag_document_active=0;
        }
    }
    int hot=skin_hover_index(x,y);
    /* Move must retain the original held state; previously any MOVE cleared
     * skin_pressed even while the left mouse button remained down. */
    int held=skin_pressed;
    if(e->flags==OXP_SHELL_POINTER_DOWN)held=1;
    else if(e->flags==OXP_SHELL_POINTER_UP)held=0;
    if(hot!=skin_last_hover || (hot>=0 && held!=skin_pressed))
        skin_input_changed=1;
    skin_last_hover=hot;
    skin_mouse_x=x;skin_mouse_y=y;skin_pressed=held;
    int target=shell_target_at(x,y);
    int next_desktop=target>=200 && target<200+(int)OXP_DESKTOP_COUNT?
                     target-200:-1;
    if(next_desktop!=desktop_hover){
        desktop_hover=next_desktop;background_visual_dirty=1;
    }
    int next_menu=target>=300 && target<300+(int)OXP_MENU_ITEMS?
                  target-300:-1;
    if(next_menu!=menu_hover){
        menu_hover=next_menu;overlay_visual_dirty=1;
        if(menu_hover>=0)menu_cursor=(unsigned)menu_hover;
    }
    int next_start=target==1;
    int next_task=target>=100 && target<117?target-100:-1;
    if(next_start!=start_hover || next_task!=task_hover){
        start_hover=next_start;task_hover=next_task;overlay_visual_dirty=1;
    }
    if(e->flags==OXP_SHELL_POINTER_UP){
        int pressed=shell_press_target;
        if(pressed!=-1){
            int activated=oxp_shell_finish_press(&shell_press_target,target);
            overlay_visual_dirty=1;
            if(activated>=0)shell_activate_target(activated);
        }
        drag_index=-1;resize_index=-1;
        return;
    }
    /* Shell controls activate on release, not down. This also makes the
     * menu selectable by dragging the cursor across rows while held. */
    if(e->flags==OXP_SHELL_POINTER_DOWN){
        if(target!=-1){
            shell_press_target=target;overlay_visual_dirty=1;
            drag_index=-1;resize_index=-1;
            return;
        }
        shell_press_target=-1;
    }
    /* Start menu is always above application windows. */
    if(menu_visible && x>=(int32_t)OXP_MENU_X &&
       x<(int32_t)(OXP_MENU_X+OXP_MENU_W) &&
       y>=(int32_t)OXP_MENU_Y && y<(int32_t)(OXP_MENU_Y+OXP_MENU_H)){
        drag_index=-1;resize_index=-1;return;
    }
    if(e->flags==OXP_SHELL_POINTER_DOWN){
        refresh_windows();drag_index=-1;resize_index=-1;
        uint32_t best_z=0;
        for(unsigned i=0;i<17u;++i){
            const oxp_shell_window_t *w=&shell_windows[i];
            if(!w->visible || w->minimized || w->maximized || w->width<90u)continue;
            int32_t wx=(int32_t)w->x,wy=(int32_t)w->y;
            int32_t right=(int32_t)(w->x+w->width),bottom=(int32_t)(w->y+w->height);
            int corner=(x>=right-18 && x<right && y>=bottom-18 && y<bottom);
            int caption=(x>=wx && x<right-84 && y>=wy && y<wy+33);
            if(!corner && !caption)continue;
            if((drag_index<0 && resize_index<0) || w->z>=best_z){
                drag_index=caption?(int32_t)i:-1;
                resize_index=corner?(int32_t)i:-1;
                best_z=w->z;
            }
        }
        if(resize_index>=0){
            const oxp_shell_window_t *w=&shell_windows[resize_index];
            drag_anchor_x=x;drag_anchor_y=y;
            resize_start_w=(int32_t)w->width;
            resize_start_h=(int32_t)w->height;
            (void)oxp_call(23,OXP_SHELL_FOCUS,(uint64_t)resize_index);
        }
        if(drag_index>=0){
            const oxp_shell_window_t *w=&shell_windows[drag_index];
            drag_anchor_x=x;drag_anchor_y=y;
            drag_start_x=(int32_t)w->x;drag_start_y=(int32_t)w->y;
            (void)oxp_call(23,OXP_SHELL_FOCUS,(uint64_t)drag_index);
        }
        return;
    }
    if(e->flags!=OXP_SHELL_POINTER_MOVE)return;
    if(resize_index>=0){
        const oxp_shell_window_t *w=&shell_windows[resize_index];
        int32_t ww=resize_start_w+x-drag_anchor_x;
        int32_t hh=resize_start_h+y-drag_anchor_y;
        int32_t minw=resize_index>=14?240:340,minh=resize_index>=14?190:280;
        if(resize_index==4){minw=270;minh=310;}
        if(resize_index==11){minw=340;minh=190;}
        if(resize_index==2){minw=340;minh=235;}
        if(ww<minw)ww=minw;if(hh<minh)hh=minh;
        if(ww>(int32_t)(SHELL_W-w->x))ww=(int32_t)(SHELL_W-w->x);
        if(hh>(int32_t)(SHELL_TASK_TOP-w->y))hh=(int32_t)(SHELL_TASK_TOP-w->y);
        if(ww>0 && hh>0){
            uint64_t dimensions=((uint64_t)(uint32_t)hh<<32)|(uint32_t)ww;
            (void)oxp_call3(23,OXP_SHELL_RESIZE,(uint64_t)resize_index,dimensions);
        }
        return;
    }
    if(drag_index<0)return;
    const oxp_shell_window_t *w=&shell_windows[drag_index];
    int32_t nx=drag_start_x+x-drag_anchor_x;
    int32_t ny=drag_start_y+y-drag_anchor_y;
    if(nx<0)nx=0;if(ny<0)ny=0;
    if((uint32_t)nx+w->width>SHELL_W)nx=(int32_t)(SHELL_W-w->width);
    if((uint32_t)ny+w->height>SHELL_TASK_TOP)ny=(int32_t)(SHELL_TASK_TOP-w->height);
    uint64_t coords=((uint64_t)(uint32_t)ny<<32)|(uint32_t)nx;
    (void)oxp_call3(23,OXP_SHELL_MOVE,(uint64_t)drag_index,coords);
}
static void handle_click(const oxp_shell_event_t *e){
    unsigned x=e->x,y=e->y;
    if(y>=SHELL_TASK_TOP){
        if(x<110u){shell_menu(!menu_visible);return;}
        taskbar_click(x);shell_menu(0);return;
    }
    if(menu_visible){
        int index=oxp_menu_hit(x,y);
        if(index>=0){activate((unsigned)index);return;}
        shell_menu(0);return;
    }
    int desktop=desktop_icons_enabled?oxp_desktop_hit(x,y):-1;
    if(desktop>=0){
        for(unsigned i=0;i<17u;++i){
            const oxp_shell_window_t *w=&shell_windows[i];
            if(w->visible && !w->minimized && x>=w->x && y>=w->y &&
               x<w->x+w->width && y<w->y+w->height)return;
        }
        activate(oxp_desktop_app((unsigned)desktop));
    }
}
void oxp_main(void){
    /* These are mutable service state, not constants: the native ELF packer
     * currently maps .text/.rodata/.data read-only. Initialize explicitly
     * in zero-filled RW memory rather than storing mutable data in RX pages. */
    drag_index=-1;
    resize_index=-1;
    skin_mouse_x=-1;skin_mouse_y=-1;skin_last_hover=-1;
    desktop_hover=-1;menu_hover=-1;task_hover=-1;
    shell_press_target=-1;start_hover=0;start_glow=0;
    desktop_icons_enabled=1u;drag_document_active=0;
    app_file_request_t deskpref={0};
    oxp_copy(deskpref.path,"home/desktop.cfg",sizeof deskpref.path);
    if(oxp_file(APP_FS_READ,&deskpref)==0 && deskpref.size==1u &&
       deskpref.data[0]<=1u)desktop_icons_enabled=deskpref.data[0];
    /* Preferences are restored before loading the selected artwork. */
    wm_state_init(&state);
    if(oxp_listen()!=0)oxp_exit(1);
    /* windowd has no application window: it leaves all native slots for
     * clients and owns the shell through its service capability instead. */
    /* Restore RAM-saved theme after windowd restarts (not persistent disk). */
    app_file_request_t preference={0};
    oxp_copy(preference.path,"home/appearance.cfg",sizeof preference.path);
    if(oxp_file(APP_FS_READ,&preference)==0 && preference.size==1 &&
       oxp_theme_valid(preference.data[0])){
        shell_theme=preference.data[0];
        (void)oxp_call(23,OXP_SHELL_THEME,shell_theme);
    }
    app_file_request_t wallpref={0};
    oxp_copy(wallpref.path,"home/wallpaper.cfg",sizeof wallpref.path);
    if(oxp_file(APP_FS_READ,&wallpref)==0 && wallpref.size==1 &&
       wallpref.data[0]<32u)(void)apply_wallpaper(wallpref.data[0]);
    if(!load_aurora_skin(shell_theme) || !load_shell_icons(shell_theme)){
        /* An absent custom pack never prevents recovery desktop startup. */
        if(shell_theme!=OXP_THEME_BLUE){
            (void)load_aurora_skin(OXP_THEME_BLUE);
            (void)load_shell_icons(OXP_THEME_BLUE);
        }
    }
    (void)load_menu_art(shell_theme);
    load_shell_font();
    refresh_windows();
    if(paint_shell() || publish_native_scene())oxp_exit(2);
    uint64_t refresh=oxp_ticks();
    for(;;){
        int work=accept_pending();
        if(work<0)break;
        for(unsigned i=0;i<WM_SERVICE_CHANNELS;++i)work+=service_one(i);
        oxp_shell_event_t event;
        while((int64_t)oxp_call(21,(uint64_t)&event,sizeof(event))==1){
            if(event.type==OXP_SHELL_EVENT_KEY)handle_key(&event);
            if(event.type==OXP_SHELL_EVENT_CLICK)handle_click(&event);
            if(event.type==OXP_SHELL_EVENT_POINTER)handle_pointer(&event);
            if(event.type==OXP_SHELL_EVENT_FOCUS)
                (void)oxp_call(23,OXP_SHELL_FOCUS,event.code);
            ++work;
        }
        if(work || oxp_ticks()-refresh>=25u){
            if(paint_shell() || publish_native_scene())break;
            refresh=oxp_ticks();
        }
        if(!work && oxp_ipc_wait_any(2)<0)break;
    }
    for(unsigned i=0;i<WM_SERVICE_CHANNELS;++i)drop_channel(i);
    (void)oxp_call(23,OXP_SHELL_STOP,0);
    oxp_exit(0);
}
