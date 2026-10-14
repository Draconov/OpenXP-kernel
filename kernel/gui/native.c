#include "gui/native.h"
#include "gui/native_viewport.h"
#include "user/wm_display.h"
#include "video/fb.h"
#if !defined(OPENXP_NATIVE_HOST_TEST) || defined(OPENXP_NATIVE_WAIT_HOST_TEST)
#include "sched/scheduler.h"
#endif
#ifndef OPENXP_NATIVE_HOST_TEST
#include "arch/x86_64/irq_state.h"
#endif
/* A small per-PID window broker. The kernel still owns composition, but each
 * user process gets separate text, event queue and lifecycle information. */
static native_window_t windows[NATIVE_MAX_WINDOWS];
static bool dirty_any;
static uint16_t pixels[NATIVE_MAX_WINDOWS][NATIVE_SURFACE_W * NATIVE_SURFACE_H];
static bool painted[NATIVE_MAX_WINDOWS];
static uint32_t wm_serial_counter;
static uint32_t content_serial;
#ifdef OPENXP_CAPIPC_SELFTEST
static native_wm_test_counts_t wm_test_counts;
#endif
static uint32_t content_version(void) {
    if (++content_serial == 0) ++content_serial;
    return content_serial;
}
static uint32_t next_wm_serial(void){
    ++wm_serial_counter;
    if(!wm_serial_counter)++wm_serial_counter;
    return wm_serial_counter;
}
static uint16_t rgb565(uint32_t rgb) {
    return (uint16_t)((((rgb >> 16) & 0xffu) >> 3u) << 11u |
                      ((((rgb >> 8) & 0xffu) >> 2u) << 5u) |
                      ((rgb & 0xffu) >> 3u));
}
static uint64_t lock_irq(void){
#ifndef OPENXP_NATIVE_HOST_TEST
    return irq_save_disable();
#else
    return 0;
#endif
}
static void unlock_irq(uint64_t flags){
#ifndef OPENXP_NATIVE_HOST_TEST
    irq_restore(flags);
#else
    (void)flags;
#endif
}
static int find_owner(uint64_t pid){
    if(!pid)return -1;
    for(unsigned i=0;i<NATIVE_MAX_WINDOWS;++i)
        if(windows[i].active&&windows[i].owner==pid)return (int)i;
    return -1;
}
#ifdef OPENXP_CAPIPC_SELFTEST
native_wm_test_counts_t native_wm_test_counts(void){
    uint64_t flags=lock_irq();
    native_wm_test_counts_t copy=wm_test_counts;
    unlock_irq(flags);
    return copy;
}
#endif
static void wake_thread(uint64_t waiter) {
#if !defined(OPENXP_NATIVE_HOST_TEST) || defined(OPENXP_NATIVE_WAIT_HOST_TEST)
    if (waiter) (void)scheduler_wake(waiter); /* caller holds IF=0 */
#else
    (void)waiter;
#endif
}
void native_reset(void){
    uint64_t flags=lock_irq();
    for(unsigned i=0;i<NATIVE_MAX_WINDOWS;++i){
        wake_thread(windows[i].waiter);
        windows[i]=(native_window_t){0}; painted[i]=false;
    }
    dirty_any=true;
    unlock_irq(flags);
}
bool native_open(uint64_t pid,const char *title,size_t n){
    if(!pid||!title||!n||n>=NATIVE_TITLE_MAX)return false;
    for(size_t i=0;i<n;++i)if((unsigned char)title[i]<32||(unsigned char)title[i]>126)return false;
    uint64_t flags=lock_irq();
    int slot=find_owner(pid);
    if(slot<0){
        for(unsigned i=0;i<NATIVE_MAX_WINDOWS;++i)
            if(!windows[i].active){slot=(int)i;break;}
    }
    if(slot<0){unlock_irq(flags);return false;}
    native_window_t *w=&windows[slot];
    /* Reopening an existing PID must not strand its event waiter. */
    wake_thread(w->waiter);
    *w=(native_window_t){0};w->active=true;w->owner=pid;w->dirty=true;
    w->content_generation=content_version();
    w->canvas_w=NATIVE_SURFACE_W;w->canvas_h=NATIVE_SURFACE_H;
    for(size_t i=0;i<n;++i)w->title[i]=title[i];
    /* A recycled PID slot must never expose the previous owner's pixels,
     * even if the new owner only paints a small rectangle. */
    for(unsigned pixel=0;pixel<NATIVE_SURFACE_W*NATIVE_SURFACE_H;++pixel)
        pixels[slot][pixel]=0;
    painted[slot]=false;
    dirty_any=true;
    unlock_irq(flags);return true;
}
bool native_set_text(uint64_t pid,const char *text,size_t n){
    if(!pid||!text||n>=NATIVE_TEXT_MAX)return false;
    for(size_t i=0;i<n;++i){
        unsigned char c=(unsigned char)text[i];
        if(c!='\n'&&(c<32||c>126))return false;
    }
    uint64_t flags=lock_irq();
    int slot=find_owner(pid);
    if(slot<0){unlock_irq(flags);return false;}
    native_window_t *w=&windows[slot];
    for(size_t i=0;i<n;++i)w->text[i]=text[i];
    w->text[n]=0;w->dirty=true;dirty_any=true;
    w->content_generation=content_version();
    unlock_irq(flags);return true;
}
static uint32_t dequeue(native_window_t *w) {
    /* Close is a sticky control signal, not a droppable FIFO entry. */
    if (w->closing) return NATIVE_EVENT_CLOSE;
    if (w->head == w->tail) return 0;
    uint32_t event = w->events[w->tail];
    w->tail = (w->tail + 1u) % NATIVE_EVENT_CAP;
    return event;
}
uint32_t native_next_event(uint64_t pid){
    uint64_t flags=lock_irq();
    int slot=find_owner(pid);
    uint32_t event=slot<0?0:dequeue(&windows[slot]);
    unlock_irq(flags);
    return event;
}
uint32_t native_wait_event(uint64_t pid, uint64_t timeout_ticks) {
    if (timeout_ticks >= (UINT64_C(1) << 63)) return 0;
    /* The queue check, waiter registration and scheduler state transition are
     * one IRQ-disabled transaction: interrupts cannot deliver an event in
     * between and cause a permanently lost wakeup. */
    uint64_t flags=lock_irq();
    int slot=find_owner(pid);
    if (slot < 0) { unlock_irq(flags); return NATIVE_EVENT_CLOSE; }
    native_window_t *w=&windows[slot];
    uint32_t event=dequeue(w);
    if (event) { unlock_irq(flags); return event; }
#if !defined(OPENXP_NATIVE_HOST_TEST) || defined(OPENXP_NATIVE_WAIT_HOST_TEST)
    const uint64_t tid=scheduler_current_id();
    if (tid && !w->waiter) {
        w->waiter=tid;
        bool parked=timeout_ticks ? scheduler_sleep_current_locked(timeout_ticks)
                                  : scheduler_park_current_locked();
        /* After resumption, the slot may have been recycled. Never consume
         * another owner's event or clear a new owner's waiter. */
        if (find_owner(pid)==slot && windows[slot].waiter==tid) {
            windows[slot].waiter=0;
            event=dequeue(&windows[slot]);
        } else if (find_owner(pid)<0) {
            event=NATIVE_EVENT_CLOSE;
        }
        (void)parked;
    }
#endif
    unlock_irq(flags);
    return event;
}
int native_slot_for_owner(uint64_t pid){
    uint64_t flags=lock_irq();int slot=find_owner(pid);unlock_irq(flags);return slot;
}
static void enqueue(native_window_t *w,uint32_t e){
    unsigned next=(w->head+1)%NATIVE_EVENT_CAP;
    if(next==w->tail)w->tail=(w->tail+1)%NATIVE_EVENT_CAP;
    w->events[w->head]=e;w->head=next;
}
void native_push_event_slot(unsigned slot,uint32_t event){
    if(slot>=NATIVE_MAX_WINDOWS||!event)return;
    uint64_t flags=lock_irq();
    if(windows[slot].active) {
        if(event==NATIVE_EVENT_CLOSE) windows[slot].closing=true;
        else if(!windows[slot].closing) enqueue(&windows[slot],event);
        wake_thread(windows[slot].waiter);
    }
    unlock_irq(flags);
}
void native_close_request_slot(unsigned slot){native_push_event_slot(slot,NATIVE_EVENT_CLOSE);}
/* Trusted desktop geometry only: never accept an app-supplied size. */
void native_set_window_size(unsigned slot,unsigned width,unsigned height){
    if(slot>=NATIVE_MAX_WINDOWS)return;
    oxp_native_canvas_t c=oxp_native_canvas(width,height);
    uint64_t flags=lock_irq();
    native_window_t *w=&windows[slot];
    if(w->active && (w->canvas_w!=c.w || w->canvas_h!=c.h)){
        w->canvas_w=c.w;w->canvas_h=c.h;
        /* Retain the last valid frame until the app repaints: clearing it
         * on every grip movement would visibly flicker. Reopen clears it. */
        w->dirty=true;dirty_any=true;
        if(!w->closing){enqueue(w,NATIVE_EVENT_RESIZE);wake_thread(w->waiter);}
    }
    unlock_irq(flags);
}
uint64_t native_canvas_size(uint64_t pid){
    uint64_t flags=lock_irq();int slot=find_owner(pid);
    uint64_t result=slot<0?0:((uint64_t)windows[slot].canvas_w<<32)|windows[slot].canvas_h;
    unlock_irq(flags);return result;
}

