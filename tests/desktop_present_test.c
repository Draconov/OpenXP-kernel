/* A redrawn region must produce exactly the same pixels as a full repaint.
 * This catches the stale underlay/window trail and cursor regressions that
 * otherwise only show up as rapid flickering in an actual VirtualBox guest. */
#include "gui/desktop.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define W 640
#define H 480
static uint32_t storage[W*H+2];
static uint32_t expected[W*H];
size_t initrd_count(void){return 0;}
bool initrd_name_at(size_t i,char *dst,size_t cap){(void)i;(void)dst;(void)cap;return false;}
static void compare_full(desktop_t *desktop){
    memcpy(expected,storage+1,sizeof(expected));
    desktop_draw(desktop);
    assert(memcmp(expected,storage+1,sizeof(expected))==0 &&
           "clipped presentation must match a complete coherent frame");
    assert(storage[0]==0x11223344u && storage[W*H+1]==0x55667788u);
}
int main(void){
    fb_surface_t fb={(volatile uint8_t *)(storage+1),W,H,W*4,
        8,8,8,16,8,0,false,0,0,0,0};
    storage[0]=0x11223344u;storage[W*H+1]=0x55667788u;
    static desktop_t d;
    desktop_init(&d,&fb,(shell_services_t){0});
    d.mouse_x=240;d.mouse_y=215;
    desktop_draw(&d);
    assert(d.cursor_painted);
    int oldx=d.windows[DESKTOP_ABOUT].x;
    int oldy=d.windows[DESKTOP_ABOUT].y;
    d.windows[DESKTOP_ABOUT].x+=39;
    d.windows[DESKTOP_ABOUT].y+=19;
    desktop_redraw_moved_window(&d,oldx,oldy,DESKTOP_ABOUT);
    compare_full(&d);
    desktop_tick(&d,400);
    desktop_redraw_taskbar(&d);
    compare_full(&d);
    d.mouse_x=490;d.mouse_y=250;
    desktop_cursor_update(&d);
    compare_full(&d);
    puts("PASS: off-screen presentation, clipped window moves, clock and cursor parity");
    return 0;
}
