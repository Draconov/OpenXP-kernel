#include "gui/desktop.h"
#include "fs/initrd.h"
#include <stdint.h>
#include <stddef.h>
#ifndef OPENXP_WM_HOST_TEST
#include "serial.h"
#endif

static uint32_t rgb(desktop_t *d,uint8_t r,uint8_t g,uint8_t b){return fb_rgb(d->surface,r,g,b);}
static void fill(desktop_t *d,int x,int y,int w,int h,uint8_t r,uint8_t g,uint8_t b) {
    fb_rect(d->surface,x,y,w,h,rgb(d,r,g,b));
}
static void text(desktop_t *d,int x,int y,const char *str,int scale,uint8_t r,uint8_t g,uint8_t b) {
    fb_text(d->surface,x,y,str,scale,rgb(d,r,g,b));
}
static void truncate_text(desktop_t *d,int x,int y,const char *s,int width,int scale,uint32_t ink) {
    if(width>0) fb_text_limit(d->surface,x,y,s,width/(6*scale),scale,ink);
}
static bool inside(int x,int y,int xx,int yy,int w,int h) {
    return x>=xx&&y>=yy&&(int64_t)x<(int64_t)xx+w&&(int64_t)y<(int64_t)yy+h;
}
static void console_output(void *ctx,const char *str) {
    desktop_t *d=ctx;
    if(!str)return;
    if(str[0]=='\f') {d->line_total=0;return;}
    char *row=d->output[d->line_total%56];
    size_t n=0;
    while(str[n]&&n<sizeof(d->output[0])-1) {row[n]=str[n];++n;}
    row[n]=0;
    d->line_total++;
#ifndef OPENXP_WM_HOST_TEST
    serial_puts(str);serial_puts("\n");
#endif
}
static void focus(desktop_t *d,int which) {
    int found=-1;
    for(int i=0;i<DESKTOP_WINDOWS;++i)if(d->order[i]==which)found=i;
    if(found<0)return;
    for(int i=found;i<DESKTOP_WINDOWS-1;++i)d->order[i]=d->order[i+1];
    d->order[DESKTOP_WINDOWS-1]=which;
}
int desktop_top_window(const desktop_t *d) {
    for(int i=DESKTOP_WINDOWS-1;i>=0;--i) {
        int idx=d->order[i];
        if(d->windows[idx].open&&!d->windows[idx].minimized)return idx;
    }
    return -1;
}
static void open_window(desktop_t *d,int which) {
    if(which<0||which>=DESKTOP_WINDOWS)return;
    d->windows[which].open=true;d->windows[which].minimized=false;
    if(which==DESKTOP_TERMINAL)d->shell.visible=true;
    focus(d,which);
}
void desktop_init(desktop_t *d,fb_surface_t *s,shell_services_t services) {
    *d=(desktop_t){0};d->surface=s;
    d->windows[0]=(desktop_window_t){70,90,s->width>810?670:s->width-110,s->height>610?370:s->height-185,true,false};
    d->windows[1]=(desktop_window_t){110,100,s->width>810?540:s->width-130,s->height>610?330:s->height-185,false,false};
    d->windows[2]=(desktop_window_t){160,105,s->width>810?440:s->width-180,s->height>450?235:s->height-170,true,false};
    d->order[0]=1;d->order[1]=0;d->order[2]=2;
    d->mouse_x=s->width/2;d->mouse_y=s->height/2;
    d->selected_file=-1;
    shell_init(&d->shell,console_output,d,services);
}
static void draw_wallpaper(desktop_t *d) {
    fb_surface_t *s=d->surface;
    for(int y=0;y<s->height-42;++y) {
        /* simple XP-inspired vertical blue-teal sky, using original artwork */
        unsigned shade=(unsigned)(y*90/(s->height>43?s->height-42:1));
        fb_rect(s,0,y,s->width,1,rgb(d,(uint8_t)(12+shade/3),(uint8_t)(91+shade),(uint8_t)(174+shade/2)));
    }
    int horizon=s->height*3/4;
    for(int y=horizon;y<s->height-42;++y) {
        int slope=(y-horizon)*5/4;
        fill(d,0,y,s->width,1,(uint8_t)(22+slope/9),(uint8_t)(115+slope/10),(uint8_t)(50+slope/20));
    }
    fill(d,0,s->height-43,s->width,2,83,145,241);
}
static void draw_icons(desktop_t *d) {
    static const char *names[]={"MY COMPUTER","DOCUMENTS","TERMINAL"};
    for(int i=0;i<3;++i) {
        int y=35+i*90;
        fill(d,25,y,32,30,239,244,255);
        fill(d,28,y+3,26,21,i==1?235:70,i==1?199:143,i==1?66:231);
        fill(d,30,y+26,22,3,37,64,93);
        text(d,16,y+39,names[i],1,255,255,255);
    }
}
static void draw_terminal(desktop_t *d,desktop_window_t *w) {
    int xx=w->x+5,yy=w->y+35,width=w->w-10,height=w->h-42;
    fill(d,xx,yy,width,height,13,22,31);
    int rows=(height-40)/12;
    if(rows<1)return;
    int avail=rows-1;
    unsigned start=d->line_total>(unsigned)avail?d->line_total-(unsigned)avail:0;
    for(unsigned n=start;n<d->line_total;++n) {
        int y=yy+8+(int)(n-start)*12;
        if(y+7>=yy+height-18)break;
        truncate_text(d,xx+9,y,d->output[n%56],width-19,1,rgb(d,198,243,208));
    }
    fill(d,xx+4,yy+height-28,width-8,24,19,45,45);
    text(d,xx+11,yy+height-20,"C:\\>",1,164,238,173);
    truncate_text(d,xx+35,yy+height-20,d->shell.input,width-51,1,rgb(d,243,255,244));
}
static void draw_files(desktop_t *d,desktop_window_t *w) {
    fill(d,w->x+5,w->y+34,w->w-10,w->h-40,255,255,255);
    fill(d,w->x+7,w->y+38,w->w-14,27,228,237,249);
    text(d,w->x+17,w->y+47,"C:\\RAMDISK",1,35,65,103);
    text(d,w->x+17,w->y+80,"NAME",1,24,67,116);
    text(d,w->x+17,w->y+97,"---------------------------------",1,151,169,194);
    size_t count=initrd_count();
    for(size_t i=0;i<count && i<15;++i) {
        char path[64];
        if(!initrd_name_at(i,path,sizeof(path)))continue;
        if(d->selected_file==(int)i)
            fill(d,w->x+13,w->y+110+(int)i*23,w->w-28,22,198,222,251);
        fill(d,w->x+17,w->y+114+(int)i*23,12,14,244,202,78);
        truncate_text(d,w->x+39,w->y+117+(int)i*23,path,w->w-62,1,rgb(d,45,59,91));
    }
    if(!count)text(d,w->x+17,w->y+120,"NO FILES",1,104,107,115);
    fill(d,w->x+5,w->y+w->h-40,w->w-10,36,228,237,249);
    fill(d,w->x+w->w-109,w->y+w->h-34,99,25,48,113,191);
    text(d,w->x+w->w-94,w->y+w->h-26,"RUN FILE",1,255,255,255);
    text(d,w->x+17,w->y+w->h-26,"READ ONLY",1,98,112,132);
}
static void draw_about(desktop_t *d,desktop_window_t *w) {
    fill(d,w->x+5,w->y+34,w->w-10,w->h-40,248,251,255);
    fill(d,w->x+22,w->y+52,76,75,30,98,190);
    text(d,w->x+31,w->y+77,"XP",3,255,255,255);
    text(d,w->x+115,w->y+59,"OPENXP",3,17,68,141);
    text(d,w->x+115,w->y+92,"VERSION 1.8.0",1,84,101,124);
    text(d,w->x+25,w->y+152,"EXPERIMENTAL X86 64 OPERATING SYSTEM",1,39,62,92);
    text(d,w->x+25,w->y+172,"ORIGINAL KERNEL AND GRAPHICAL SHELL",1,39,62,92);
    text(d,w->x+25,w->y+197,"NOT FOR PRODUCTION USE",1,159,48,50);
}
static const char *const titles[]={"OpenXP Command Prompt","My Computer - RAMDISK","About OpenXP"};
static void draw_window(desktop_t *d,int index,bool active) {
    desktop_window_t *w=&d->windows[index];
    if(!w->open||w->minimized)return;
    fill(d,w->x+4,w->y+5,w->w,w->h,18,64,90);
    fill(d,w->x,w->y,w->w,w->h,222,231,243);
    fb_outline(d->surface,w->x,w->y,w->w,w->h,rgb(d,42,81,143));
    fill(d,w->x+3,w->y+3,w->w-6,28,active?17:97,active?88:118,active?195:149);
    truncate_text(d,w->x+13,w->y+11,titles[index],w->w-92,1,rgb(d,255,255,255));
    fill(d,w->x+w->w-28,w->y+6,21,21,192,63,61);
    text(d,w->x+w->w-22,w->y+13,"X",1,255,255,255);
    fill(d,w->x+w->w-54,w->y+6,21,21,79,133,203);
    text(d,w->x+w->w-48,w->y+13,"-",1,255,255,255);
    if(index==DESKTOP_TERMINAL)draw_terminal(d,w);
    else if(index==DESKTOP_FILES)draw_files(d,w);
    else draw_about(d,w);
}
static void draw_taskbar(desktop_t *d) {
    int h=d->surface->height,w=d->surface->width;
    fill(d,0,h-42,w,42,20,69,164);
    fill(d,0,h-42,w,3,98,156,243);
    fill(d,2,h-38,105,35,35,153,48);
    fill(d,5,h-37,100,32,48,176,68);
    text(d,19,h-26,"START",2,255,255,255);
    for(int i=0;i<DESKTOP_WINDOWS;++i) {
        if(!d->windows[i].open)continue;
        int bx=117+i*145;
        if(bx+140>w-100)break;
        fill(d,bx,h-36,136,31,desktop_top_window(d)==i?54:42,desktop_top_window(d)==i?127:98,204);
        fb_outline(d->surface,bx,h-36,136,31,rgb(d,135,174,237));
        truncate_text(d,bx+8,h-25,titles[i],125,1,rgb(d,255,255,255));
    }
    fill(d,w-90,h-37,85,32,36,108,188);
    char timer[22]; uint64_t secs=d->clock_ticks/100;
    char rev[22];int n=0;do{rev[n++]=(char)('0'+secs%10);secs/=10;}while(secs&&n<20);
    int k=0; timer[k++]='T';timer[k++]='+';
    while(n && k<21)timer[k++]=rev[--n];
    timer[k]=0;
    text(d,w-78,h-25,timer,1,255,255,255);
}
static void draw_menu(desktop_t *d) {
    if(!d->start_open)return;
    int h=d->surface->height,y=h-42-232;
    fill(d,2,y,250,232,238,245,255);
    fb_outline(d->surface,2,y,250,232,rgb(d,20,56,139));
    fill(d,3,y+3,248,37,26,95,185);
    text(d,19,y+15,"OPENXP",2,255,255,255);
    static const char *options[]={"TERMINAL","MY COMPUTER","ABOUT OPENXP"};
    for(int i=0;i<3;++i) {
        fill(d,12,y+56+i*49,230,40,i==0?232:249,i==0?240:250,i==0?255:253);
        text(d,27,y+72+i*49,options[i],1,28,57,104);
    }
}
static void draw_cursor(desktop_t *d) {
    static const uint16_t shape[]={1,3,7,15,31,63,127,255,511,63,103,195,129};
    int x=d->mouse_x,y=d->mouse_y;
    for(int r=0;r<13;++r)
        for(int c=0;c<9;++c)if(shape[r]&(1u<<(8-c)))
            fb_rect(d->surface,x+c,y+r,1,1,rgb(d,255,255,255));
    for(int r=1;r<12;++r)fb_rect(d->surface,x,y+r,1,1,rgb(d,12,30,51));
}
void desktop_draw(desktop_t *d) {
    draw_wallpaper(d);draw_icons(d);
    int top=desktop_top_window(d);
    for(int z=0;z<DESKTOP_WINDOWS;++z)
        draw_window(d,d->order[z],d->order[z]==top);
    draw_taskbar(d);draw_menu(d);draw_cursor(d);
}
static void desktop_click(desktop_t *d,int x,int y) {
    int h=d->surface->height;
    if(y>=h-42) {
        if(x<110){d->start_open=!d->start_open;return;}
        for(int i=0;i<DESKTOP_WINDOWS;++i) {
            int bx=117+i*145;
            if(x>=bx&&x<bx+136) {
                if(d->windows[i].open && desktop_top_window(d)==i)d->windows[i].minimized=true;
                else open_window(d,i);
                d->start_open=false;return;
            }
        }
        d->start_open=false;return;
    }
    if(d->start_open) {
        int top=h-42-232;
        if(inside(x,y,12,top+53,230,145)) {
            int idx=(y-top-53)/49;
            if(idx>=0&&idx<3)open_window(d,idx);
        }
        d->start_open=false;return;
    }
    for(int z=DESKTOP_WINDOWS-1;z>=0;--z) {
        int i=d->order[z];desktop_window_t *w=&d->windows[i];
        if(!w->open||w->minimized||!inside(x,y,w->x,w->y,w->w,w->h))continue;
        if(y<w->y+33 && x>=w->x+w->w-31){w->open=false;return;}
        if(y<w->y+33 && x>=w->x+w->w-57){w->minimized=true;return;}
        focus(d,i);
        if(i==DESKTOP_FILES && y>=w->y+112 && y<w->y+112+(int)initrd_count()*23) {
            int choice=(y-w->y-112)/23;
            if(choice>=0 && choice<(int)initrd_count()) {
                if(d->selected_file==choice && d->clock_ticks-d->last_file_click<40) {
                    char path[64];
                    if(initrd_name_at((size_t)choice,path,sizeof(path)) && d->shell.services.run)
                        console_output(d,d->shell.services.run(d->shell.services.ctx,path)?
                            "Application exited successfully.":"Application not executable (or faulted).");
                }
                d->selected_file=choice;d->last_file_click=d->clock_ticks;
            }
            return;
        }
        if(i==DESKTOP_FILES && d->selected_file>=0 &&
           inside(x,y,w->x+w->w-109,w->y+w->h-34,99,25)) {
            char path[64];
            if(initrd_name_at((size_t)d->selected_file,path,sizeof(path)) && d->shell.services.run)
                console_output(d,d->shell.services.run(d->shell.services.ctx,path)?
                    "Application exited successfully.":"Application not executable (or faulted).");
            return;
        }
        if(y<w->y+33){d->dragging=true;d->drag_window=i;d->last_x=x;d->last_y=y;}
        return;
    }
    if(inside(x,y,12,25,104,65))open_window(d,DESKTOP_FILES);
    else if(inside(x,y,12,115,104,65))open_window(d,DESKTOP_FILES);
    else if(inside(x,y,12,205,104,65))open_window(d,DESKTOP_TERMINAL);
}
void desktop_mouse(desktop_t *d,int x,int y,bool down) {
    if(!d||!d->surface)return;
    if(x<0)x=0;
    if(y<0)y=0;
    if(x>=d->surface->width)x=d->surface->width-1;
    if(y>=d->surface->height)y=d->surface->height-1;
    if(down&&!d->mouse_down)desktop_click(d,x,y);
    else if(down && d->dragging) {
        desktop_window_t *w=&d->windows[d->drag_window];
        int nx=w->x+x-d->last_x,ny=w->y+y-d->last_y;
        if(nx<0)nx=0;
        if(ny<0)ny=0;
        if(nx+w->w>d->surface->width)nx=d->surface->width-w->w;
        if(ny+w->h>d->surface->height-42)ny=d->surface->height-42-w->h;
        w->x=nx;w->y=ny;d->last_x=x;d->last_y=y;
    }
    if(!down)d->dragging=false;
    d->mouse_down=down;
    d->mouse_x=x;d->mouse_y=y;
}
void desktop_key(desktop_t *d,char c) {
    if(c=='\t') {
        for(int i=0;i<DESKTOP_WINDOWS;++i) {
            int idx=d->order[i];
            if(d->windows[idx].open&&!d->windows[idx].minimized){focus(d,idx);break;}
        }
        return;
    }
    if(desktop_top_window(d)==DESKTOP_TERMINAL) {
        (void)shell_key(&d->shell,c);
        if(!d->shell.visible)d->windows[DESKTOP_TERMINAL].minimized=true;
    }
}
void desktop_tick(desktop_t *d,uint64_t ticks){d->clock_ticks=ticks;}
