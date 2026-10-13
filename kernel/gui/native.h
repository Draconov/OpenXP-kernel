#ifndef OPENXP_GUI_NATIVE_H
#define OPENXP_GUI_NATIVE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Experimental single-CPU, kernel-hosted native window broker. One window per
 * PID, up to three concurrent owners. A window is *not* a shared framebuffer. */
#define NATIVE_MAX_WINDOWS 3u
#define NATIVE_TITLE_MAX 48u
#define NATIVE_TEXT_MAX 384u
#define NATIVE_EVENT_CAP 32u
/* A separate, fixed-size RGB565 surface for each native owner.
 * Pixels are never directly mapped writable into userspace. */
#define NATIVE_SURFACE_W 320u
#define NATIVE_SURFACE_H 180u
#define NATIVE_DRAW_MAX 32u
#define NATIVE_DRAW_FILL 1u
#define NATIVE_DRAW_CLEAR 2u
#define NATIVE_DRAW_GLYPH 3u
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
/* Clicks pack x in bits 10..19 and y in bits 0..9; coordinates are
 * window-relative, including the title bar. 10-bit fields are bounded. */
typedef struct {
    uint64_t owner;
    bool active,dirty,closing;
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
} native_window_t;
void native_reset(void);
bool native_open(uint64_t pid,const char *title,size_t n);
bool native_set_text(uint64_t pid,const char *text,size_t n);
uint32_t native_next_event(uint64_t pid);
/* Single-CPU atomic dequeue+park: 0 waits indefinitely, >0 waits that many
 * timer ticks (less than 2^63); return 0 on timeout/unavailable scheduler.
 * Native applications can still use native_next_event for nonblocking reads. */
uint32_t native_wait_event(uint64_t pid, uint64_t timeout_ticks);
int native_slot_for_owner(uint64_t pid);
void native_push_event_slot(unsigned slot,uint32_t event);
void native_close_request_slot(unsigned slot);
void native_owner_exit(uint64_t pid);
native_window_t native_snapshot_slot(unsigned slot);
bool native_dirty_take(void);
/* Only reachable from the authorized IPC server syscall after peer lookup.
 * No framebuffer address or arbitrary PID is exposed to userspace. */
bool native_wm_manage(uint64_t client_pid, uint32_t action, uint32_t x, uint32_t y,
                      uint32_t width, uint32_t height);
/* Accept a bounded batch atomically: ownership and all rectangles are
 * verified before modifying any pixel. Colors are 0x00RRGGBB. */
bool native_draw(uint64_t pid, const native_draw_cmd_t *cmds, size_t count);
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
