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
/* Read-only scene epoch: reject visibly torn, cross-commit renderings. */
uint64_t shell_bridge_scene_version(void);
void shell_bridge_request_redraw(void);
void shell_bridge_watchdog(uint64_t now);
bool shell_bridge_active(void);
bool shell_bridge_has_window_plan(void);
bool shell_bridge_menu_open(void);
bool shell_bridge_submit(uint64_t pid,const oxp_shell_draw_t *commands,uint32_t count,uint64_t tick);
void shell_bridge_compose(fb_surface_t *target);
void shell_bridge_compose_background(fb_surface_t *target);
void shell_bridge_compose_native(fb_surface_t *target);
/* A service may publish a layer only after every row has been received.
 * The active layer remains unchanged while the next frame is in flight. */
bool shell_bridge_raster_row(uint64_t pid,const oxp_shell_raster_row_t *row);
bool shell_bridge_raster_commit(uint64_t pid,uint32_t mask);
bool shell_bridge_raster_ready(uint32_t layer);
/* Returns true only when windowd has supplied a validated composition plan.
 * The kernel retains the compositor for the physical scanout/fallback. */
bool shell_bridge_window_order(uint32_t *indices, uint32_t capacity, uint32_t *length);
void shell_bridge_notify_focus(uint32_t index);
bool shell_bridge_route(const input_event_t *event,bool menu_visible,int height);
bool shell_bridge_pop(oxp_shell_event_t *event);
bool shell_bridge_action(uint32_t action,uint32_t index,uint64_t payload);
bool shell_bridge_window(uint32_t index,oxp_shell_window_t *out);
#ifdef OPENXP_SHELL_HOST_TEST
unsigned shell_bridge_test_pending(void);
#endif
#endif
