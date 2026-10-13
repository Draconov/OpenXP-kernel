#include "oxp.h"
#include "wm_protocol.h"
#include "../../kernel/user/wm_display.h"
#include "../../kernel/user/shell_abi.h"
#include "../../kernel/input/input.h"

/* Persistent, single-threaded userspace WM policy server. The kernel handles
 * per-PID surfaces and low-level composition; no raw framebuffer is exposed.
 * A connection owns at most one logical window until it closes. */
#define WM_SERVICE_CHANNELS WM_MAX_WINDOWS
static wm_state_t state;
static uint64_t channels[WM_SERVICE_CHANNELS];

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
    if(count==(int64_t)sizeof(request)) {
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
static int menu_visible;
static uint32_t menu_cursor;
static oxp_shell_window_t shell_windows[17];
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
static void add_rect(unsigned x,unsigned y,unsigned w,unsigned h,uint32_t color){
    if(drawn>=OXP_SHELL_DRAW_MAX)return;
    shell_frame[drawn++]=(oxp_shell_draw_t){.op=OXP_SHELL_RECT,
        .x=x,.y=y,.w=w,.h=h,.color=color};
}
static void add_text(unsigned x,unsigned y,const char *s,unsigned scale,uint32_t color){
    if(drawn>=OXP_SHELL_DRAW_MAX)return;
    oxp_shell_draw_t *cmd=&shell_frame[drawn++];
    *cmd=(oxp_shell_draw_t){.op=OXP_SHELL_TEXT,.x=x,.y=y,.w=scale,.color=color};
    unsigned i=0;while(s[i]&&i<OXP_SHELL_TEXT_MAX-1){cmd->text[i]=s[i];++i;}
    cmd->text[i]=0;
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
    refresh_windows();drawn=0;
    add_rect(0,SHELL_TASK_TOP,SHELL_W,42,0x225cab);
    add_rect(0,SHELL_TASK_TOP,1024,3,0x78ace6);
    add_rect(2,SHELL_TASK_TOP+4,105,35,0x26a749);
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
    return (int64_t)oxp_call(22,(uint64_t)shell_frame,drawn)==0?0:-1;
}
static void shell_menu(int on){
    menu_visible=on?1:0;
    if(menu_visible)menu_cursor=0;
    (void)oxp_call(23,OXP_SHELL_MENU,(uint64_t)menu_visible);
}
static void activate(unsigned index){
    if(index<14u)(void)oxp_call(23,OXP_SHELL_OPEN,index);
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
static void handle_click(const oxp_shell_event_t *e){
    unsigned x=e->x,y=e->y;
    if(y>=SHELL_TASK_TOP){
        if(x<110u){shell_menu(!menu_visible);return;}
        taskbar_click(x);shell_menu(0);return;
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
    wm_state_init(&state);
    if(oxp_listen()!=0)oxp_exit(1);
    /* windowd has no application window: it leaves all native slots for
     * clients and owns the shell through its service capability instead. */
    refresh_windows();
    if(paint_shell())oxp_exit(2);
    uint64_t refresh=oxp_ticks();
    for(;;){
        int work=accept_pending();
        if(work<0)break;
        for(unsigned i=0;i<WM_SERVICE_CHANNELS;++i)work+=service_one(i);
        oxp_shell_event_t event;
        while((int64_t)oxp_call(21,(uint64_t)&event,sizeof(event))==1){
            if(event.type==OXP_SHELL_EVENT_KEY)handle_key(&event);
            if(event.type==OXP_SHELL_EVENT_CLICK)handle_click(&event);
            if(event.type==OXP_SHELL_EVENT_FOCUS)
                (void)oxp_call(23,OXP_SHELL_FOCUS,event.code);
            ++work;
        }
        if(work || oxp_ticks()-refresh>=25u){
            if(paint_shell())break;
            refresh=oxp_ticks();
        }
        if(!work && oxp_ipc_wait_any(2)<0)break;
    }
    for(unsigned i=0;i<WM_SERVICE_CHANNELS;++i)drop_channel(i);
    (void)oxp_call(23,OXP_SHELL_STOP,0);
    oxp_exit(0);
}
