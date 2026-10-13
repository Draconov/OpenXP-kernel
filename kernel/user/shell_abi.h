#ifndef OPENXP_SHELL_ABI_H
#define OPENXP_SHELL_ABI_H
#include <stdint.h>
/* Native Ring 3 desktop policy / bounded kernel framebuffer renderer ABI.
 * Physical framebuffer and desktop_t never cross into userspace. */
#define OXP_SHELL_DRAW_MAX 96u
#define OXP_SHELL_TEXT_MAX 24u
#define OXP_SHELL_RECT 1u
#define OXP_SHELL_TEXT 2u
#define OXP_SHELL_BG_RECT 3u
#define OXP_SHELL_BG_TEXT 4u
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
    uint32_t index,visible,active,minimized;
    uint32_t x,y,width,height,z;
    char title[32];
} oxp_shell_window_t;
_Static_assert(sizeof(oxp_shell_window_t)==68,"window query wire ABI");
#endif
