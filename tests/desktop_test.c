#include "gui/desktop.h"
#include <assert.h>
#include <stdio.h>
static uint32_t image[640*480];
static unsigned launches;
static bool launch(void *ctx,const char *path) {
    (void)ctx;
    if(path[0]=='b') ++launches;
    return path[0]=='b';
}
size_t initrd_count(void){return 3;}
bool initrd_name_at(size_t index,char *dst,size_t cap){
    const char *names[]={"bin/fault","bin/hello","docs/readme.txt"};
    if(index>=3||cap<20)return false;
    unsigned n=0;while(names[index][n]){dst[n]=names[index][n];++n;}
    dst[n]=0;return true;
}
int main(void){
    fb_surface_t f={(volatile uint8_t*)image,640,480,640*4,8,8,8,16,8,0};
    static desktop_t d;
    desktop_init(&d,&f,(shell_services_t){.run=launch});
    assert(desktop_top_window(&d)==DESKTOP_ABOUT);
    desktop_draw(&d);
    desktop_mouse(&d,20,462,true);assert(d.start_open);
    desktop_mouse(&d,20,462,false);
    desktop_mouse(&d,27,480-42-232+60,true);
    assert(!d.start_open && desktop_top_window(&d)==DESKTOP_TERMINAL);
    desktop_mouse(&d,27,480-42-232+60,false);
    int x=d.windows[0].x,y=d.windows[0].y;
    desktop_mouse(&d,x+20,y+10,true);
    desktop_mouse(&d,x+60,y+40,true);
    assert(d.windows[0].x==x+40 && d.windows[0].y==y+30);
    desktop_mouse(&d,x+60,y+40,false);
    desktop_key(&d,'v');desktop_key(&d,'e');desktop_key(&d,'r');desktop_key(&d,'\n');
    assert(d.shell.used==0 && d.line_total>=4);
    desktop_draw(&d);
    desktop_mouse(&d,d.windows[0].x+d.windows[0].w-15,d.windows[0].y+10,true);
    assert(!d.windows[0].open);
    desktop_mouse(&d,2,2,false);
    desktop_mouse(&d,20,462,true);assert(d.start_open);
    desktop_mouse(&d,20,462,false);
    desktop_mouse(&d,27,480-42-232+110,true);
    assert(desktop_top_window(&d)==DESKTOP_FILES);
    desktop_mouse(&d,27,480-42-232+110,false);
    desktop_window_t w=d.windows[DESKTOP_FILES];
    desktop_mouse(&d,w.x+42,w.y+115+23,true);
    assert(d.selected_file==1);
    desktop_mouse(&d,w.x+42,w.y+115+23,false);
    desktop_mouse(&d,w.x+w.w-80,w.y+w.h-21,true);
    assert(launches==1);
    puts("PASS: window focus, menu, drag, terminal, file selection/run, close, rendering");
}
