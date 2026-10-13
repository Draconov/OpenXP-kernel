#ifndef OPENXP_SHELL_BRIDGE_H
#define OPENXP_SHELL_BRIDGE_H
#include <stdbool.h>
#include <stdint.h>
#include "user/shell_abi.h"
#include "input/input.h"
#include "video/fb.h"
struct desktop;
/* Kernel only transports validated user-owned drawing commands and events.
 * Ring 3 chooses layout, focus and input policy. Drawing remains privileged. */
void shell_bridge_reset(void);
void shell_bridge_attach(void *desktop);
void shell_bridge_shutdown(void);
void shell_bridge_owner_exit(uint64_t pid);
bool shell_bridge_take_dirty(void);
void shell_bridge_watchdog(uint64_t now);
bool shell_bridge_active(void);
bool shell_bridge_menu_open(void);
bool shell_bridge_submit(uint64_t pid,const oxp_shell_draw_t *commands,uint32_t count,uint64_t tick);
void shell_bridge_compose(fb_surface_t *target);
void shell_bridge_notify_focus(uint32_t index);
bool shell_bridge_route(const input_event_t *event,bool menu_visible,int height);
bool shell_bridge_pop(oxp_shell_event_t *event);
bool shell_bridge_action(uint32_t action,uint32_t index);
bool shell_bridge_window(uint32_t index,oxp_shell_window_t *out);
#ifdef OPENXP_SHELL_HOST_TEST
unsigned shell_bridge_test_pending(void);
#endif
#endif
