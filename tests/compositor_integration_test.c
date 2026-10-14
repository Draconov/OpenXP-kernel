/* Host-side pixel integration test for the real desktop renderer using a
 * Ring 3 supplied window stacking plan. Hardware interrupt paths are not
 * exercised; this verifies that the compositor uses the submitted order. */
#include "gui/desktop.h"
#include "gui/shell_bridge.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t framebuffer[640u*480u];
static uint32_t first[640u*480u];
void serial_puts(const char *message){(void)message;}
size_t initrd_count(void){return 0;}
bool initrd_name_at(size_t i,char *dst,size_t cap){(void)i;(void)dst;(void)cap;return false;}
int main(void){
    static desktop_t d;
    fb_surface_t s={(volatile uint8_t *)framebuffer,640,480,640*4,
        8,8,8,16,8,0,false,0,0,0,0};
    desktop_init(&d,&s,(shell_services_t){0});
    for(unsigned i=0;i<DESKTOP_WINDOWS;++i){d.windows[i].open=false;d.windows[i].minimized=false;}
    d.windows[DESKTOP_ABOUT]=(desktop_window_t){45,45,370,255,true,false,false,0,0,0,0};
    d.windows[DESKTOP_NOTEPAD]=(desktop_window_t){45,45,370,255,true,false,false,0,0,0,0};
    shell_bridge_reset();shell_bridge_attach(&d);
    oxp_shell_draw_t cmds[3]={{.op=OXP_SHELL_COMPOSE_BEGIN},
        {.op=OXP_SHELL_COMPOSE_WINDOW,.x=DESKTOP_ABOUT},
        {.op=OXP_SHELL_COMPOSE_WINDOW,.x=DESKTOP_NOTEPAD}};
    assert(shell_bridge_submit(77,cmds,3,100));
    desktop_draw(&d);
    memcpy(first,framebuffer,sizeof(first));
    cmds[1].x=DESKTOP_NOTEPAD;cmds[2].x=DESKTOP_ABOUT;
    assert(shell_bridge_submit(77,cmds,3,101));
    memset(framebuffer,0,sizeof(framebuffer));
    desktop_draw(&d);
    size_t different=0;
    for(size_t i=0;i<640u*480u;++i)different+=(first[i]!=framebuffer[i]);
    assert(different>100 && "Ring 3 ordering should change composed pixels");
    /* Header-button dispatch must agree with the Ring 3 z-order plan,
       even when it differs from the legacy kernel focus stack. */
    assert(!d.windows[DESKTOP_ABOUT].maximized);
    assert(!d.windows[DESKTOP_NOTEPAD].maximized);
    desktop_mouse(&d,45+370-45,56,true);
    assert(d.windows[DESKTOP_ABOUT].maximized);
    assert(!d.windows[DESKTOP_NOTEPAD].maximized);
    desktop_mouse(&d,45+370-45,56,false);
    shell_bridge_shutdown();
    assert(!shell_bridge_active());
    memset(framebuffer,0,sizeof(framebuffer));
    desktop_draw(&d); /* Legacy safety path still rasterizes the desktop. */
    assert(framebuffer[3*640+3]!=0);
    puts("PASS: windowd composition ordering affects actual pixels; fallback renders");
    return 0;
}
