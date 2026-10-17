#ifndef OPENXP_GUI_NATIVE_H
#define OPENXP_GUI_NATIVE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "editor_keycodes.h"
/* Experimental single-CPU, kernel-hosted native window broker. One window per
 * PID, up to three concurrent owners. A window is *not* a shared framebuffer. */
#define NATIVE_MAX_WINDOWS 3u
#define NATIVE_TITLE_MAX 48u
#define NATIVE_TEXT_MAX 384u
#define NATIVE_EVENT_CAP 32u
/* A separate, fixed-size RGB565 surface for each native owner.
 * Pixels are never directly mapped writable into userspace. */
#define NATIVE_SURFACE_W 640u
#define NATIVE_SURFACE_H 360u
#define NATIVE_DRAW_MAX 32u
#define NATIVE_DRAW_FILL 1u
#define NATIVE_DRAW_CLEAR 2u
#define NATIVE_DRAW_GLYPH 3u
/* Bitmap syscall 34: untrusted pixels copied by kernel, never mapped to apps.
 * Each bounded tile uses RGB565 LE + A8 and belongs to the caller's window. */
#define NATIVE_BITMAP_TILE 32u
typedef struct {
    uint32_t x,y,w,h;
    uint8_t pixels[NATIVE_BITMAP_TILE*NATIVE_BITMAP_TILE*3u];
} native_bitmap_req_t;
_Static_assert(sizeof(native_bitmap_req_t)==3088u,"bitmap syscall ABI");

typedef struct {
    uint32_t op, x, y, w, h, color;
} native_draw_cmd_t;
#define NATIVE_EVENT_CLOSE 0x10000u
#define NATIVE_EVENT_SAVE 0x10001u
#define NATIVE_EVENT_OPEN 0x10002u
#define NATIVE_EVENT_CLICK 0x80000000u
#define NATIVE_EVENT_RIGHT_CLICK 0x40000000u
#define NATIVE_EVENT_UP 0x10003u
#define NATIVE_EVENT_DOWN 0x10004u
#define NATIVE_EVENT_LEFT 0x10005u
#define NATIVE_EVENT_RIGHT 0x10006u
#define NATIVE_EVENT_RESIZE 0x10007u
#define NATIVE_EVENT_POINTER_LEAVE 0x10008u
#define NATIVE_EVENT_SHIFT_TAB 0x10009u
/* ABI: top 4 bits = kind, x bits 10..19 and y bits 0..9,
 * both window-relative INCLUDING decorations. Legacy click is unchanged. */
#define NATIVE_EVENT_POINTER_MOVE 0x20000000u
#define NATIVE_EVENT_POINTER_DOWN 0x10000000u
#define NATIVE_EVENT_POINTER_UP 0x08000000u
#define NATIVE_EVENT_POINTER_MASK 0x38000000u
/* Clicks pack x in bits 10..19 and y in bits 0..9; coordinates are
 * window-relative, including the title bar. 10-bit fields are bounded. */
typedef struct {
    uint64_t owner;
    bool active,dirty,closing,pointer_events;
    char title[NATIVE_TITLE_MAX];
    char text[NATIVE_TEXT_MAX];
    uint32_t events[NATIVE_EVENT_CAP];
    unsigned head,tail;
    /* Internal scheduler thread waiting for an event, never a userspace ptr. */
    uint64_t waiter;
    /* Service-requested compositor state, copied from trusted kernel storage.
     * Generation counters let the desktop apply each command just once. */
    uint32_t wm_x, wm_y, wm_width, wm_height;
    uint32_t wm_geometry_serial, wm_focus_serial;
    uint32_t content_generation;
    /* Logical scanlines changed since the authorized compositor last polled.
     * Half-open bounds; first == last means no pending pixel damage. */
    uint32_t damage_y0,damage_y1;
    uint32_t canvas_w,canvas_h;
    /* Last completed frame size; resize keeps that frame until repaint. */
    uint32_t surface_w,surface_h;
} native_window_t;
void native_reset(void);
bool native_open(uint64_t pid,const char *title,size_t n);
bool native_set_text(uint64_t pid,const char *text,size_t n);
uint32_t native_next_event(uint64_t pid);
bool native_pointer_enable(uint64_t pid);
bool native_pointer_enabled_slot(unsigned slot);
/* Single-CPU atomic dequeue+park: 0 waits indefinitely, >0 waits that many
 * timer ticks (less than 2^63); return 0 on timeout/unavailable scheduler.
 * Native applications can still use native_next_event for nonblocking reads. */
uint32_t native_wait_event(uint64_t pid, uint64_t timeout_ticks);
int native_slot_for_owner(uint64_t pid);
void native_push_event_slot(unsigned slot,uint32_t event);
void native_close_request_slot(unsigned slot);
void native_set_window_size(unsigned slot,unsigned width,unsigned height);
uint64_t native_canvas_size(uint64_t pid);
void native_owner_exit(uint64_t pid);
native_window_t native_snapshot_slot(unsigned slot);
/* Only the authorized window-manager syscall consumes these dirty bounds. */
native_window_t native_snapshot_take_damage_slot(unsigned slot);
bool native_dirty_take(void);
/* Only reachable from the authorized IPC server syscall after peer lookup.
 * No framebuffer address or arbitrary PID is exposed to userspace. */
bool native_wm_manage(uint64_t client_pid, uint32_t action, uint32_t x, uint32_t y,
                      uint32_t width, uint32_t height);
/* Accept a bounded batch atomically: ownership and all rectangles are
 * verified before modifying any pixel. Colors are 0x00RRGGBB. */
bool native_draw(uint64_t pid, const native_draw_cmd_t *cmds, size_t count);
bool native_bitmap(uint64_t pid,const native_bitmap_req_t *req);
bool native_has_surface(unsigned slot);
bool native_copy_surface_row(unsigned slot, unsigned y, uint16_t *dst, size_t cap);
#ifdef OPENXP_NATIVE_HOST_TEST
uint16_t native_test_pixel(unsigned slot, unsigned x, unsigned y);
#endif
#ifdef OPENXP_CAPIPC_SELFTEST
/* Test-only counters verify live Ring 3 service-to-compositor requests. */
typedef struct {uint32_t geometry,raise,close;} native_wm_test_counts_t;
native_wm_test_counts_t native_wm_test_counts(void);
#endif
/* Compatibility interface for existing one-window tests. */
native_window_t native_snapshot(void);
void native_push_event(uint32_t event);
void native_close_request(void);
#endif
