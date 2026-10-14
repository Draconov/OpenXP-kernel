#include "oxp.h"
#include "wm_protocol.h"
#include "theme.h"
#include "wallpaper.h"
#include "shell_raster.h"
#include "native_scene.h"
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
/* Decoded, downsampled RGB565 image; no raw framebuffer mapping in Ring 3. */
static uint16_t wallpaper_pixels[OXP_WALL_W*OXP_WALL_H];
static int wallpaper_active;
static unsigned last_wallpaper_generation,wallpaper_generation;
static unsigned wallpaper_ordinal;
static char wallpaper_path[96];
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
    wm_packet_t request={0};
    int64_t count=oxp_recv(channel,&request,sizeof(request));
    if(count==-11)return 0;
    if(count<=0){drop_channel(slot);return 1;}
    wm_packet_t reply={0};
    if(count==(int64_t)sizeof(request) && request.magic==WM_MAGIC &&
       request.version==WM_VERSION && request.operation==WM_THEME){
        reply=(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,
            .operation=WM_THEME|WM_REPLY_FLAG,.status=WM_BAD_REQUEST};
        if(oxp_theme_valid(request.x) &&
           (int64_t)oxp_call(23,OXP_SHELL_THEME,request.x)==0){
            shell_theme=request.x;
            wallpaper_active=0;
            ++wallpaper_generation;
            reply.status=WM_OK;
        }
    }else if(count==(int64_t)sizeof(request) && request.magic==WM_MAGIC &&
             request.version==WM_VERSION && request.operation==WM_WALLPAPER){
        reply=(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,
            .operation=WM_WALLPAPER|WM_REPLY_FLAG,.status=WM_BAD_REQUEST};
        if(request.x<32u && apply_wallpaper(request.x))reply.status=WM_OK;
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
            }else oxp_raster_row(shell_frame,drawn,layer,y,raster_row.pixels);
            if((int64_t)oxp_call3(29,(uint64_t)&raster_row,sizeof(raster_row),0)!=0)
                return -1;
        }
    }
    return (int64_t)oxp_call(30,mask,0)==0?0:-1;
}

/* Windowd now constructs native window pixels, not just their order.
 * Static data lives in the service's private writable ELF segment. */
static oxp_shell_window_t shell_windows[17];
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
    int changed=!native_scene_valid;
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
            for(unsigned y=0;y<OXP_NATIVE_H;++y){
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
        oxp_native_scene_row(views,native_infos,native_buffers,y,raster_row.pixels);
        if((int64_t)oxp_call3(29,(uint64_t)&raster_row,sizeof(raster_row),0))return -1;
    }
    if((int64_t)oxp_call(30,OXP_SHELL_MASK_NATIVE,0))return -1;
    for(unsigned i=0;i<OXP_NATIVE_SLOTS;++i){
        old_native_infos[i]=native_infos[i];
        old_native_windows[i]=views[i];
    }
    native_scene_valid=1;
    return 0;
}

