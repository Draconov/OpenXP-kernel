#include "oxp.h"
#include "wm_protocol.h"
#include "theme.h"
#include "ui_widgets.h"
/* Appearance: color schemes or a selected read-only BMP from Wallpapers. */
static unsigned palette_index,wallpaper_index,wallpaper_scroll;
static int mode; /* 0 = gradient, 1 = wallpaper */
static unsigned desktop_icons_enabled=1u;
static const char *status="SELECT A COLOR OR IMAGE";
static int wallpaper_at(unsigned ordinal,char path[96]){
    unsigned count=0;
    for(unsigned i=0;i<32u;++i){
        app_file_request_t r={0};
        oxp_copy(r.path,"windows/web/wallpaper",sizeof r.path);
        r.dest[0]='D';r.index=i;
        if(oxp_file(APP_FS_LIST,&r))break;
        unsigned n=0;while(r.path[n])++n;
        if(n>=4 && r.path[n-4]=='.' && r.path[n-3]=='b' &&
           r.path[n-2]=='m' && r.path[n-1]=='p'){
            if(count++==ordinal){oxp_copy(path,r.path,96);return 1;}
        }
    }
    return 0;
}
static const char *leaf(const char *path){
    const char *r=path;while(*path){if(*path=='/')r=path+1;++path;}
    return r;
}
static void load(void){
    app_file_request_t desk={0};
    oxp_copy(desk.path,"home/desktop.cfg",sizeof desk.path);
    if(oxp_file(APP_FS_READ,&desk)==0 && desk.size==1u && desk.data[0]<=1u)
        desktop_icons_enabled=desk.data[0];
    app_file_request_t r={0};oxp_copy(r.path,"home/appearance.cfg",sizeof r.path);
    if(!oxp_file(APP_FS_READ,&r)&&r.size==1&&oxp_theme_valid(r.data[0]))palette_index=r.data[0];
    r=(app_file_request_t){0};oxp_copy(r.path,"home/wallpaper.cfg",sizeof r.path);
    if(!oxp_file(APP_FS_READ,&r)&&r.size==1&&r.data[0]<32u){
        char path[96];if(wallpaper_at(r.data[0],path)){
            mode=1;wallpaper_index=r.data[0];wallpaper_scroll=(wallpaper_index/4u)*4u;
        }
    }
}
static void save(void){
    app_file_request_t r={0};
    oxp_copy(r.path,mode?"home/wallpaper.cfg":"home/appearance.cfg",sizeof r.path);
    r.size=1;r.data[0]=(uint8_t)(mode?wallpaper_index:palette_index);
    if(oxp_file(APP_FS_WRITE,&r)!=0){status="SAVE FAILED";return;}
    /* Switching to a color scheme must not restore the old BMP on service restart. */
    if(!mode){
        app_file_request_t old={0};oxp_copy(old.path,"home/wallpaper.cfg",sizeof old.path);
        (void)oxp_file(APP_FS_DELETE,&old);
    }
    status=(oxp_storage_status()&OXP_STORAGE_PRESENT)?
        "SAVED - DISK CHECKPOINT PENDING":"SELECTION SAVED IN RAM";
}
static int apply(void){
    int64_t h=oxp_connect();if(h<=0){status="WINDOW SERVICE OFFLINE";return 0;}
    wm_packet_t r={.magic=WM_MAGIC,.version=WM_VERSION,
                   .operation=mode?WM_WALLPAPER:WM_THEME,
                   .x=mode?wallpaper_index:palette_index},answer={0};
    if(oxp_send((uint64_t)h,&r,sizeof r)!=(int64_t)sizeof r){
        status="IMAGE REQUEST FAILED";(void)oxp_close((uint64_t)h);return 0;
    }
    /* Artwork switches may load multiple resource atlases on the WM side.
     * A single short IPC wait incorrectly reports the request as failed
     * while the compositor is still applying the theme. */
    int64_t n=-11;
    /* capipc_wait returns -EINVAL on a *normal timer expiry* too.  A timeout
     * is not an IPC disconnect. Retry reads for a bounded wall-clock period;
     * only a closed or invalid handle ends the transaction early.  Loading
     * the three large WM image resources can span several scheduler slices. */
    uint64_t began=oxp_ticks();
    while(n==-11 && oxp_ticks()-began<400u){
        n=oxp_recv((uint64_t)h,&answer,sizeof answer);
        if(n==-11){
            int64_t waited=oxp_ipc_wait((uint64_t)h,10u);
            if(waited!=0 && waited!=-22)break;
        }
    }
    int ok=n==(int64_t)sizeof answer && answer.magic==WM_MAGIC &&
           answer.version==WM_VERSION && answer.operation==(r.operation|WM_REPLY_FLAG) &&
           answer.status==WM_OK;
    status=ok?"DESKTOP UPDATED":(n==-11?"THEME SERVICE TIMEOUT":"THEME UPDATE FAILED");
    (void)oxp_close((uint64_t)h);
    return ok;
}
static void choose_theme(unsigned n){
    unsigned old=palette_index;int old_mode=mode;
    mode=0;palette_index=n;
    if(apply()){
        save();
        /* Refresh this Settings window immediately; the normal theme
         * polling path also refreshes every other native application. */
        oxp_ui_load_theme(n);
    }else{palette_index=old;mode=old_mode;}
}
static void choose_wallpaper(unsigned n){
    char path[96];if(!wallpaper_at(n,path)){status="NO SUCH WALLPAPER";return;}
    mode=1;wallpaper_index=n;if(apply())save();
}
static void toggle_desktop_icons(void){
    unsigned requested=desktop_icons_enabled?0u:1u;
    int64_t h=oxp_connect();
    if(h<=0){status="WINDOW SERVICE OFFLINE";return;}
    wm_packet_t msg={.magic=WM_MAGIC,.version=WM_VERSION,
                     .operation=WM_DESKTOP,.x=requested},reply={0};
    int64_t sent=oxp_send((uint64_t)h,&msg,sizeof msg);
    int64_t count=sent==(int64_t)sizeof msg?
        oxp_recv((uint64_t)h,&reply,sizeof reply):-1;
    if(count==-11 && oxp_ipc_wait((uint64_t)h,50)>=0)
        count=oxp_recv((uint64_t)h,&reply,sizeof reply);
    (void)oxp_close((uint64_t)h);
    if(count!=(int64_t)sizeof reply || reply.magic!=WM_MAGIC ||
       reply.operation!=(WM_DESKTOP|WM_REPLY_FLAG) || reply.status!=WM_OK){
        status="DESKTOP UPDATE FAILED";return;
    }
    desktop_icons_enabled=requested;
    app_file_request_t save={0};
    oxp_copy(save.path,"home/desktop.cfg",sizeof save.path);
    save.size=1u;save.data[0]=(uint8_t)requested;
    if(oxp_file(APP_FS_WRITE,&save)==0){
        status=(oxp_storage_status()&OXP_STORAGE_PRESENT)?
            "ICONS SAVED - DISK SYNC PENDING":"ICONS SAVED IN RAM";
    }else status="ICONS APPLIED - SETTINGS NOT SAVED";
}
static unsigned theme_step(void){return (oxp_canvas_width()-28u)/3u;}
static unsigned theme_bar_width(void){return theme_step()>4u?theme_step()-4u:1u;}
static unsigned shown_wallpapers(void){
    unsigned h=oxp_canvas_height();return h>106u?oxp_min(4u,(h-99u)/17u):0u;
}
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    unsigned list_top=78u,rows=shown_wallpapers();
    oxp_draw_cmd_t cmds[]={{OXP_DRAW_CLEAR,0,0,0,0,oxp_theme_panel(palette_index)},
        {OXP_DRAW_FILL,8,26,w-16,42,oxp_theme_toolbar(palette_index)},
        {OXP_DRAW_FILL,8,list_top,w-16,h>list_top+13u?h-list_top-13u:1u,0xffffff}};
    oxp_draw(cmds,3);
    oxp_ui_toolbar((oxp_ui_rect){0,0,w,24});
    oxp_ui_text((oxp_ui_rect){12u,0u,w>135u?w-135u:1u,24u},"Display Properties",10u,oxp_ui_ink(),0);
    oxp_ui_button((oxp_ui_rect){w-112u,3u,108u,18u},
                  desktop_icons_enabled?"ICONS: SHOW":"ICONS: HIDE",OXP_UI_NORMAL,0);
    oxp_ui_panel((oxp_ui_rect){8,26,w-16,42});
    oxp_ui_text((oxp_ui_rect){13u,26u,w-26u,16u},"Appearance theme",29u,oxp_ui_ink(),0);
    unsigned step=theme_step();
    for(unsigned i=0;i<OXP_THEME_COUNT;++i){
        unsigned x=14u+i*step;
        oxp_ui_tab((oxp_ui_rect){x,41,theme_bar_width(),22},"",!mode&&palette_index==i);
        oxp_fill(x+3u,45u,theme_bar_width()>6u?theme_bar_width()-6u:1u,13u,oxp_theme_wallpaper(i));
        if(!mode&&palette_index==i)oxp_label(x+theme_bar_width()-10u,46,"*",1,0xffffff);
        /* Named themes; swatch fills and bitmap controls all change together. */
        oxp_label(x+3u,55u,i==0u?"BLUE":i==1u?"OLIVE":"NIGHT",1u,0xffffff);
    }
    oxp_ui_text((oxp_ui_rect){13u,66u,w-70u,16u},"Background",69u,oxp_ui_ink(),0);
    oxp_ui_button((oxp_ui_rect){w-62u,66u,25u,14u},"<",OXP_UI_NORMAL,0);
    oxp_ui_button((oxp_ui_rect){w-36u,66u,25u,14u},">",OXP_UI_NORMAL,0);
    for(unsigned i=0;i<rows;++i){
        char path[96];if(!wallpaper_at(wallpaper_scroll+i,path))break;
        unsigned y=82u+i*17u;
        if(mode&&wallpaper_index==wallpaper_scroll+i)oxp_fill(10,y,w-20u,16,0xc7def9);
        oxp_fill(14,y+2u,17,12,0x7dadd3);
        oxp_fill(16,y+5u,13,7,0x60a86b);
        oxp_label(38,y+4u,leaf(path),1,0x24517c);
    }
    if(h>32u){oxp_label(12,h-12u,status,1,oxp_theme_ink(palette_index));
        oxp_ui_button((oxp_ui_rect){w-67u,h-21u,61u,19u},"SAVE",OXP_UI_PRIMARY,0);}
}
void oxp_main(void){
    if(oxp_open("Display Properties"))oxp_exit(1);
    oxp_ui_atlas_init_widgets();
    load();
    for(;;){
        render();uint32_t e=oxp_ui_wait_theme();if(e==OXP_CLOSE||e==27)break;
        if(e==OXP_SAVE||e=='s'||e=='S'){save();continue;}
        if(e==OXP_LEFT){if(wallpaper_scroll>=4)wallpaper_scroll-=4;continue;}
        if(e==OXP_RIGHT){char test[96];if(wallpaper_at(wallpaper_scroll+4,test))wallpaper_scroll+=4;continue;}
        if(e>='1'&&e<='3'){choose_theme(e-'1');continue;}
        if(e=='i'||e=='I'){toggle_desktop_icons();continue;}
        if(e&OXP_CLICK){
            if(oxp_hit(e,oxp_canvas_width()-112u,3u,108u,18u)){
                toggle_desktop_icons();continue;
            }
            for(unsigned i=0;i<3;++i)if(oxp_hit(e,14+i*theme_step(),43,theme_bar_width(),18)){
                choose_theme(i);break;
            }
            if(oxp_hit(e,oxp_canvas_width()-62u,65,22,16)){if(wallpaper_scroll>=4)wallpaper_scroll-=4;}
            if(oxp_hit(e,oxp_canvas_width()-36u,65,22,16)){char test[96];if(wallpaper_at(wallpaper_scroll+4,test))wallpaper_scroll+=4;}
            for(unsigned i=0;i<shown_wallpapers();++i)if(oxp_hit(e,10,82+i*17,oxp_canvas_width()-20u,16)){
                choose_wallpaper(wallpaper_scroll+i);break;
            }
            if(oxp_hit(e,oxp_canvas_width()-67u,oxp_canvas_height()-21u,61,19))save();
        }
    }
    oxp_exit(0);
}
