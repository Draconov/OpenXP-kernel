#ifndef OPENXP_GUI_DESKTOP_H
#define OPENXP_GUI_DESKTOP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "video/fb.h"
#include "shell/shell.h"

enum { DESKTOP_TERMINAL=0, DESKTOP_FILES=1, DESKTOP_ABOUT=2, DESKTOP_WINDOWS=3 };
typedef struct { int x,y,w,h; bool open,minimized; } desktop_window_t;
typedef struct {
    fb_surface_t *surface;
    desktop_window_t windows[DESKTOP_WINDOWS];
    int order[DESKTOP_WINDOWS]; /* low to high: focused window last */
    bool start_open,dragging,mouse_down;
    int drag_window,last_x,last_y,mouse_x,mouse_y;
    int selected_file;
    uint64_t last_file_click;
    char output[56][90];
    unsigned line_total;
    shell_t shell;
    uint64_t clock_ticks;
} desktop_t;
void desktop_init(desktop_t *d,fb_surface_t *s,shell_services_t services);
void desktop_draw(desktop_t *d);
void desktop_mouse(desktop_t *d,int x,int y,bool down);
void desktop_key(desktop_t *d,char c);
void desktop_tick(desktop_t *d,uint64_t ticks);
int desktop_top_window(const desktop_t *d);
#endif
