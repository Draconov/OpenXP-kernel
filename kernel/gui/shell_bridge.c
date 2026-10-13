#include "gui/shell_bridge.h"
#include "gui/desktop.h"
#include "arch/x86_64/irq_state.h"
#include <stddef.h>
#define SHELL_INPUT_CAP 64u
static oxp_shell_draw_t frame[OXP_SHELL_DRAW_MAX];
static uint32_t frame_count;
static oxp_shell_event_t queue[SHELL_INPUT_CAP];
static uint32_t qhead,qtail;
static bool running,dirty,menu_is_open;
static uint64_t service_pid, last_frame_tick;
static desktop_t *desktop;
static uint64_t lock_state(void){
#ifndef OPENXP_SHELL_HOST_TEST
    return irq_save_disable();
#else
    return 0;
#endif
}
static void unlock_state(uint64_t flags){
#ifndef OPENXP_SHELL_HOST_TEST
    irq_restore(flags);
#else
    (void)flags;
#endif
}
void shell_bridge_reset(void){
    uint64_t flags=lock_state();running=false;menu_is_open=false;dirty=true;frame_count=0;qhead=qtail=0;service_pid=0;last_frame_tick=0;
    unlock_state(flags);
}
void shell_bridge_attach(void *d){desktop=(desktop_t *)d;}
void shell_bridge_shutdown(void){shell_bridge_reset();}
void shell_bridge_owner_exit(uint64_t pid){if(pid && pid==service_pid)shell_bridge_shutdown();}
void shell_bridge_watchdog(uint64_t now){
    /* Ring 3 cannot indefinitely capture keyboard/mouse input if it hangs. */
    if(running && now-last_frame_tick>300u)shell_bridge_shutdown();
}
bool shell_bridge_take_dirty(void){uint64_t f=lock_state();bool old=dirty;dirty=false;unlock_state(f);return old;}
bool shell_bridge_active(void){return running && desktop!=NULL;}
bool shell_bridge_menu_open(void){return shell_bridge_active() && menu_is_open;}
static bool valid(const oxp_shell_draw_t *c){
    if(c->color&0xff000000u)return false;
    if(c->op==OXP_SHELL_RECT)
        return c->w && c->h && c->x<1024u && c->y<768u &&
               c->w<=1024u-c->x && c->h<=768u-c->y;
    if(c->op!=OXP_SHELL_TEXT || c->w<1u || c->w>2u ||
       c->x>=1024u || c->y>=768u)return false;
    for(unsigned i=0;i<OXP_SHELL_TEXT_MAX;++i){
        unsigned char ch=(unsigned char)c->text[i];
        if(!ch)return true;
        if(ch<32u || ch>126u)return false;
    }
    return false; /* Unterminated user-supplied text. */
}
bool shell_bridge_submit(uint64_t pid,const oxp_shell_draw_t *commands,uint32_t count,uint64_t tick){
    if(!pid||!commands||!count||count>OXP_SHELL_DRAW_MAX)return false;
    for(uint32_t i=0;i<count;++i)if(!valid(&commands[i]))return false;
    uint64_t flags=lock_state();
    for(uint32_t i=0;i<count;++i)frame[i]=commands[i];
    frame_count=count;running=true;service_pid=pid;last_frame_tick=tick;dirty=true;
    unlock_state(flags);return true;
}
void shell_bridge_compose(fb_surface_t *target){
    if(!target||!shell_bridge_active())return;
    uint64_t flags=lock_state();
    for(uint32_t i=0;i<frame_count;++i){
        const oxp_shell_draw_t *c=&frame[i];
        uint32_t col=fb_rgb(target,(uint8_t)(c->color>>16),
                                  (uint8_t)(c->color>>8),(uint8_t)c->color);
        if(c->op==OXP_SHELL_RECT)fb_rect(target,(int)c->x,(int)c->y,(int)c->w,(int)c->h,col);
        else fb_text_limit(target,(int)c->x,(int)c->y,c->text,
                           OXP_SHELL_TEXT_MAX-1u,(int)c->w,col);
    }
    unlock_state(flags);
}
static void push(const oxp_shell_event_t *event){
    uint64_t flags=lock_state();
    uint32_t next=(qhead+1u)%SHELL_INPUT_CAP;
    if(next==qtail)qtail=(qtail+1u)%SHELL_INPUT_CAP;
    queue[qhead]=*event;qhead=next;
    unlock_state(flags);
}
bool shell_bridge_route(const input_event_t *e,bool menu_visible,int height){
    if(!shell_bridge_active() || !e)return false;
    oxp_shell_event_t m={0};
    if(e->kind==INPUT_KEY){
        bool global=(e->code==KEY_WINDOWS ||
            (e->code==KEY_ESCAPE && menu_visible) ||
            ((e->modifiers&KEYMOD_CTRL)&&e->code==KEY_ESCAPE) ||
            ((e->modifiers&KEYMOD_ALT)&&
              (e->code==KEY_TAB || e->code==KEY_F4)) ||
            (e->modifiers&KEYMOD_WIN));
        if(!global && !menu_visible)return false;
        m.type=OXP_SHELL_EVENT_KEY;m.code=(uint32_t)e->code;
        m.modifiers=e->modifiers;m.character=(unsigned char)e->key;
    }else if(e->kind==INPUT_BUTTON && e->left){
        if(e->y<height-42 && !(menu_visible && e->x<510 && e->y>=height-412))return false;
        m.type=OXP_SHELL_EVENT_CLICK;m.x=(uint32_t)e->x;m.y=(uint32_t)e->y;
    }else return false;
    push(&m);return true;
}
void shell_bridge_notify_focus(uint32_t index){
    if(shell_bridge_active() && index<DESKTOP_WINDOWS){
        oxp_shell_event_t m={.type=OXP_SHELL_EVENT_FOCUS,.code=index};
        push(&m);
    }
}
bool shell_bridge_pop(oxp_shell_event_t *event){
    if(!event)return false;
    uint64_t flags=lock_state();
    if(qhead==qtail){unlock_state(flags);return false;}
    *event=queue[qtail];qtail=(qtail+1u)%SHELL_INPUT_CAP;
    unlock_state(flags);return true;
}
bool shell_bridge_action(uint32_t action,uint32_t index){
    if(!shell_bridge_active() || !desktop)return false;
    if(action==OXP_SHELL_STOP){shell_bridge_shutdown();return true;}
    dirty=true;
    if(action==OXP_SHELL_MENU){menu_is_open=index!=0;return true;}
#ifdef OPENXP_SHELL_HOST_TEST
    (void)index;
    return false;
#else
    if(action==OXP_SHELL_CLOSE)return desktop_shell_close(desktop);
    if(action==OXP_SHELL_DESKTOP){desktop_shell_desktop(desktop);return true;}
    if(action==OXP_SHELL_OPEN)return desktop_shell_open(desktop,index);
    if(action==OXP_SHELL_TASK)return desktop_shell_task(desktop,index);
    if(action==OXP_SHELL_FOCUS)return desktop_shell_focus(desktop,index);
    return false;
#endif
}
bool shell_bridge_window(uint32_t index,oxp_shell_window_t *out){
#ifdef OPENXP_SHELL_HOST_TEST
    (void)index;(void)out;return false;
#else
    if(!out||!desktop||index>=DESKTOP_WINDOWS)return false;
    *out=(oxp_shell_window_t){.index=index,
        .visible=desktop->windows[index].open,
        .minimized=desktop->windows[index].minimized,
        .active=(desktop_top_window(desktop)==(int)index)};
    const char *name=desktop_shell_title(desktop,index);
    unsigned n=0;while(name[n] && n<sizeof(out->title)-1){out->title[n]=name[n];++n;}
    out->title[n]=0;
    return true;
#endif
}
#ifdef OPENXP_SHELL_HOST_TEST
unsigned shell_bridge_test_pending(void){return (qhead+SHELL_INPUT_CAP-qtail)%SHELL_INPUT_CAP;}
#endif
