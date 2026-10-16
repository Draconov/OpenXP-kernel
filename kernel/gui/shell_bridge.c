#include "gui/shell_bridge.h"
#include "gui/desktop.h"
#include "arch/x86_64/irq_state.h"
#ifndef OPENXP_SHELL_HOST_TEST
#include "arch/x86_64/timer.h"
#endif
#include <stddef.h>
#define SHELL_INPUT_CAP 64u
static oxp_shell_draw_t frame[OXP_SHELL_DRAW_MAX];
static uint32_t frame_count;
/* Double-buffered, bounded RGB565 scene layers. Only the *back* buffer is
 * writable by the syscall; the compositor reads the published bank. */
static uint16_t raster[OXP_SHELL_LAYERS][2][OXP_SHELL_RASTER_H][OXP_SHELL_RASTER_W];
static uint8_t rows_seen[OXP_SHELL_LAYERS][OXP_SHELL_RASTER_H];
static uint32_t rows_total[OXP_SHELL_LAYERS], active_bank[OXP_SHELL_LAYERS];
static bool published[OXP_SHELL_LAYERS];
static uint64_t raster_pid;
static uint32_t window_order[DESKTOP_WINDOWS];
static uint32_t window_count;
static bool has_window_plan;
static oxp_shell_event_t queue[SHELL_INPUT_CAP];
static uint32_t qhead,qtail;
static bool running,dirty,menu_is_open;
static uint64_t service_pid, last_frame_tick;
static uint64_t scene_version;
static bool pointer_captured, shell_press_captured;
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
    uint64_t flags=lock_state();
    ++scene_version;running=false;menu_is_open=false;dirty=true;frame_count=0;window_count=0;has_window_plan=false;qhead=qtail=0;service_pid=0;last_frame_tick=0;pointer_captured=false;shell_press_captured=false;
    raster_pid=0;
    for(unsigned l=0;l<OXP_SHELL_LAYERS;++l){
        published[l]=false;active_bank[l]=0;rows_total[l]=0;
        for(unsigned y=0;y<OXP_SHELL_RASTER_H;++y)rows_seen[l][y]=0;
    }
    unlock_state(flags);
}
void shell_bridge_attach(void *d){desktop=(desktop_t *)d;}
void shell_bridge_shutdown(void){shell_bridge_reset();}
void shell_bridge_owner_exit(uint64_t pid){if(pid && pid==service_pid)shell_bridge_shutdown();}
void shell_bridge_watchdog(uint64_t now){
    /* Ring 3 cannot indefinitely capture keyboard/mouse input if it hangs. */
    if(running && now-last_frame_tick>300u)shell_bridge_shutdown();
}
uint64_t shell_bridge_scene_version(void){
    uint64_t flags=lock_state();
    uint64_t version=scene_version;
    unlock_state(flags);
    return version;
}
void shell_bridge_request_redraw(void){
    uint64_t flags=lock_state();dirty=true;unlock_state(flags);
}
bool shell_bridge_take_dirty(void){uint64_t f=lock_state();bool old=dirty;dirty=false;unlock_state(f);return old;}
bool shell_bridge_active(void){return running && desktop!=NULL;}
bool shell_bridge_menu_open(void){return shell_bridge_active() && menu_is_open;}
bool shell_bridge_has_window_plan(void){return shell_bridge_active() && has_window_plan;}
static bool valid(const oxp_shell_draw_t *c){
    if(c->op==OXP_SHELL_COMPOSE_BEGIN || c->op==OXP_SHELL_COMPOSE_WINDOW)
        return c->y==0 && c->w==0 && c->h==0 && c->color==0 &&
               (c->op==OXP_SHELL_COMPOSE_BEGIN ? c->x==0 : c->x<DESKTOP_WINDOWS) &&
               !c->text[0] &&
               !c->text[1] && !c->text[2] && !c->text[3] &&
               !c->text[4] && !c->text[5] && !c->text[6] &&
               !c->text[7]; /* Remaining bytes are ignored by kernel. */
    if(c->color&0xff000000u)return false;
    if(c->op==OXP_SHELL_RECT || c->op==OXP_SHELL_BG_RECT)
        return c->w && c->h && c->x<1024u && c->y<768u &&
               c->w<=1024u-c->x && c->h<=768u-c->y;
    if((c->op!=OXP_SHELL_TEXT && c->op!=OXP_SHELL_BG_TEXT) || c->w<1u || c->w>2u ||
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
    uint32_t requested_order[DESKTOP_WINDOWS];
    uint32_t requested_count=0;
    uint32_t seen=0;
    bool began=false;
    for(uint32_t i=0;i<count;++i){
        const oxp_shell_draw_t *cmd=&commands[i];
        if(!valid(cmd))return false;
        if(cmd->op==OXP_SHELL_COMPOSE_BEGIN){
            if(began)return false;
            began=true;
        }else if(cmd->op==OXP_SHELL_COMPOSE_WINDOW){
            if(!began || requested_count>=DESKTOP_WINDOWS)return false;
            uint32_t bit=UINT32_C(1)<<cmd->x;
            if(seen&bit)return false;
            seen|=bit;
            requested_order[requested_count++]=cmd->x;
        }
    }
    uint64_t flags=lock_state();
    bool changed=!running || service_pid!=pid || frame_count!=count;
    if(!changed){
        /* Windowd refreshes its heartbeat often; identical frames must not
           trigger a 1024x768 full-screen repaint every quarter second. */
        const uint8_t *a=(const uint8_t *)frame;
        const uint8_t *b=(const uint8_t *)commands;
        for(size_t j=0;j<count*sizeof(*commands);++j)
            if(a[j]!=b[j]){changed=true;break;}
    }
    if(changed){
        ++scene_version;
        for(uint32_t i=0;i<count;++i)frame[i]=commands[i];
        frame_count=count;dirty=true;
        has_window_plan=began;
        window_count=requested_count;
        for(uint32_t j=0;j<requested_count;++j)window_order[j]=requested_order[j];
    }
    running=true;service_pid=pid;last_frame_tick=tick;
    unlock_state(flags);return true;
}
bool shell_bridge_window_order(uint32_t *indices,uint32_t capacity,uint32_t *length){
    if(!indices||!length||capacity<DESKTOP_WINDOWS)return false;
    uint64_t flags=lock_state();
    bool ready=running && desktop!=NULL && has_window_plan;
    if(ready){
        for(uint32_t i=0;i<window_count;++i)indices[i]=window_order[i];
        *length=window_count;
    }
    unlock_state(flags);
    return ready;
}
bool shell_bridge_raster_row(uint64_t pid,const oxp_shell_raster_row_t *row){
    if(!pid || !row || row->layer>=OXP_SHELL_LAYERS ||
       row->y>=OXP_SHELL_RASTER_H)return false;
    uint64_t flags=lock_state();
    /* Rows cannot silently transfer from one service incarnation to another. */
    if(raster_pid && raster_pid!=pid){unlock_state(flags);return false;}
    if(running && service_pid!=pid){unlock_state(flags);return false;}
    if(!raster_pid)raster_pid=pid;
#ifndef OPENXP_SHELL_HOST_TEST
    if(running && service_pid==pid)last_frame_tick=timer_ticks();
#endif
    unsigned layer=row->layer;
    unsigned bank=active_bank[layer]^1u;
    for(unsigned x=0;x<OXP_SHELL_RASTER_W;++x)
        raster[layer][bank][row->y][x]=row->pixels[x];
    if(!rows_seen[layer][row->y]){
        rows_seen[layer][row->y]=1;
        ++rows_total[layer];
    }
    unlock_state(flags);
    return true;
}
bool shell_bridge_raster_commit(uint64_t pid,uint32_t mask){
    if(!pid || !mask || (mask & ~(OXP_SHELL_MASK_BACKGROUND|OXP_SHELL_MASK_OVERLAY|OXP_SHELL_MASK_NATIVE)))
        return false;
    uint64_t flags=lock_state();
    if(!running || service_pid!=pid || raster_pid!=pid){
        unlock_state(flags);return false;
    }
    for(unsigned l=0;l<OXP_SHELL_LAYERS;++l)if((mask&(1u<<l)) && rows_total[l]!=OXP_SHELL_RASTER_H){
        unlock_state(flags);return false;
    }
    for(unsigned l=0;l<OXP_SHELL_LAYERS;++l)if(mask&(1u<<l)){
        active_bank[l]^=1u;
        published[l]=true;
        rows_total[l]=0;
        for(unsigned y=0;y<OXP_SHELL_RASTER_H;++y)rows_seen[l][y]=0;
    }
    ++scene_version;
    dirty=true;
    unlock_state(flags);
    return true;
}
bool shell_bridge_raster_ready(uint32_t layer){
    return layer<OXP_SHELL_LAYERS && shell_bridge_active() && published[layer];
}
static void raster_compose(fb_surface_t *target,unsigned layer){
    if(!target || !target->pixels)return;
    /* Frequent title-bar drags use tiny framebuffer clip rectangles. Avoid
     * scanning the other ~786,000 pixels for a 9x13 mouse/dirty-region update. */
    int x0=0,y0=0,x1=target->width,y1=target->height;
    if(x1>(int)OXP_SHELL_RASTER_W)x1=OXP_SHELL_RASTER_W;
    if(y1>(int)OXP_SHELL_RASTER_H)y1=OXP_SHELL_RASTER_H;
    if(target->clip_active){
        if(x0<target->clip_x)x0=target->clip_x;
        if(y0<target->clip_y)y0=target->clip_y;
        if(x1>target->clip_x+target->clip_w)x1=target->clip_x+target->clip_w;
        if(y1>target->clip_y+target->clip_h)y1=target->clip_y+target->clip_h;
    }
    if(x0>=x1 || y0>=y1)return;
    unsigned bank=active_bank[layer];
    for(int y=y0;y<y1;++y){
        const uint16_t *src=raster[layer][bank][y];
        if(layer==OXP_SHELL_LAYER_BACKGROUND){
            fb_blit565_row(target,x0,y,src+x0,x1-x0);
        }else{
            int x=x0;
            while(x<x1){
                while(x<x1 && src[x]==OXP_SHELL_TRANSPARENT)++x;
                int start=x;
                while(x<x1 && src[x]!=OXP_SHELL_TRANSPARENT)++x;
                if(x>start)fb_blit565_row(target,start,y,src+start,x-start);
            }
        }
    }
}
static void compose_pass(fb_surface_t *target,bool background){
    if(!target||!shell_bridge_active())return;
    uint64_t flags=lock_state();
    for(uint32_t i=0;i<frame_count;++i){
        const oxp_shell_draw_t *c=&frame[i];
        /* Composition markers are metadata, not raster commands. */
        if(c->op==OXP_SHELL_COMPOSE_BEGIN ||
           c->op==OXP_SHELL_COMPOSE_WINDOW)continue;
        if((c->op==OXP_SHELL_BG_RECT || c->op==OXP_SHELL_BG_TEXT)!=background)continue;
        uint32_t col=fb_rgb(target,(uint8_t)(c->color>>16),
                                  (uint8_t)(c->color>>8),(uint8_t)c->color);
        if(c->op==OXP_SHELL_RECT || c->op==OXP_SHELL_BG_RECT)fb_rect(target,(int)c->x,(int)c->y,(int)c->w,(int)c->h,col);
        else fb_text_limit(target,(int)c->x,(int)c->y,c->text,
                           OXP_SHELL_TEXT_MAX-1u,(int)c->w,col);
    }
    unlock_state(flags);
}
void shell_bridge_compose_native(fb_surface_t *target){
    if(shell_bridge_raster_ready(OXP_SHELL_LAYER_NATIVE))
        raster_compose(target,OXP_SHELL_LAYER_NATIVE);
}
void shell_bridge_compose(fb_surface_t *target){
    if(shell_bridge_raster_ready(OXP_SHELL_LAYER_OVERLAY))raster_compose(target,OXP_SHELL_LAYER_OVERLAY);
    else compose_pass(target,false);
}
void shell_bridge_compose_background(fb_surface_t *target){
    if(shell_bridge_raster_ready(OXP_SHELL_LAYER_BACKGROUND))raster_compose(target,OXP_SHELL_LAYER_BACKGROUND);
    else compose_pass(target,true);
}
static void push(const oxp_shell_event_t *event){
    uint64_t flags=lock_state();
    /* Continuous PS/2 motion must not evict a pending key, press or release.
     * Keep the newest coordinate when the queue's tail event is also motion.
     * Never coalesce across a button transition or reorder clicks. */
    if(event->type==OXP_SHELL_EVENT_POINTER &&
       event->flags==OXP_SHELL_POINTER_MOVE && qhead!=qtail){
        uint32_t last=(qhead+SHELL_INPUT_CAP-1u)%SHELL_INPUT_CAP;
        if(queue[last].type==OXP_SHELL_EVENT_POINTER &&
           queue[last].flags==OXP_SHELL_POINTER_MOVE){
            queue[last]=*event;
            unlock_state(flags);
            return;
        }
    }
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
    }else if((pointer_captured || shell_press_captured) &&
             (e->kind==INPUT_MOVE || e->kind==INPUT_BUTTON)){
        /* A captured gesture is completed in Ring 3 even when the pointer
         * moves off-screen or over a different process. */
        m.type=OXP_SHELL_EVENT_POINTER;
        m.flags=e->left?OXP_SHELL_POINTER_MOVE:OXP_SHELL_POINTER_UP;
        m.x=(uint32_t)e->x;m.y=(uint32_t)e->y;
        if(!e->left){pointer_captured=false;shell_press_captured=false;}
    }else if(e->kind==INPUT_MOVE){
        /* Forward motion without consuming it: clients still receive their
         * regular mouse input, while windowd can update hover artwork.
         * Successive motion events coalesce in push(). */
        m.type=OXP_SHELL_EVENT_POINTER;m.flags=OXP_SHELL_POINTER_MOVE;
        m.x=(uint32_t)e->x;m.y=(uint32_t)e->y;
        push(&m);return false;
    }else if(e->kind==INPUT_BUTTON && !e->left){
        /* Also observe ordinary client releases without consuming them.
         * Cross-window file drops need a global UP event, while the source
         * client must retain its own release for text/file gestures. */
        m.type=OXP_SHELL_EVENT_POINTER;m.flags=OXP_SHELL_POINTER_UP;
        m.x=(uint32_t)e->x;m.y=(uint32_t)e->y;
        push(&m);return false;
    }else if(e->kind==INPUT_BUTTON && e->left){
        /* Shell controls use press/release rather than launching on mouse
         * down. A press on an obscured desktop icon belongs to the window. */
        bool icon=!menu_visible && e->x>=14 && e->x<115 &&
                  e->y>=23 && e->y<23+5*109 &&
                  (e->y-23)%109<92;
        if(icon && desktop){
            for(int z=DESKTOP_WINDOWS-1;z>=0;--z){
                unsigned i=has_window_plan && (uint32_t)z<window_count?
                    window_order[z]:(unsigned)desktop->order[z];
                if(has_window_plan && (uint32_t)z>=window_count)continue;
                desktop_window_t *w=&desktop->windows[i];
                if(w->open && !w->minimized && e->x>=w->x && e->x<w->x+w->w &&
                   e->y>=w->y && e->y<w->y+w->h){icon=false;break;}
            }
        }
        if(menu_visible || e->y>=height-42 || icon){
            shell_press_captured=true;
            m.type=OXP_SHELL_EVENT_POINTER;m.flags=OXP_SHELL_POINTER_DOWN;
            m.x=(uint32_t)e->x;m.y=(uint32_t)e->y;
            push(&m);return true;
        }
        /* Capture titlebar drags and bottom-right resize grips, never the
           application viewport itself or trusted caption buttons. */
        if(desktop){
            for(int z=DESKTOP_WINDOWS-1;z>=0;--z){
                if(has_window_plan && (uint32_t)z>=window_count)continue;
                unsigned i=has_window_plan?window_order[z]:(unsigned)desktop->order[z];
                desktop_window_t *w=&desktop->windows[i];
                if(!w->open || w->minimized || e->x<w->x || e->x>=w->x+w->w ||
                   e->y<w->y || e->y>=w->y+w->h)continue;
                bool corner=!w->maximized &&
                    e->x>=w->x+w->w-18 && e->y>=w->y+w->h-18;
                bool title=!w->maximized && e->y<w->y+33 &&
                    e->x<w->x+w->w-84;
                if(!corner && !title)break;
                pointer_captured=true;
                m.type=OXP_SHELL_EVENT_POINTER;
                m.flags=OXP_SHELL_POINTER_DOWN;
                m.x=(uint32_t)e->x;m.y=(uint32_t)e->y;
                break;
            }
        }
        if(m.type==OXP_SHELL_EVENT_POINTER){push(&m);return true;}
        return false;
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
bool shell_bridge_action(uint32_t action,uint32_t index,uint64_t payload){
    if(!shell_bridge_active() || !desktop)return false;
    if(action==OXP_SHELL_STOP){shell_bridge_shutdown();return true;}
    dirty=true;
    if(action==OXP_SHELL_MENU){menu_is_open=index!=0;return true;}
    /* Only the capability-holding Ring 3 windowd can set global appearance.
       Keep the legacy desktop in sync for service crash recovery. */
    if(action==OXP_SHELL_THEME){
        if(index>2u)return false;
        desktop->appearance=index;
        return true;
    }
#ifdef OPENXP_SHELL_HOST_TEST
    (void)index;(void)payload;
    return false;
#else
    if(action==OXP_SHELL_LAUNCH_NATIVE)return desktop_shell_launch_native(desktop,index);
    if(action==OXP_SHELL_MAXIMIZE)return desktop_shell_maximize(desktop,index);
    if(action==OXP_SHELL_RESIZE)return desktop_shell_resize(desktop,index,
          (uint32_t)payload,(uint32_t)(payload>>32));
    if(action==OXP_SHELL_MOVE)return desktop_shell_move(desktop,index,
          (uint32_t)payload,(uint32_t)(payload>>32));
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
    desktop_window_t *w=&desktop->windows[index];
    uint32_t z=0;
    for(;z<DESKTOP_WINDOWS;++z)if(desktop->order[z]==(int)index)break;
    *out=(oxp_shell_window_t){.index=index,
        .x=(uint32_t)w->x,.y=(uint32_t)w->y,.width=(uint32_t)w->w,
        .height=(uint32_t)w->h,.z=z,
        .visible=desktop->windows[index].open,
        .minimized=desktop->windows[index].minimized,
        .maximized=desktop->windows[index].maximized,
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