void native_owner_exit(uint64_t pid){
    uint64_t flags=lock_irq();
    int slot=find_owner(pid);
    if(slot>=0){
        wake_thread(windows[slot].waiter);
        windows[slot]=(native_window_t){0};painted[slot]=false;dirty_any=true;
    }
    unlock_irq(flags);
}
native_window_t native_snapshot_slot(unsigned slot){
    if(slot>=NATIVE_MAX_WINDOWS)return (native_window_t){0};
    uint64_t flags=lock_irq();native_window_t copy=windows[slot];unlock_irq(flags);return copy;
}
bool native_wm_manage(uint64_t client_pid, uint32_t action, uint32_t x, uint32_t y,
                      uint32_t width, uint32_t height) {
    if(!client_pid)return false;
    if(action==OPENXP_WM_SET_RECT) {
        /* Fixed virtual screen for the initial 1024x768 framebuffer mode.
         * The bottom 40 pixels are reserved for the taskbar. */
        if(width<120u || width>640u || height<80u || height>480u ||
           x>1024u-width || y>728u-height)return false;
    }else if(action!=OPENXP_WM_RAISE && action!=OPENXP_WM_REQUEST_CLOSE)return false;
    uint64_t flags=lock_irq();
    int slot=find_owner(client_pid);
    if(slot<0){unlock_irq(flags);return false;}
    native_window_t *w=&windows[slot];
    if(action==OPENXP_WM_SET_RECT){
        w->wm_x=x;w->wm_y=y;w->wm_width=width;w->wm_height=height;
        w->wm_geometry_serial=next_wm_serial();
    } else if(action==OPENXP_WM_RAISE){
        w->wm_focus_serial=next_wm_serial();
    } else {
        w->closing=true;
        wake_thread(w->waiter);
    }
#ifdef OPENXP_CAPIPC_SELFTEST
    if(action==OPENXP_WM_SET_RECT)++wm_test_counts.geometry;
    else if(action==OPENXP_WM_RAISE)++wm_test_counts.raise;
    else ++wm_test_counts.close;
#endif
    w->dirty=true;dirty_any=true;
    w->content_generation=content_version();
    unlock_irq(flags);
    return true;
}
bool native_dirty_take(void){
    uint64_t flags=lock_irq();bool was_dirty=dirty_any;dirty_any=false;
    for(unsigned i=0;i<NATIVE_MAX_WINDOWS;++i)windows[i].dirty=false;
    unlock_irq(flags);return was_dirty;
}
native_window_t native_snapshot(void){return native_snapshot_slot(0);}
void native_push_event(uint32_t event){native_push_event_slot(0,event);}
void native_close_request(void){native_close_request_slot(0);}

