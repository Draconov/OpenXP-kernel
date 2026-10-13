#include "oxp.h"
#include "wm_protocol.h"
#include "theme.h"
/* Preferences are RAM-backed; windowd owns global wallpaper and taskbar. */
static unsigned palette_index;
static const char filename[]="home/appearance.cfg";
static const char *status="CHOOSE A COLOR SCHEME";
static void load(void){
    app_file_request_t r={0};oxp_copy(r.path,filename,sizeof r.path);
    if(!oxp_file(APP_FS_READ,&r)&&r.size==1&&oxp_theme_valid(r.data[0])){
        palette_index=r.data[0];status="SAVED SCHEME LOADED";
    }
}
static void save(void){
    app_file_request_t r={0};oxp_copy(r.path,filename,sizeof r.path);
    r.size=1;r.data[0]=(uint8_t)palette_index;
    status=oxp_file(APP_FS_WRITE,&r)==0?"SAVED IN RAM":"SAVE FAILED";
}
static void apply(void){
    int64_t h=oxp_connect();
    if(h<=0){status="WINDOW SERVICE OFFLINE";return;}
    wm_packet_t r={.magic=WM_MAGIC,.version=WM_VERSION,.operation=WM_THEME,
                   .x=palette_index},answer={0};
    if(oxp_send((uint64_t)h,&r,sizeof r)!=(int64_t)sizeof r){
        status="THEME REQUEST FAILED";(void)oxp_close((uint64_t)h);return;
    }
    int64_t n=oxp_recv((uint64_t)h,&answer,sizeof answer);
    if(n==-11 && oxp_ipc_wait((uint64_t)h,40)>=0)
        n=oxp_recv((uint64_t)h,&answer,sizeof answer);
    status=n==(int64_t)sizeof answer && answer.magic==WM_MAGIC &&
           answer.version==WM_VERSION && answer.operation==(WM_THEME|WM_REPLY_FLAG) &&
           answer.status==WM_OK?"DESKTOP THEME APPLIED":"THEME REQUEST REJECTED";
    (void)oxp_close((uint64_t)h);
}
static void render(void){
    oxp_draw_cmd_t cmds[]={{OXP_DRAW_CLEAR,0,0,0,0,0xf0f5fd},
        {OXP_DRAW_FILL,12,39,295,85,0xe2eaf4}};
    oxp_draw(cmds,2);oxp_label(12,10,"APPEARANCE SETTINGS",1,0x234b7d);
    for(unsigned i=0;i<OXP_THEME_COUNT;++i){
        oxp_fill(28+i*91,53,72,50,oxp_theme_wallpaper(i));
        if(palette_index==i)oxp_label(52+i*91,108,"^",1,0x174679);
    }
    oxp_label(16,137,"S SAVE   O RESTORE   1-3 APPLY",1,0x234b7d);
    oxp_label(16,157,status,1,0x65758d);
}
void oxp_main(void){
    if(oxp_open("Settings - native ELF64"))oxp_exit(1);
    load();
    for(;;){
        render();uint32_t e=oxp_wait(0);if(e==OXP_CLOSE||e==27)break;
        if(e==OXP_SAVE||e=='s'||e=='S')save();
        else if(e==OXP_OPEN||e=='o'||e=='O'){load();apply();}
        else if(e>='1'&&e<='3'){palette_index=e-'1';apply();}
        else if(e&OXP_CLICK)for(unsigned i=0;i<OXP_THEME_COUNT;++i)
            if(oxp_hit(e,28+i*91,53,72,50)){palette_index=i;apply();break;}
    }
    oxp_exit(0);
}
