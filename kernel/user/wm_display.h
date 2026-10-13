#ifndef OPENXP_WM_DISPLAY_H
#define OPENXP_WM_DISPLAY_H
#include <stdint.h>
/* Small, copied syscall payload; callers never supply another process PID.
 * The kernel resolves the peer through an authorized accepted IPC handle. */
#define OPENXP_WM_SET_RECT 1u
#define OPENXP_WM_RAISE 2u
#define OPENXP_WM_REQUEST_CLOSE 3u
typedef struct {
    uint32_t action, x, y, width, height;
} openxp_wm_display_cmd_t;
_Static_assert(sizeof(openxp_wm_display_cmd_t)==20, "window display syscall ABI");
#endif