static int menu_visible;
static uint32_t menu_cursor;
static int32_t drag_index;
static int32_t resize_index;
static int32_t resize_start_w,resize_start_h;
static int32_t drag_anchor_x,drag_anchor_y,drag_start_x,drag_start_y;


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
    add_background_rect(14,26,94,65,0x286db7);
    add_background_text(24,48,"MY COMPUTER",1,0xffffff);
    add_background_rect(14,116,94,65,0x286db7);
    add_background_text(30,138,"DOCUMENTS",1,0xffffff);
    add_background_rect(14,206,94,65,0x286db7);
    add_background_text(30,228,"TERMINAL",1,0xffffff);
    add_window_composition();
    add_rect(0,SHELL_TASK_TOP,SHELL_W,42,oxp_theme_taskbar(shell_theme));
    add_rect(0,SHELL_TASK_TOP,1024,3,0x78ace6);
    add_rect(2,SHELL_TASK_TOP+4,105,35,oxp_theme_start(shell_theme));
    add_text(19,SHELL_TASK_TOP+16,"START",2,0xffffff);
    unsigned count=0,slot=0;
    for(unsigned i=0;i<17u;++i)if(shell_windows[i].visible)++count;
    unsigned bw=count?(SHELL_W-216u)/count:1u;
    if(bw>142u)bw=142u;
    if(!bw)bw=1u;
    for(unsigned i=0;i<17u;++i){
        if(!shell_windows[i].visible)continue;
        unsigned bx=115u+slot++*bw;
        unsigned wide=bw>3u?bw-3u:1u;
        add_rect(bx,SHELL_TASK_TOP+6,wide,31,
                 shell_windows[i].active?0x317ec6:0x2e65a3);
        add_text(bx+5,SHELL_TASK_TOP+16,shell_windows[i].title,1,0xffffff);
    }
    add_rect(934,SHELL_TASK_TOP+5,85,32,0x246cbc);
    char seconds[24]="T+";decimal(seconds+2,oxp_ticks()/100u);
    add_text(944,SHELL_TASK_TOP+17,seconds,1,0xffffff);
    if(menu_visible){
        add_rect(2,SHELL_TASK_TOP-370u,500,370,0xeaf3ff);
        add_rect(3,SHELL_TASK_TOP-367u,498,37,0x1a5fb9);
        add_text(19,SHELL_TASK_TOP-357u,"OPENXP PROGRAMS",2,0xffffff);
        for(unsigned i=0;i<14u;++i){
            unsigned xx=12u+(i/7u)*245u,yy=SHELL_TASK_TOP-319u+(i%7u)*43u;
            add_rect(xx,yy,230,40,menu_cursor==i?0xcfe7ff:0xf9fafd);
            add_text(xx+15,yy+15,menu_name(i),1,0x1c3968);
        }
    }
    if(frame_overflow)return -1; /* Do not submit a partial composition. */
    unsigned layers=(layer_changed(0)?OXP_SHELL_MASK_BACKGROUND:0u) |
                    (layer_changed(1)?OXP_SHELL_MASK_OVERLAY:0u);
    /* Publish metadata first; if raster upload fails, the validated old
     * command renderer remains an immediately usable fallback. */
    if((int64_t)oxp_call(22,(uint64_t)shell_frame,drawn)!=0)return -1;
    if(layers && publish_pixel_layers(layers))return -1;
    for(unsigned i=0;i<drawn;++i)previous_frame[i]=shell_frame[i];
    previous_count=drawn;previous_valid=1;
    last_wallpaper_generation=wallpaper_generation;
    return 0;
}
static void shell_menu(int on){
    menu_visible=on?1:0;
    if(menu_visible)menu_cursor=0;
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
        if(e->code==KEY_ESCAPE){shell_menu(0);return;}
        if(e->code==KEY_UP && menu_cursor>0)--menu_cursor;
        if(e->code==KEY_DOWN && menu_cursor<13u)++menu_cursor;
        if(e->code==KEY_LEFT && menu_cursor>=7u)menu_cursor-=7u;
        if(e->code==KEY_RIGHT && menu_cursor<7u)menu_cursor+=7u;
        if(e->code==KEY_ENTER)activate(menu_cursor);
    }
}
static void handle_pointer(const oxp_shell_event_t *e){
    int32_t x=(int32_t)e->x,y=(int32_t)e->y;
    if(e->flags==OXP_SHELL_POINTER_UP){drag_index=-1;resize_index=-1;return;}
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
    if(!menu_visible && x<110u && y<275u){
        if(y>=25u && y<92u){activate(1);return;}
        if(y>=115u && y<182u){activate(1);return;}
        if(y>=205u && y<272u){activate(0);return;}
    }
    if(menu_visible && y>=SHELL_TASK_TOP-370u){
        if(x>=12u&&x<502u){
            unsigned col=(x-12u)/245u;
            unsigned localx=(x-12u)%245u;
            if(col<2u && localx<230u && y>=SHELL_TASK_TOP-319u){
                unsigned row=(y-(SHELL_TASK_TOP-319u))/43u;
                if(row<7u && (y-(SHELL_TASK_TOP-319u))%43u<40u){
                    activate(col*7u+row);return;
                }
            }
        }
        shell_menu(0);
    }
}
void oxp_main(void){
    /* These are mutable service state, not constants: the native ELF packer
     * currently maps .text/.rodata/.data read-only. Initialize explicitly
     * in zero-filled RW memory rather than storing mutable data in RX pages. */
    drag_index=-1;
    resize_index=-1;
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