/* Commands are copied into kernel storage by the syscall boundary, so no
 * userspace pointer is dereferenced by this renderer. */
bool native_draw(uint64_t pid, const native_draw_cmd_t *cmds, size_t count) {
    if (!pid || !cmds || count == 0 || count > NATIVE_DRAW_MAX) return false;
    for (size_t i=0; i<count; ++i) {
        const native_draw_cmd_t *c=&cmds[i];
        if (c->color & 0xff000000u) return false;
        if (c->op == NATIVE_DRAW_CLEAR) continue;
        if (c->op == NATIVE_DRAW_GLYPH) {
            /* w = printable ASCII code; h = glyph scale 1 or 2. */
            if(c->w<32 || c->w>126 || c->h<1 || c->h>2 ||
               c->x>NATIVE_SURFACE_W-5u*c->h ||
               c->y>NATIVE_SURFACE_H-7u*c->h)return false;
            continue;
        }
        if (c->op != NATIVE_DRAW_FILL || !c->w || !c->h ||
            c->x >= NATIVE_SURFACE_W || c->y >= NATIVE_SURFACE_H ||
            c->w > NATIVE_SURFACE_W-c->x || c->h > NATIVE_SURFACE_H-c->y)
            return false;
    }
    uint64_t flags=lock_irq();
    int slot=find_owner(pid);
    if (slot<0) { unlock_irq(flags); return false; }
    for (size_t i=0; i<count; ++i) {
        const native_draw_cmd_t *c=&cmds[i];
        uint16_t color=rgb565(c->color);
        uint32_t x=0,y=0,w=NATIVE_SURFACE_W,h=NATIVE_SURFACE_H;
        if (c->op == NATIVE_DRAW_GLYPH) {
            const uint8_t *bits=fb_glyph5x7((char)c->w);
            for(unsigned row=0;row<7;++row)for(unsigned col=0;col<5;++col){
                if(!(bits[row]&(1u<<(4u-col))))continue;
                for(unsigned dy=0;dy<c->h;++dy)for(unsigned dx=0;dx<c->h;++dx)
                    pixels[slot][(c->y+row*c->h+dy)*NATIVE_SURFACE_W +
                                 c->x+col*c->h+dx]=color;
            }
            continue;
        }
        if (c->op == NATIVE_DRAW_FILL) { x=c->x;y=c->y;w=c->w;h=c->h; }
        for (uint32_t row=y;row<y+h;++row)
            for (uint32_t col=x;col<x+w;++col)
                pixels[slot][row*NATIVE_SURFACE_W+col]=color;
    }
    painted[slot]=true;windows[slot].dirty=true;dirty_any=true;
    windows[slot].content_generation=content_version();
    unlock_irq(flags);return true;
}
bool native_has_surface(unsigned slot) {
    if(slot>=NATIVE_MAX_WINDOWS)return false;
    uint64_t flags=lock_irq();
    bool result=windows[slot].active && painted[slot];
    unlock_irq(flags);return result;
}
bool native_copy_surface_row(unsigned slot,unsigned y,uint16_t *dst,size_t cap) {
    if(slot>=NATIVE_MAX_WINDOWS||y>=NATIVE_SURFACE_H||!dst||cap<NATIVE_SURFACE_W)return false;
    uint64_t flags=lock_irq();
    if(!windows[slot].active || !painted[slot]) {unlock_irq(flags);return false;}
    for(unsigned x=0;x<NATIVE_SURFACE_W;++x)
        dst[x]=pixels[slot][y*NATIVE_SURFACE_W+x];
    unlock_irq(flags);return true;
}
#ifdef OPENXP_NATIVE_HOST_TEST
uint16_t native_test_pixel(unsigned slot,unsigned x,unsigned y) {
    if(slot>=NATIVE_MAX_WINDOWS||x>=NATIVE_SURFACE_W||y>=NATIVE_SURFACE_H)return 0;
    uint64_t flags=lock_irq();uint16_t result=pixels[slot][y*NATIVE_SURFACE_W+x];
    unlock_irq(flags);return result;
}
#endif
