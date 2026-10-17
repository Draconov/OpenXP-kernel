#ifndef OPENXP_SHELL_ABI_H
#define OPENXP_SHELL_ABI_H
#include <stdint.h>
/* Native Ring 3 desktop policy / bounded kernel framebuffer renderer ABI.
 * Physical framebuffer and desktop_t never cross into userspace. */
#define OXP_SHELL_DRAW_MAX 128u
/* Versioned, fixed-size scanline transport for userspace-rasterized desktop
 * layers. The CPU scans out through the kernel; no framebuffer is mapped into
 * userspace. Layer 0 is opaque wallpaper; layer 1 has color-key transparency. */
#define OXP_SHELL_RASTER_W 1024u
#define OXP_SHELL_RASTER_H 768u
#define OXP_SHELL_LAYER_BACKGROUND 0u
#define OXP_SHELL_LAYER_OVERLAY 1u
/* Optional Ring 3-native window raster, between legacy windows and taskbar. */
#define OXP_SHELL_LAYER_NATIVE 2u
#define OXP_SHELL_LAYERS 3u
#define OXP_SHELL_MASK_BACKGROUND 1u
#define OXP_SHELL_MASK_OVERLAY 2u
#define OXP_SHELL_MASK_NATIVE 4u
/* Update only staged native rows; retain all other published rows. */
#define OXP_SHELL_MASK_NATIVE_PARTIAL 8u
#define OXP_SHELL_TRANSPARENT 0x0001u
typedef struct {
    uint32_t layer, y;
    uint16_t pixels[OXP_SHELL_RASTER_W];
} oxp_shell_raster_row_t;
_Static_assert(sizeof(oxp_shell_raster_row_t)==2056, "raster row wire ABI");
#define OXP_SHELL_TEXT_MAX 24u
#define OXP_SHELL_RECT 1u
#define OXP_SHELL_TEXT 2u
#define OXP_SHELL_BG_RECT 3u
#define OXP_SHELL_BG_TEXT 4u
/* Ring 3 chooses which windows are composed and their back-to-front order.
 * These commands contain only indices, never framebuffers or memory pointers.
 * A BEGIN without any WINDOW is a valid empty-window composition plan. */
#define OXP_SHELL_COMPOSE_BEGIN 5u
#define OXP_SHELL_COMPOSE_WINDOW 6u
#define OXP_SHELL_OPEN 1u
#define OXP_SHELL_TASK 2u
#define OXP_SHELL_FOCUS 3u
#define OXP_SHELL_STOP 4u
#define OXP_SHELL_MENU 5u
#define OXP_SHELL_CLOSE 6u
#define OXP_SHELL_DESKTOP 7u
#define OXP_SHELL_MOVE 8u
#define OXP_SHELL_LAUNCH_NATIVE 9u
#define OXP_SHELL_THEME 10u
#define OXP_SHELL_MAXIMIZE 11u
#define OXP_SHELL_RESIZE 12u
#define OXP_SHELL_EVENT_KEY 1u
#define OXP_SHELL_EVENT_CLICK 2u
#define OXP_SHELL_EVENT_FOCUS 3u
#define OXP_SHELL_EVENT_CLOSE 4u
#define OXP_SHELL_EVENT_POINTER 5u
#define OXP_SHELL_POINTER_DOWN 1u
#define OXP_SHELL_POINTER_MOVE 2u
#define OXP_SHELL_POINTER_UP 3u
/* Fixed-width ABI across kernel, app and host tests. */
typedef struct {
    uint32_t op, x, y, w, h, color;
    char text[OXP_SHELL_TEXT_MAX];
} oxp_shell_draw_t;
_Static_assert(sizeof(oxp_shell_draw_t)==48, "shell drawing wire ABI");
typedef struct {
    uint32_t type,code,modifiers,character,x,y,flags;
} oxp_shell_event_t;
_Static_assert(sizeof(oxp_shell_event_t)==28,"shell event wire ABI");
typedef struct {
    uint32_t index,visible,active,minimized,maximized;
    uint32_t x,y,width,height,z;
    char title[32];
} oxp_shell_window_t;
_Static_assert(sizeof(oxp_shell_window_t)==72,"window query wire ABI");
/* Only the authorized windowd service can inspect these read-only snapshots.
 * No kernel pointer, framebuffer mapping, or other process's address space
 * is made writable to the service. */
#define OXP_NATIVE_SLOTS 3u
#define OXP_NATIVE_W 640u
#define OXP_NATIVE_H 360u
typedef struct {
    uint32_t active, painted, generation, surface_w, surface_h;
    /* Changed logical scanlines since previous WM snapshot; half-open. */
    uint32_t damage_y0,damage_y1;
    char title[48];
    char text[384];
} oxp_native_info_t;
typedef struct {
    uint32_t slot, y;
    uint16_t pixels[OXP_NATIVE_W];
} oxp_native_row_t;
_Static_assert(sizeof(oxp_native_row_t)==1288,"native scanline ABI");
#endif
