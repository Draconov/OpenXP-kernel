#ifndef OPENXP_WM_PROTOCOL_H
#define OPENXP_WM_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>

/* Versioned, fixed-size messages transmitted through capability IPC.
 * No pointers, packed structures or kernel addresses cross the boundary. */
#define WM_MAGIC UINT32_C(0x31574D4F) /* OMW1 */
#define WM_VERSION 1u
#define WM_REPLY_FLAG UINT32_C(0x80000000)
#define WM_TITLE_SIZE 24u
#define WM_MAX_WINDOWS 6u
#define WM_DESKTOP_WIDTH 1024u
#define WM_DESKTOP_HEIGHT 728u

enum wm_operation { WM_CREATE=1, WM_MOVE=2, WM_FOCUS=3, WM_QUERY=4,
                    WM_CLOSE=5, WM_PING=6, WM_THEME=7, WM_WALLPAPER=8,
                    WM_DESKTOP=9, /* x: 0=hide shortcuts, 1=show */
                    WM_DRAG_BEGIN=10 };
enum wm_result { WM_OK=0, WM_BAD_REQUEST=1, WM_UNAUTHORIZED=2,
                 WM_NOT_FOUND=3, WM_NO_SLOTS=4, WM_BAD_GEOMETRY=5 };
typedef struct {
    uint32_t magic, version, operation, window_id;
    uint32_t x, y, width, height, status;
    char title[WM_TITLE_SIZE];
} wm_packet_t;
_Static_assert(sizeof(wm_packet_t)==60, "IPC wire packet must fit 64 bytes");
/* Separate 64-byte packet: a sanitized short VFS document path, never a
 * pointer or an arbitrary executable. Standard WM packets stay ABI-stable. */
typedef struct {
    uint32_t magic,version,operation;
    char path[52];
} wm_drag_packet_t;
_Static_assert(sizeof(wm_drag_packet_t)==64, "drop packet ABI");

/* Service-side logical window policy. A restricted syscall bridges accepted
 * changes into the Ring 0 compositor without exposing kernel pointers. */
typedef struct {
    uint64_t owner;
    uint32_t generation, z;
    uint32_t x, y, width, height;
    char title[WM_TITLE_SIZE];
    uint8_t used;
} wm_window_t;
typedef struct {
    wm_window_t windows[WM_MAX_WINDOWS];
    uint32_t next_z;
} wm_state_t;

void wm_state_init(wm_state_t *s);
void wm_state_disconnect(wm_state_t *s,uint64_t owner);
void wm_state_process(wm_state_t *s,uint64_t owner,const wm_packet_t *request,wm_packet_t *reply);
uint32_t wm_state_active_window(const wm_state_t *s);

#endif
