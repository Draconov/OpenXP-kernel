/* OpenXP host test suite: shell.
 * Select exactly one original test per compilation using -DOPENXP_TEST_NAME.
 * Individual test binaries and sanitizers remain unchanged. */
#if defined(OPENXP_TEST_DESKTOP)
#include "gui/desktop.h"
#include "fs/ramfs.h"
#include "gui/native.h"
#include "gui/native_viewport.h"
#include <assert.h>
#include <stdio.h>
static uint32_t image[640*480];
static unsigned launches;
static size_t read_file(void *ctx,const char *path,char *dst,size_t cap){
    (void)ctx;
    const char *expected="docs/readme.txt",*body="Welcome to OpenXP!\nThis text is read-only on disk.";
    size_t i=0;while(expected[i] && path[i]==expected[i])++i;
    if(expected[i] || path[i])return 0;
    size_t len=0;while(body[len])++len;
    if(len>=cap)return 0;
    for(i=0;i<len;++i)dst[i]=body[i];
    dst[len]=0;
    return len;
}
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
    fb_surface_t f={(volatile uint8_t*)image,640,480,640*4,8,8,8,16,8,0,false,0,0,0,0};
    static desktop_t d;
    desktop_init(&d,&f,(shell_services_t){.run=launch});
    assert(desktop_top_window(&d)==DESKTOP_ABOUT);
    /* A content click on an exposed portion of an unfocused window must
     * raise it before mouse-down delivery; not just caption drags. */
    static desktop_t background_click;
    desktop_init(&background_click,&f,(shell_services_t){.run=launch});
    assert(desktop_top_window(&background_click)==DESKTOP_ABOUT);
    desktop_mouse(&background_click,120,155,true);
    assert(desktop_top_window(&background_click)==DESKTOP_TERMINAL);
    desktop_mouse(&background_click,120,155,false);
    /* XP titlebar: maximize/restore, minimize and close are separate targets.
       The usable desktop excludes the persistent bottom taskbar. */
    desktop_window_t initial=d.windows[DESKTOP_ABOUT];
    assert(desktop_shell_maximize(&d,DESKTOP_ABOUT));
    assert(d.windows[DESKTOP_ABOUT].maximized);
    assert(d.windows[DESKTOP_ABOUT].x==0 && d.windows[DESKTOP_ABOUT].y==0);
    assert(d.windows[DESKTOP_ABOUT].w==640 && d.windows[DESKTOP_ABOUT].h==438);
    assert(!desktop_shell_maximize(&d,DESKTOP_WINDOWS));
    desktop_mouse(&d,640-70,12,true); /* Minimize: leftmost caption button */
    assert(d.windows[DESKTOP_ABOUT].minimized && d.windows[DESKTOP_ABOUT].maximized);
    desktop_mouse(&d,640-70,12,false);
    assert(desktop_shell_focus(&d,DESKTOP_ABOUT));
    desktop_mouse(&d,640-44,12,true); /* Restore: middle caption button */
    assert(!d.windows[DESKTOP_ABOUT].maximized);
    assert(d.windows[DESKTOP_ABOUT].x==initial.x &&
           d.windows[DESKTOP_ABOUT].y==initial.y &&
           d.windows[DESKTOP_ABOUT].w==initial.w &&
           d.windows[DESKTOP_ABOUT].h==initial.h);
    desktop_mouse(&d,640-44,12,false);
    /* The keyboard has the same semantics as titlebar controls. */
    input_event_t key_max={.kind=INPUT_KEY,.code=KEY_UP,.modifiers=KEYMOD_WIN};
    desktop_key_event(&d,&key_max);
    assert(d.windows[DESKTOP_ABOUT].maximized);
    key_max.code=KEY_DOWN;
    desktop_key_event(&d,&key_max);
    assert(!d.windows[DESKTOP_ABOUT].maximized);
    /* Closed maximized windows reopen at their original size. */
    assert(desktop_shell_maximize(&d,DESKTOP_ABOUT));
    assert(desktop_shell_close(&d));
    assert(!d.windows[DESKTOP_ABOUT].open);
    assert(desktop_shell_focus(&d,DESKTOP_TERMINAL));
    assert(desktop_shell_open(&d,DESKTOP_ABOUT));
    assert(!d.windows[DESKTOP_ABOUT].maximized);
    assert(d.windows[DESKTOP_ABOUT].w==initial.w);
    /* Large native application windows also use the common geometry path. */
    native_reset();
    assert(native_open(800,"Large native",12));
    desktop_native_show(&d,0,true);
    /* Aurora 13: global shortcuts stay in desktop policy, while edit
     * commands go only to the currently focused native window. */
    assert(desktop_shell_focus(&d,DESKTOP_NATIVE));
    while(native_next_event(800)){}
    input_event_t edit_key={.kind=INPUT_KEY,.code=KEY_CHARACTER,
                           .key='C',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&d,&edit_key);
    assert(native_next_event(800)==OXP_KEY_COPY);
    edit_key.key='v';desktop_key_event(&d,&edit_key);
    assert(native_next_event(800)==OXP_KEY_PASTE);
    edit_key.code=KEY_RIGHT;edit_key.key=0;edit_key.modifiers=KEYMOD_SHIFT;
    desktop_key_event(&d,&edit_key);
    assert(native_next_event(800)==OXP_KEY_SELECT_RIGHT);
    edit_key.code=KEY_DELETE;edit_key.modifiers=0;
    desktop_key_event(&d,&edit_key);
    assert(native_next_event(800)==OXP_KEY_DELETE);
    assert(desktop_shell_maximize(&d,DESKTOP_NATIVE));
    assert(d.windows[DESKTOP_NATIVE].w==640);
    assert(desktop_shell_maximize(&d,DESKTOP_NATIVE));
    assert(d.windows[DESKTOP_NATIVE].w==470);
    assert(desktop_shell_resize(&d,DESKTOP_NATIVE,400u,250u));
    assert(d.windows[DESKTOP_NATIVE].w==400 && d.windows[DESKTOP_NATIVE].h==250);
    assert(!desktop_shell_resize(&d,DESKTOP_NATIVE,100u,80u));
    assert(!desktop_shell_resize(&d,DESKTOP_NATIVE,UINT32_MAX,300u));
    assert(desktop_shell_maximize(&d,DESKTOP_NATIVE));
    assert(!desktop_shell_resize(&d,DESKTOP_NATIVE,350u,230u));
    assert(desktop_shell_maximize(&d,DESKTOP_NATIVE));
    assert(d.windows[DESKTOP_NATIVE].w==400 && d.windows[DESKTOP_NATIVE].h==250);
    /* The compositor fallback can resize from the grip without windowd. */
    int rx=d.windows[DESKTOP_NATIVE].x+d.windows[DESKTOP_NATIVE].w-9;
    int ry=d.windows[DESKTOP_NATIVE].y+d.windows[DESKTOP_NATIVE].h-9;
    desktop_mouse(&d,rx,ry,true);
    assert(d.resizing);
    desktop_mouse(&d,rx+12,ry+14,true);
    assert(d.windows[DESKTOP_NATIVE].w==412 && d.windows[DESKTOP_NATIVE].h==264);
    desktop_mouse(&d,rx+12,ry+14,false);
    assert(!d.resizing);
    native_owner_exit(800);
    desktop_native_show(&d,0,false);

    assert(desktop_shell_move(&d,DESKTOP_ABOUT,20,30));
    assert(d.windows[DESKTOP_ABOUT].x==20 && d.windows[DESKTOP_ABOUT].y==30);
    assert(!desktop_shell_move(&d,DESKTOP_ABOUT,UINT32_MAX,0));
    assert(!desktop_shell_move(&d,DESKTOP_ABOUT,0,UINT32_MAX));
    assert(!desktop_shell_move(&d,DESKTOP_ABOUT,0,400));
    unsigned previous_launches=launches;
    assert(desktop_shell_launch_native(&d,DESKTOP_NOTEPAD));
    assert(desktop_shell_launch_native(&d,DESKTOP_CALCULATOR));
    assert(desktop_shell_launch_native(&d,DESKTOP_MINES));
    assert(desktop_shell_launch_native(&d,DESKTOP_SNAKE));
    assert(desktop_shell_launch_native(&d,DESKTOP_CALENDAR));
    assert(desktop_shell_launch_native(&d,DESKTOP_MONITOR));
    assert(desktop_shell_launch_native(&d,DESKTOP_ABOUT));
    assert(desktop_shell_launch_native(&d,DESKTOP_PAINT));
    assert(desktop_shell_launch_native(&d,DESKTOP_WORDPAD));
    assert(desktop_shell_launch_native(&d,DESKTOP_TERMINAL));
    assert(desktop_shell_launch_native(&d,DESKTOP_FILES));
    assert(desktop_shell_launch_native(&d,DESKTOP_RUN));
    assert(desktop_shell_launch_native(&d,DESKTOP_SETTINGS));
    assert(launches==previous_launches+13u);
    launches=previous_launches;
    desktop_draw(&d);
    desktop_mouse(&d,20,462,true);assert(d.start_open);
    desktop_mouse(&d,20,462,false);
    desktop_mouse(&d,27,480-42-370+65,true);
    assert(!d.start_open && desktop_top_window(&d)==DESKTOP_TERMINAL);
    desktop_mouse(&d,27,480-42-370+65,false);
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
    desktop_mouse(&d,27,480-42-370+114,true);
    assert(desktop_top_window(&d)==DESKTOP_FILES);
    desktop_mouse(&d,27,480-42-370+114,false);
    desktop_window_t w=d.windows[DESKTOP_FILES];
    /* Root lists virtual folders; navigate BIN and launch its HELLO binary. */
    desktop_mouse(&d,w.x+42,w.y+115,true);
    desktop_mouse(&d,w.x+42,w.y+115,false);
    desktop_mouse(&d,w.x+w.w-80,w.y+w.h-21,true);
    desktop_mouse(&d,w.x+w.w-80,w.y+w.h-21,false);
    assert(d.file_folder[0]=='b' && d.file_folder[1]=='i');
    desktop_mouse(&d,w.x+42,w.y+115+23,true);
    assert(d.selected_file==1);
    desktop_mouse(&d,w.x+42,w.y+115+23,false);
    desktop_mouse(&d,w.x+w.w-80,w.y+w.h-21,true);
    assert(launches==1);
    /* The cursor must be movable without repainting or damaging wallpaper. */
    static desktop_t fast;
    desktop_init(&fast,&f,(shell_services_t){.run=launch,.read_text=read_file});
    native_reset();
    assert(native_open(701,"Window Client",13));
    desktop_native_show(&fast,0,true);
    assert(native_wm_manage(701,1,33,47,270,180));
    native_window_t service_window=native_snapshot_slot(0);
    desktop_native_sync(&fast,0,&service_window);
    assert(fast.windows[DESKTOP_NATIVE].x==33 && fast.windows[DESKTOP_NATIVE].y==47);
    assert(fast.windows[DESKTOP_NATIVE].w==270 && fast.windows[DESKTOP_NATIVE].h==180);
    assert(native_wm_manage(701,2,0,0,0,0));
    service_window=native_snapshot_slot(0);
    desktop_native_sync(&fast,0,&service_window);
    assert(desktop_top_window(&fast)==DESKTOP_NATIVE);
    /* Sync is idempotent: userspace cannot indefinitely steal focus. */
    desktop_native_sync(&fast,0,&service_window);
    native_owner_exit(701);
    desktop_native_show(&fast,0,false);
    desktop_native_sync(&fast,0,&(native_window_t){0});
    assert(fast.wm_seen_geometry[0]==0 && fast.wm_seen_focus[0]==0);
    fast.mouse_x=400;fast.mouse_y=400;
    desktop_draw(&fast);
    uint32_t untouched=fast.cursor_background[0];
    desktop_mouse(&fast,410,410,false);
    desktop_cursor_update(&fast);
    assert(image[400*640+400]==untouched);
    desktop_mouse(&fast,470,410,false);
    desktop_cursor_update(&fast);
    assert(image[400*640+400]==untouched); /* original restored */
    /* Windows key / Enter navigates the Start menu without a mouse. */
    input_event_t ev={.kind=INPUT_KEY,.code=KEY_WINDOWS,.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);assert(fast.start_open);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_DOWN};
    desktop_key_event(&fast,&ev);assert(fast.menu_selection==1);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_ENTER,.key='\n'};
    desktop_key_event(&fast,&ev);
    assert(!fast.start_open && desktop_top_window(&fast)==DESKTOP_FILES);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='r',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);
    assert(desktop_top_window(&fast)==DESKTOP_RUN);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='t',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);assert(desktop_top_window(&fast)==DESKTOP_TERMINAL);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_F4,.modifiers=KEYMOD_ALT};
    desktop_key_event(&fast,&ev);assert(!fast.windows[DESKTOP_TERMINAL].open);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='d',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);assert(fast.showing_desktop);
    desktop_key_event(&fast,&ev);assert(!fast.showing_desktop);
    /* New desktop applications are controlled with both hotkeys and typing. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='n',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);
    assert(desktop_top_window(&fast)==DESKTOP_NOTEPAD);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='H'};
    desktop_key_event(&fast,&ev);
    ev.key='i';desktop_key_event(&fast,&ev);
    assert(fast.note_length==2 && fast.note_cursor==2);
    assert(fast.note[0]=='H' && fast.note[1]=='i');
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_LEFT};
    desktop_key_event(&fast,&ev);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='!'};
    desktop_key_event(&fast,&ev);
    assert(fast.note_length==3 && fast.note[1]=='!');
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_BACKSPACE};
    desktop_key_event(&fast,&ev);
    assert(fast.note_length==2 && fast.note[1]=='i');
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='n',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&fast,&ev);
    assert(fast.note_length==0);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='c',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);
    assert(desktop_top_window(&fast)==DESKTOP_CALCULATOR);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='8'};
    desktop_key_event(&fast,&ev);
    ev.key='*';desktop_key_event(&fast,&ev);
    ev.key='7';desktop_key_event(&fast,&ev);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_ENTER,.key='\n'};
    desktop_key_event(&fast,&ev);
    assert(!fast.calc_error && fast.calc_entry==56);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='c'};
    desktop_key_event(&fast,&ev);
    assert(fast.calc_entry==0);
    /* Dirty-window repaint must preserve pixels outside the moving region. */
    desktop_draw(&fast);
    uint32_t corner=image[0];
    int oldx=fast.windows[DESKTOP_CALCULATOR].x;
    int oldy=fast.windows[DESKTOP_CALCULATOR].y;
    fast.windows[DESKTOP_CALCULATOR].x+=11;
    desktop_redraw_moved_window(&fast,oldx,oldy,DESKTOP_CALCULATOR);
    assert(image[0]==corner);
    assert(!f.clip_active);
    /* Additional applications: hotkeys, Paint brush and palettes. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='p',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);assert(desktop_top_window(&fast)==DESKTOP_PAINT);
    w=fast.windows[DESKTOP_PAINT];
    desktop_mouse(&fast,w.x+16+38*2+8,w.y+60,true);
    assert(fast.paint_color==2);
    desktop_mouse(&fast,w.x+16+38*2+8,w.y+60,false);
    desktop_mouse(&fast,w.x+16+4,w.y+96+4,true);
    assert(fast.paint[2*160+2]==2);
    desktop_mouse(&fast,w.x+16+44,w.y+96+4,true);
    for(unsigned cx=2;cx<=22;++cx) assert(fast.paint[2*160+cx]==2);
    assert(fast.paint_dirty);
    uint32_t untouched_paint=image[0];
    desktop_redraw_paint(&fast);
    assert(!fast.paint_dirty && image[0]==untouched_paint);
    desktop_mouse(&fast,w.x+16+44,w.y+96+4,false);
    desktop_mouse(&fast,w.x+24,w.y+w.h-25,true);
    assert(fast.paint[2*160+2]==0 && fast.paint[2*160+22]==0);
    desktop_mouse(&fast,w.x+24,w.y+w.h-25,false);
    /* Taskbar must keep all open windows within its usable area. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='m',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);assert(desktop_top_window(&fast)==DESKTOP_MONITOR);
    desktop_draw(&fast);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='s',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);assert(desktop_top_window(&fast)==DESKTOP_SETTINGS);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='3'};
    desktop_key_event(&fast,&ev);assert(fast.appearance==2);
    desktop_draw(&fast);
    assert(image[0]!=corner);
    ev.key='1';desktop_key_event(&fast,&ev);assert(fast.appearance==0);
    /* Mines is guaranteed to keep the first move safe. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='g',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);assert(desktop_top_window(&fast)==DESKTOP_MINES);
    w=fast.windows[DESKTOP_MINES];
    desktop_mouse(&fast,w.x+30+25*4+9,w.y+92+25*4+9,true);
    assert(fast.mine_started && !fast.mine_lost && fast.mine_revealed>=1);
    assert(fast.mine_values[4*9+4]>=0);
    unsigned mines=0;
    for(int z=0;z<81;++z)if(fast.mine_values[z]==-1)++mines;
    assert(mines==10);
    desktop_mouse(&fast,w.x+30+25*4+9,w.y+92+25*4+9,false);
    /* Restart then place/remove a flag using F over the hovered cell. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='n'};
    desktop_key_event(&fast,&ev);assert(!fast.mine_started && !fast.mine_flags);
    desktop_mouse(&fast,w.x+32,w.y+94,false);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='f'};
    desktop_key_event(&fast,&ev);assert(fast.mine_flags==1 && fast.mine_cells[0]==2);
    desktop_key_event(&fast,&ev);assert(fast.mine_flags==0 && fast.mine_cells[0]==0);
    desktop_right_mouse(&fast,w.x+32,w.y+94,true);
    assert(fast.mine_flags==1 && fast.mine_cells[0]==2);
    desktop_right_mouse(&fast,w.x+32,w.y+94,false);
    desktop_right_mouse(&fast,w.x+32,w.y+94,true);
    assert(fast.mine_flags==0 && fast.mine_cells[0]==0);
    desktop_right_mouse(&fast,w.x+32,w.y+94,false);
    /* Uncover every non-mine square: must report a win without a fault. */
    desktop_mouse(&fast,w.x+30+25*4+4,w.y+92+25*4+4,true);
    desktop_mouse(&fast,w.x+30+25*4+4,w.y+92+25*4+4,false);
    for(int cell=0;cell<81;++cell)if(fast.mine_values[cell]>=0){
        int mx=w.x+30+(cell%9)*25+5,my=w.y+92+(cell/9)*25+5;
        desktop_mouse(&fast,mx,my,true);
        desktop_mouse(&fast,mx,my,false);
    }
    assert(fast.mine_won && !fast.mine_lost && fast.mine_revealed==71);
    desktop_draw(&fast);
    /* Second column of the Start menu works with the pointer. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_WINDOWS};
    desktop_key_event(&fast,&ev);assert(fast.start_open);
    desktop_mouse(&fast,27,480-42-370+51+5*43+12,true);
    assert(!fast.start_open && desktop_top_window(&fast)==DESKTOP_PAINT);
    /* Run is a real launcher: it resolves built-ins, reports errors, and can
       dispatch bundled initrd applications through the service callback. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='r',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);
    assert(desktop_top_window(&fast)==DESKTOP_RUN);
    const char *unknown="nosuchprogram";
    for(const char *p=unknown;*p;++p){
        ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key=*p};
        desktop_key_event(&fast,&ev);
    }
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_ENTER,.key='\n'};
    desktop_key_event(&fast,&ev);
    assert(fast.windows[DESKTOP_RUN].open && fast.run_status[0]);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_DELETE};
    desktop_key_event(&fast,&ev);assert(!fast.run_length);
    const char *name="nOtEpAd";
    for(const char *p=name;*p;++p){
        ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key=*p};
        desktop_key_event(&fast,&ev);
    }
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_ENTER,.key='\n'};
    desktop_key_event(&fast,&ev);
    assert(!fast.windows[DESKTOP_RUN].open && desktop_top_window(&fast)==DESKTOP_NOTEPAD);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='r',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_DELETE};
    desktop_key_event(&fast,&ev);
    const char *bin="BIN/HELLO";
    for(const char *p=bin;*p;++p){
        ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key=*p};
        desktop_key_event(&fast,&ev);
    }
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_ENTER,.key='\n'};
    unsigned before_launch=launches;
    desktop_key_event(&fast,&ev);
    assert(launches==before_launch+1 && !fast.windows[DESKTOP_RUN].open);
    /* Monotonic stopwatch: start, pause, reset, countdown alarm, minutes. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='k',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);assert(desktop_top_window(&fast)==DESKTOP_CLOCK);
    desktop_tick(&fast,1000);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key=' '};
    desktop_key_event(&fast,&ev);assert(fast.watch_running);
    desktop_tick(&fast,1550);
    desktop_key_event(&fast,&ev);assert(!fast.watch_running && fast.watch_elapsed==550);
    ev.key='r';desktop_key_event(&fast,&ev);
    assert(!fast.watch_running && fast.watch_elapsed==0);
    ev.key='m';desktop_key_event(&fast,&ev);assert(fast.watch_countdown);
    ev.key='+';desktop_key_event(&fast,&ev);assert(fast.watch_limit==12000);
    ev.key=' ';desktop_key_event(&fast,&ev);
    desktop_tick(&fast,13550);
    assert(!fast.watch_running && fast.watch_alarm && fast.watch_elapsed==12000);
    ev.key='r';desktop_key_event(&fast,&ev);assert(!fast.watch_alarm);
    /* Snake: movement, growing, anti-reverse, pause and wall collision. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='y',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);assert(desktop_top_window(&fast)==DESKTOP_SNAKE);
    assert(fast.snake_length==3 && !fast.snake_started);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_RIGHT};
    desktop_key_event(&fast,&ev);assert(fast.snake_started);
    uint16_t head=fast.snake_segments[0];
    fast.snake_food=(uint16_t)(head+1);
    desktop_tick(&fast,13568);
    assert(fast.snake_segments[0]==head+1 && fast.snake_length==4 && fast.snake_score==10);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_LEFT};
    desktop_key_event(&fast,&ev);assert(fast.snake_next_direction==1);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key=' '};
    desktop_key_event(&fast,&ev);assert(fast.snake_paused);
    uint16_t paused_head=fast.snake_segments[0];
    desktop_tick(&fast,13610);assert(fast.snake_segments[0]==paused_head);
    desktop_key_event(&fast,&ev);assert(!fast.snake_paused);
    fast.snake_segments[0]=19;fast.snake_length=1;
    fast.snake_direction=1;fast.snake_next_direction=1;
    desktop_tick(&fast,13628);assert(fast.snake_lost);
    ev.key='n';desktop_key_event(&fast,&ev);
    assert(!fast.snake_lost && !fast.snake_started && fast.snake_length==3);
    /* WordPad: RAM document edits, bold mode and zoom, without altering Notepad. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='w',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);assert(desktop_top_window(&fast)==DESKTOP_WORDPAD);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='H'};
    desktop_key_event(&fast,&ev);ev.key='i';desktop_key_event(&fast,&ev);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_ENTER,.key='\n'};
    desktop_key_event(&fast,&ev);assert(fast.word_length==3 && fast.word[2]=='\n');
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='b',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&fast,&ev);assert(fast.word_bold);
    w=fast.windows[DESKTOP_WORDPAD];
    desktop_mouse(&fast,w.x+182,w.y+51,false);
    desktop_mouse(&fast,w.x+182,w.y+51,true);desktop_mouse(&fast,w.x+182,w.y+51,false);
    assert(fast.word_scale==2);
    desktop_draw(&fast);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='n',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&fast,&ev);assert(!fast.word_length && !fast.word_cursor);
    /* Calendar: Gregorian leap centuries, RTC date selection and navigation. */
    assert(desktop_calendar_days(2000,2)==29);
    assert(desktop_calendar_days(1900,2)==28);
    assert(desktop_calendar_days(2024,2)==29);
    assert(desktop_calendar_days(2025,2)==28);
    assert(desktop_calendar_weekday(2026,10,13)==2); /* Tuesday */
    assert(desktop_calendar_weekday(2026,2,30)==7); /* Invalid date */
    desktop_calendar_set_today(&fast,2024,2,29);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='a',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);assert(desktop_top_window(&fast)==DESKTOP_CALENDAR);
    assert(fast.calendar_year==2024 && fast.calendar_month==2 && fast.calendar_day==29);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_RIGHT};
    desktop_key_event(&fast,&ev);assert(fast.calendar_month==3);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='t'};
    desktop_key_event(&fast,&ev);assert(fast.calendar_month==2 && fast.calendar_day==29);
    desktop_draw(&fast);
    /* Explorer: virtual directories, preview text safely, edit a RAM copy in WordPad. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='e',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);assert(desktop_top_window(&fast)==DESKTOP_FILES);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_DOWN};
    desktop_key_event(&fast,&ev);assert(fast.selected_file==0); /* bin */
    desktop_key_event(&fast,&ev);assert(fast.selected_file==1); /* docs */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_ENTER,.key='\n'};
    desktop_key_event(&fast,&ev);assert(fast.file_folder[0]=='d');
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_DOWN};
    desktop_key_event(&fast,&ev);
    assert(fast.selected_file==0 && fast.file_preview_text);
    assert(fast.file_preview_len>0);
    desktop_draw(&fast);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_ENTER,.key='\n'};
    desktop_key_event(&fast,&ev);
    assert(desktop_top_window(&fast)==DESKTOP_WORDPAD);
    assert(fast.word_length>0 && fast.word[0]=='W');
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='e',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_BACKSPACE};
    desktop_key_event(&fast,&ev);assert(!fast.file_folder[0]);
    desktop_draw(&fast);
    /* Temporary saving: each application has a fixed HOME filename. */
    ramfs_reset();
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='n',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='Q'};
    desktop_key_event(&fast,&ev);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='s',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&fast,&ev);assert(fast.note_saved && ramfs_count()==1);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='n',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&fast,&ev);assert(!fast.note_length);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='o',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&fast,&ev);assert(fast.note_length==1 && fast.note[0]=='Q');
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='w',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='n',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&fast,&ev);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='X'};
    desktop_key_event(&fast,&ev);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='s',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&fast,&ev);assert(fast.word_saved && ramfs_count()==2);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='n',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&fast,&ev);assert(fast.word_length==0);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='o',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&fast,&ev);assert(fast.word_length==1 && fast.word[0]=='X');
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='p',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);
    fast.paint[1234]=5;
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='s',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&fast,&ev);assert(fast.paint_saved && ramfs_count()==3);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='x'};
    desktop_key_event(&fast,&ev);assert(fast.paint[1234]==0);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='o',.modifiers=KEYMOD_CTRL};
    desktop_key_event(&fast,&ev);assert(fast.paint[1234]==5);
    /* Both initial archive folders and newly saved files appear in My Computer. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='e',.modifiers=KEYMOD_WIN};
    desktop_key_event(&fast,&ev);
    assert(desktop_top_window(&fast)==DESKTOP_FILES);
    /* Existing root has BIN and DOCS; RAMFS adds HOME. */
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_DOWN};
    desktop_key_event(&fast,&ev);desktop_key_event(&fast,&ev);desktop_key_event(&fast,&ev);
    assert(fast.selected_file==2);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_ENTER,.key='\n'};
    desktop_key_event(&fast,&ev);
    assert(fast.file_folder[0]=='h' && fast.file_folder[1]=='o');
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_DOWN};
    desktop_key_event(&fast,&ev);assert(fast.selected_file==0);
    /* RAMFS reset simulates a reboot: no persistent storage is claimed. */
    ramfs_reset();assert(ramfs_count()==0);
    /* Three native windows must be independently visible and input routed to
       the *focused* owner, including client-area mouse clicks. */
    native_reset();
    assert(native_open(90,"Calculator",10));
    assert(native_open(91,"Clock",5));
    assert(native_open(92,"Notepad",7));
    for(unsigned slot=0;slot<NATIVE_MAX_WINDOWS;++slot)
        desktop_native_show(&fast,slot,true);
    assert(desktop_top_window(&fast)==DESKTOP_NATIVE3);
    for(unsigned slot=0;slot<NATIVE_MAX_WINDOWS;++slot){
        unsigned expected=(unsigned)(90+slot);
        assert(native_next_event(expected)==NATIVE_EVENT_RESIZE);
        assert(native_next_event(expected)==0);
    }
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_CHARACTER,.key='A'};
    desktop_key_event(&fast,&ev);
    assert(native_next_event(92)=='A');
    assert(native_next_event(90)==0 && native_next_event(91)==0);
    int mx=fast.windows[DESKTOP_NATIVE2].x+25;
    int my=fast.windows[DESKTOP_NATIVE2].y+60;
    /* Higher overlapping windows must be minimized to reach a lower owner. */
    fast.windows[DESKTOP_NATIVE3].minimized=true;
    desktop_mouse(&fast,mx,my,true);desktop_mouse(&fast,mx,my,false);
    uint32_t click=native_next_event(91);
    assert((click&NATIVE_EVENT_CLICK)==NATIVE_EVENT_CLICK);
    uint32_t cx=25u,cy=60u;
    oxp_native_pointer_to_canvas(cx,cy,(unsigned)fast.windows[DESKTOP_NATIVE2].w,
                                 (unsigned)fast.windows[DESKTOP_NATIVE2].h,&cx,&cy);
    assert(((click>>10)&1023u)==cx && (click&1023u)==cy);
    assert(native_next_event(90)==0 && native_next_event(92)==0);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_UP};
    desktop_key_event(&fast,&ev);
    assert(native_next_event(91)==NATIVE_EVENT_UP);
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_LEFT};
    desktop_key_event(&fast,&ev);
    assert(native_next_event(91)==NATIVE_EVENT_LEFT);
    desktop_right_mouse(&fast,mx,my,true);
    uint32_t right=native_next_event(91);
    assert((right&NATIVE_EVENT_RIGHT_CLICK)==NATIVE_EVENT_RIGHT_CLICK);
    assert(((right>>10)&1023u)==cx && (right&1023u)==cy);
    desktop_right_mouse(&fast,mx,my,false);
    assert(native_next_event(90)==0 && native_next_event(92)==0);
    /* Aurora 11: an opted-in app gets a continuous pointer stream instead
       of an immediate legacy CLICK. Capture survives a move off the client. */
    assert(native_pointer_enable(91));
    desktop_mouse(&fast,mx,my,false);
    uint32_t motion=native_next_event(91);
    assert((motion&NATIVE_EVENT_POINTER_MASK)==NATIVE_EVENT_POINTER_MOVE);
    desktop_mouse(&fast,mx,my,true);
    uint32_t press=native_next_event(91);
    assert((press&NATIVE_EVENT_POINTER_MASK)==NATIVE_EVENT_POINTER_DOWN);
    motion=native_next_event(91);
    assert((motion&NATIVE_EVENT_POINTER_MASK)==NATIVE_EVENT_POINTER_MOVE);
    desktop_mouse(&fast,2,2,true);
    assert(native_next_event(91)==NATIVE_EVENT_POINTER_LEAVE);
    motion=native_next_event(91);
    assert((motion&NATIVE_EVENT_POINTER_MASK)==NATIVE_EVENT_POINTER_MOVE);
    assert((motion&1023u)==1023u);
    desktop_mouse(&fast,2,2,false);
    uint32_t release=native_next_event(91);
    assert((release&NATIVE_EVENT_POINTER_MASK)==NATIVE_EVENT_POINTER_UP);
    assert((release&1023u)==1023u);
    assert(!native_next_event(91));
    /* Keyboard focus traversal stays separate from native text input. */
    assert(desktop_shell_focus(&fast,DESKTOP_NATIVE2));
    ev=(input_event_t){.kind=INPUT_KEY,.code=KEY_TAB};
    desktop_key_event(&fast,&ev);
    assert(native_next_event(91)==9u);
    ev.modifiers=KEYMOD_SHIFT;
    desktop_key_event(&fast,&ev);
    assert(native_next_event(91)==NATIVE_EVENT_SHIFT_TAB);
    native_owner_exit(91);desktop_native_show(&fast,1,false);
    assert(!fast.windows[DESKTOP_NATIVE2].open);
    assert(fast.windows[DESKTOP_NATIVE].open);
    assert(fast.windows[DESKTOP_NATIVE3].open);
    puts("PASS: desktop, file editing, calendar, and multi-process native window input routing");
}
#elif defined(OPENXP_TEST_DESKTOP_PRESENT)
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
    /* Events consumed by windowd update the cursor position without
     * invoking desktop_mouse(), whose click routing must not run twice. */
    desktop_cursor_position(&d,123,55);
    assert(d.mouse_x==123 && d.mouse_y==55);
    desktop_cursor_update(&d);
    assert(d.cursor_saved_x==123 && d.cursor_saved_y==55);
    assert(storage[1+55*W+123]==fb_rgb(&fb,12,30,51));
    assert(storage[1+56*W+124]==fb_rgb(&fb,12,30,51));
    assert(storage[1+57*W+124]==fb_rgb(&fb,255,255,255));
    desktop_cursor_position(&d,10000,-123);
    assert(d.mouse_x==W-1 && d.mouse_y==0);
    desktop_cursor_update(&d);
    compare_full(&d);
    puts("PASS: off-screen presentation, clipped window moves, clock and cursor parity");
    return 0;
}
#elif defined(OPENXP_TEST_SHELL)
#include "shell/shell.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char lines[128][128]; static int count;
static void output(void *p,const char *line) { (void)p; assert(count < 128); snprintf(lines[count++],128,"%s",line); }
static uint64_t ticks(void *p) { (void)p; return 123; }
static uint64_t freepages(void *p) { (void)p; return 48; }
static size_t filecount(void *p) { (void)p; return 1; }
static bool filename(void *p,size_t idx,char *dst,size_t cap) { (void)p; assert(idx==0); assert(cap>10); strcpy(dst,"bin/hello"); return true; }
static size_t readtext(void *p,const char *path,char *dst,size_t cap) {
    (void)p;(void)path;assert(cap>6);strcpy(dst,"hello\n");return 6;
}
static bool psentry(void *p,size_t slot,uint64_t *pid,unsigned *state){
    (void)p;
    if(slot!=0)return false;
    *pid=27;*state=2;return true;
}
static bool file_copy(void *ctx,const char *src,const char *dst){
    (void)ctx;return strcmp(src,"home/a.txt")==0 && strcmp(dst,"home/b.txt")==0;
}
static bool file_rename(void *ctx,const char *src,const char *dst){
    (void)ctx;return strcmp(src,"home/b.txt")==0 && strcmp(dst,"home/c.txt")==0;
}
static bool file_remove(void *ctx,const char *path){
    (void)ctx;return strcmp(path,"home/c.txt")==0;
}
static bool run(void *p,const char *path) { (void)p; return strcmp(path,"bin/hello")==0; }
int main(void) {
    shell_t shell; shell_services_t svc={.ticks=ticks,.free_pages=freepages,.file_count=filecount,.file_name=filename,.run=run,.read_text=readtext,.process_entry=psentry,.file_copy=file_copy,.file_rename=file_rename,.file_remove=file_remove};
    shell_init(&shell,output,NULL,svc); assert(count==2);
    shell_execute(&shell,"dir"); assert(!strcmp(lines[3],"bin/hello"));
    shell_execute(&shell,"ps"); assert(strstr(lines[count-1],"PID 27  RUNNING"));
    shell_execute(&shell,"mem"); assert(!strcmp(lines[count-1],"48"));
    shell_execute(&shell,"uptime"); assert(!strcmp(lines[count-1],"123"));
    shell_execute(&shell,"run bin/hello"); assert(strstr(lines[count-1],"Launch requested"));
    shell_execute(&shell,"run missing"); assert(strstr(lines[count-1],"Could not"));
    assert(shell_key(&shell,'A') && shell_key(&shell,'B'));
    assert(shell.used==2 && !strcmp(shell.input,"AB"));
    assert(shell_key(&shell,'\b') && shell.used==1);
    assert(shell_key(&shell,'\n') && shell.used==0);
    shell_execute(&shell,"type docs/readme.txt");
    assert(!strcmp(lines[count-1],"hello"));
    shell_execute(&shell,"cd bin");assert(!strcmp(shell.cwd,"bin/"));
    shell_execute(&shell,"cd ..");assert(!shell.cwd[0]);
    shell_execute(&shell,"exit"); assert(!shell.visible);
    shell_execute(&shell,"pwd");assert(!strcmp(lines[count-1],"C:\\"));
    shell_execute(&shell,"ls");assert(!strcmp(lines[count-1],"bin/hello"));
    shell_execute(&shell,"calc 8 * 7");assert(!strcmp(lines[count-1],"56"));
    shell_execute(&shell,"calc 1 - 3");assert(!strcmp(lines[count-1],"-2"));
    shell_execute(&shell,"calc 2 / 0");assert(strstr(lines[count-1],"zero"));
    shell_execute(&shell,"calc 2000000000 * 2");assert(strstr(lines[count-1],"Usage"));
    shell_execute(&shell,"copy home/a.txt home/b.txt");
    assert(strstr(lines[count-1],"copied"));
    shell_execute(&shell,"copy home/a.txt");
    assert(strstr(lines[count-1],"Usage"));
    shell_execute(&shell,"ren home/b.txt home/c.txt");
    assert(strstr(lines[count-1],"renamed"));
    shell_execute(&shell,"del home/c.txt");
    assert(strstr(lines[count-1],"deleted"));
    shell_execute(&shell,"del home/missing");
    assert(strstr(lines[count-1],"Cannot"));
    shell_execute(&shell,"sysinfo");assert(strstr(lines[count-1],"PS/2"));
    shell_execute(&shell,"apps");assert(strstr(lines[count-1],"Calculator"));
    shell_execute(&shell,"tree");assert(strstr(lines[count-1],"DIR"));
    shell.visible=true;
    assert(shell_key(&shell,'h')&&shell_key(&shell,'e')&&shell_key(&shell,'l'));
    assert(shell.cursor==3 && shell.used==3);
    assert(shell_home(&shell) && shell.cursor==0);
    assert(shell_delete(&shell) && !strcmp(shell.input,"el"));
    assert(shell_key(&shell,'H') && !strcmp(shell.input,"Hel"));
    assert(shell_end(&shell) && shell.cursor==3);
    assert(shell_left(&shell) && shell.cursor==2);
    assert(shell_right(&shell) && shell.cursor==3);
    assert(shell_key(&shell,'p') && !strcmp(shell.input,"Help"));
    assert(shell_key(&shell,'\n'));
    assert(shell_history(&shell,-1) && !strcmp(shell.input,"Help"));
    assert(shell_history(&shell,1) && !shell.input[0]);
    puts("PASS: shell commands, arithmetic, history and line editing");
}
#elif defined(OPENXP_TEST_SHELL_BRIDGE)
#include "gui/shell_bridge.h"
#include "gui/desktop.h"
#include "input/input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t pixels[32*32+2];
int main(void){
    shell_bridge_reset();
    uint64_t scene_epoch=shell_bridge_scene_version();
    static desktop_t mock={0};
    for(unsigned i=0;i<DESKTOP_WINDOWS;++i)mock.order[i]=(int)i;
    mock.windows[DESKTOP_NOTEPAD]=(desktop_window_t){100,100,400,220,true,false,false,0,0,0,0};
    shell_bridge_attach(&mock);
    assert(!shell_bridge_active() && !shell_bridge_has_window_plan());
    oxp_shell_draw_t cmd={.op=OXP_SHELL_RECT,.x=0,.y=0,.w=3,.h=3,.color=0x345678};
    assert(shell_bridge_submit(7,&cmd,1,5));
    assert(shell_bridge_scene_version()>scene_epoch);
    scene_epoch=shell_bridge_scene_version();
    assert(shell_bridge_active() && shell_bridge_take_dirty());
    assert(!shell_bridge_action(OXP_SHELL_THEME,3,0));
    assert(mock.appearance==0);
    assert(shell_bridge_action(OXP_SHELL_THEME,2,0));
    assert(mock.appearance==2 && shell_bridge_take_dirty());
    assert(shell_bridge_action(OXP_SHELL_THEME,0,0));
    assert(mock.appearance==0 && shell_bridge_take_dirty());
    assert(!shell_bridge_take_dirty());
    assert(shell_bridge_submit(7,&cmd,1,6));
    assert(shell_bridge_scene_version()==scene_epoch);
    assert(!shell_bridge_take_dirty()); /* identical heartbeat is not a repaint */
    input_event_t key={.kind=INPUT_KEY,.code=KEY_WINDOWS};
    assert(shell_bridge_route(&key,false,768));
    oxp_shell_event_t event={0};
    assert(shell_bridge_pop(&event));
    assert(event.type==OXP_SHELL_EVENT_KEY && event.code==KEY_WINDOWS);
    assert(!shell_bridge_pop(&event));
    /* Title-bar action buttons must reach trusted desktop hit testing.
       The native window service must never capture these as drags. */
    input_event_t chrome={.kind=INPUT_BUTTON,.left=true,.x=455,.y=114};
    assert(!shell_bridge_route(&chrome,false,768)); /* middle: maximize */
    assert(!shell_bridge_pop(&event));
    chrome.x=429;
    assert(!shell_bridge_route(&chrome,false,768)); /* left: minimize */
    chrome.x=481;
    assert(!shell_bridge_route(&chrome,false,768)); /* right: close */
    /* Resize-corner capture goes to Ring 3, not an app's canvas. */
    input_event_t corner={.kind=INPUT_BUTTON,.left=true,.x=494,.y=315};
    assert(shell_bridge_route(&corner,false,768));
    assert(shell_bridge_pop(&event) && event.type==OXP_SHELL_EVENT_POINTER &&
           event.flags==OXP_SHELL_POINTER_DOWN);
    corner.kind=INPUT_MOVE;corner.x=510;corner.y=325;
    assert(shell_bridge_route(&corner,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_MOVE);
    corner.kind=INPUT_BUTTON;corner.left=false;
    assert(shell_bridge_route(&corner,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_UP);
    /* Full drag life-cycle is handed to Ring 3, including release. */
    input_event_t drag={.kind=INPUT_BUTTON,.left=true,.x=130,.y=115};
    assert(shell_bridge_route(&drag,false,768));
    assert(shell_bridge_pop(&event) && event.type==OXP_SHELL_EVENT_POINTER &&
           event.flags==OXP_SHELL_POINTER_DOWN);
    drag.kind=INPUT_MOVE;drag.x=150;drag.y=140;
    assert(shell_bridge_route(&drag,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_MOVE);
    drag.kind=INPUT_BUTTON;drag.left=false;
    assert(shell_bridge_route(&drag,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_UP);
    /* Shell controls activate on release: the compositor receives separate
     * down, coalesced move and up events while owning the pointer. */
    input_event_t icon={.kind=INPUT_BUTTON,.left=true,.x=32,.y=46};
    assert(shell_bridge_route(&icon,false,768));
    assert(shell_bridge_pop(&event) && event.type==OXP_SHELL_EVENT_POINTER &&
           event.flags==OXP_SHELL_POINTER_DOWN);
    icon.kind=INPUT_MOVE;icon.x=48;icon.y=48;
    assert(shell_bridge_route(&icon,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_MOVE);
    icon.kind=INPUT_BUTTON;icon.left=false;
    assert(shell_bridge_route(&icon,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_UP);
    icon.left=true;icon.x=300;icon.y=300;
    assert(!shell_bridge_route(&icon,false,768));
    /* Aurora 16: regular client releases are mirrored to the shell for
     * cross-window drop completion, never consumed from the source app. */
    icon.left=false;icon.x=310;icon.y=320;
    assert(!shell_bridge_route(&icon,false,768));
    assert(shell_bridge_pop(&event) && event.type==OXP_SHELL_EVENT_POINTER &&
           event.flags==OXP_SHELL_POINTER_UP && event.x==310 && event.y==320);
    /* Bare movement reports hover without consuming native application
     * input. Adjacent moves collapse to the most recent coordinates. */
    input_event_t motion={.kind=INPUT_MOVE,.x=20,.y=30};
    assert(!shell_bridge_route(&motion,false,768));
    motion.x=30;motion.y=35;
    assert(!shell_bridge_route(&motion,false,768));
    motion.x=40;motion.y=40;
    assert(!shell_bridge_route(&motion,false,768));
    assert(shell_bridge_pop(&event) && event.type==OXP_SHELL_EVENT_POINTER &&
           event.flags==OXP_SHELL_POINTER_MOVE && event.x==40 && event.y==40);
    assert(!shell_bridge_pop(&event));
    input_event_t mouse={.kind=INPUT_BUTTON,.left=true,.x=40,.y=740};
    assert(shell_bridge_route(&mouse,false,768));
    assert(shell_bridge_pop(&event) && event.type==OXP_SHELL_EVENT_POINTER &&
           event.flags==OXP_SHELL_POINTER_DOWN && event.x==40);
    /* Releasing over a different target gives windowd a chance to cancel. */
    mouse.kind=INPUT_MOVE;mouse.x=250;mouse.y=740;
    assert(shell_bridge_route(&mouse,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_MOVE);
    mouse.kind=INPUT_BUTTON;mouse.left=false;
    assert(shell_bridge_route(&mouse,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_UP);
    /* Neither pointer movement nor release can swallow keyboard input. */
    motion=(input_event_t){.kind=INPUT_MOVE,.x=88,.y=88};
    assert(!shell_bridge_route(&motion,false,768));
    key.code=KEY_CHARACTER;key.key='z';
    assert(!shell_bridge_route(&key,false,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_MOVE);
    assert(!shell_bridge_pop(&event));
    assert(shell_bridge_action(OXP_SHELL_MENU,1,0));
    assert(shell_bridge_menu_open());
    assert(shell_bridge_route(&key,true,768));
    assert(shell_bridge_pop(&event) && event.type==OXP_SHELL_EVENT_KEY);
    /* Clicking outside an open menu must first dismiss it, not accidentally
     * activate the application behind the popup. */
    mouse=(input_event_t){.kind=INPUT_BUTTON,.left=true,.x=800,.y=300};
    assert(shell_bridge_route(&mouse,true,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_DOWN);
    mouse.left=false;
    assert(shell_bridge_route(&mouse,true,768));
    assert(shell_bridge_pop(&event) && event.flags==OXP_SHELL_POINTER_UP);
    oxp_shell_draw_t invalid=cmd;invalid.w=1025;
    assert(!shell_bridge_submit(7,&invalid,1,6));
    invalid=cmd;invalid.op=OXP_SHELL_TEXT;
    memset(invalid.text,'Q',sizeof(invalid.text));
    assert(!shell_bridge_submit(7,&invalid,1,6));
    fb_surface_t s={(volatile uint8_t *)(pixels+1),32,32,32*4,8,8,8,16,8,0,false,0,0,0,0};
    pixels[0]=0x55aaff;pixels[32*32+1]=0x55aaff;
    shell_bridge_compose(&s);
    assert(pixels[1]==0x345678);
    /* Ring 3 controls window order, never the framebuffer memory itself. */
    oxp_shell_draw_t plan[4]={
        {.op=OXP_SHELL_BG_RECT,.x=0,.y=0,.w=3,.h=3,.color=0x123456},
        {.op=OXP_SHELL_COMPOSE_BEGIN},
        {.op=OXP_SHELL_COMPOSE_WINDOW,.x=DESKTOP_NOTEPAD},
        {.op=OXP_SHELL_COMPOSE_WINDOW,.x=DESKTOP_TERMINAL}
    };
    uint32_t order[DESKTOP_WINDOWS],length=99;
    assert(shell_bridge_submit(7,plan,4,6));
    assert(shell_bridge_window_order(order,DESKTOP_WINDOWS,&length));
    assert(shell_bridge_has_window_plan());
    assert(length==2 && order[0]==DESKTOP_NOTEPAD && order[1]==DESKTOP_TERMINAL);
    assert(!shell_bridge_window_order(order,1,&length));
    plan[3].x=DESKTOP_NOTEPAD; /* Duplicate indices could hide a window. */
    assert(!shell_bridge_submit(7,plan,4,6));
    plan[3].x=DESKTOP_TERMINAL;
    plan[1].op=OXP_SHELL_RECT;plan[1].w=1;plan[1].h=1;
    assert(!shell_bridge_submit(7,plan,4,6)); /* Missing BEGIN. */
    plan[1]=(oxp_shell_draw_t){.op=OXP_SHELL_COMPOSE_BEGIN};
    plan[2].text[0]='A';
    assert(!shell_bridge_submit(7,plan,4,6)); /* Reserved bytes. */
    plan[2].text[0]=0;
    assert(shell_bridge_window_order(order,DESKTOP_WINDOWS,&length));
    assert(length==2); /* Rejected frames never replace the active plan. */
    oxp_shell_draw_t empty={.op=OXP_SHELL_COMPOSE_BEGIN};
    assert(shell_bridge_submit(7,&empty,1,6));
    assert(shell_bridge_window_order(order,DESKTOP_WINDOWS,&length) && !length);
    cmd.op=OXP_SHELL_BG_RECT;cmd.x=4;
    assert(shell_bridge_submit(7,&cmd,1,7));
    shell_bridge_compose_background(&s);
    assert(pixels[5]==0x345678);
    cmd.op=OXP_SHELL_RECT;cmd.x=0;
    assert(shell_bridge_submit(7,&cmd,1,8));
    assert(pixels[1]==0x345678 && pixels[32*32+1]==0x55aaff);
    /* Two complete pixel layers are published atomically by windowd.
     * Incomplete updates must leave the previous scene unchanged. */
    static oxp_shell_raster_row_t row;
    row.layer=3;
    assert(!shell_bridge_raster_row(7,&row));
    row.layer=0;
    assert(!shell_bridge_raster_commit(7,OXP_SHELL_MASK_BACKGROUND));
    for(unsigned layer=0;layer<2;++layer){
        row.layer=layer;
        for(unsigned x=0;x<OXP_SHELL_RASTER_W;++x)
            row.pixels[x]=layer?OXP_SHELL_TRANSPARENT:0xf800u;
        for(unsigned y=0;y<OXP_SHELL_RASTER_H;++y){
            row.y=y;
            if(layer==1 && y==0)row.pixels[1]=0x07e0u;
            assert(shell_bridge_raster_row(7,&row));
            if(layer==1 && y==0)row.pixels[1]=OXP_SHELL_TRANSPARENT;
        }
    }
    assert(shell_bridge_raster_commit(7,OXP_SHELL_MASK_BACKGROUND|OXP_SHELL_MASK_OVERLAY));
    assert(shell_bridge_scene_version()>scene_epoch);
    assert(shell_bridge_raster_ready(0) && shell_bridge_raster_ready(1));
    shell_bridge_compose_background(&s);
    shell_bridge_compose(&s);
    assert(pixels[1]==0xff0000u && pixels[2]==0x00ff00u);
    pixels[1]=0xaabbccu;pixels[1+3+3*32]=0xaabbccu;
    fb_clip_set(&s,3,3,2,1);
    shell_bridge_compose_background(&s);
    fb_clip_clear(&s);
    assert(pixels[1]==0xaabbccu);
    assert(pixels[1+3+3*32]==0xff0000u);

    row.layer=1;row.y=0;
    row.pixels[1]=0x001fu;
    assert(shell_bridge_raster_row(7,&row));
    assert(!shell_bridge_raster_commit(7,OXP_SHELL_MASK_OVERLAY));
    shell_bridge_compose(&s);
    assert(pixels[2]==0x00ff00u); /* no tearing from incomplete back bank */
    for(unsigned y=1;y<OXP_SHELL_RASTER_H;++y){
        row.y=y;row.pixels[1]=OXP_SHELL_TRANSPARENT;
        assert(shell_bridge_raster_row(7,&row));
    }
    assert(shell_bridge_raster_commit(7,OXP_SHELL_MASK_OVERLAY));
    shell_bridge_compose(&s);
    assert(pixels[2]==0x0000ffu);
    /* An uncommitted native layer stays invisible. Full-window pixels come
       from the service; a failed service exposes the old kernel fallback. */
    row.layer=OXP_SHELL_LAYER_NATIVE;
    for(unsigned x=0;x<OXP_SHELL_RASTER_W;++x)row.pixels[x]=OXP_SHELL_TRANSPARENT;
    row.pixels[0]=0xf81fu;
    row.y=0;assert(shell_bridge_raster_row(7,&row));
    assert(!shell_bridge_raster_ready(OXP_SHELL_LAYER_NATIVE));
    assert(!shell_bridge_raster_commit(7,OXP_SHELL_MASK_NATIVE));
    for(unsigned y=1;y<OXP_SHELL_RASTER_H;++y){row.y=y;assert(shell_bridge_raster_row(7,&row));}
    assert(shell_bridge_raster_commit(7,OXP_SHELL_MASK_NATIVE));
    assert(shell_bridge_raster_ready(OXP_SHELL_LAYER_NATIVE));
    shell_bridge_compose_native(&s);
    assert(pixels[1]==0xff00ffu);
    assert(!shell_bridge_raster_commit(999u,OXP_SHELL_MASK_OVERLAY));
    /* Partial commits update an already-published native layer without
     * requiring the other 767 rows or accepting mixed full/partial masks. */
    assert(!shell_bridge_raster_commit(7,OXP_SHELL_MASK_NATIVE_PARTIAL));
    row.layer=OXP_SHELL_LAYER_NATIVE;row.y=5;
    for(unsigned x=0;x<OXP_SHELL_RASTER_W;++x)
        row.pixels[x]=OXP_SHELL_TRANSPARENT;
    row.pixels[0]=0x07e0u;
    assert(shell_bridge_raster_row(7,&row));
    assert(!shell_bridge_raster_commit(7,OXP_SHELL_MASK_NATIVE|
                                         OXP_SHELL_MASK_NATIVE_PARTIAL));
    assert(shell_bridge_raster_commit(7,OXP_SHELL_MASK_NATIVE_PARTIAL));
    /* Flush any previously pending full commit, then verify partial damage
     * has its own bounded repaint and full redraws supersede it. */
    unsigned damage_first=0,damage_last=0;
    assert(shell_bridge_take_dirty_region(&damage_first,&damage_last));
    assert(damage_first==0u && damage_last==OXP_SHELL_RASTER_H);
    row.y=6;
    assert(shell_bridge_raster_row(7,&row));
    assert(shell_bridge_raster_commit(7,OXP_SHELL_MASK_NATIVE_PARTIAL));
    assert(shell_bridge_take_dirty_region(&damage_first,&damage_last));
    assert(damage_first==6u && damage_last==7u);
    assert(!shell_bridge_take_dirty_region(&damage_first,&damage_last));
    shell_bridge_compose_native(&s);
    assert(pixels[5*32+1]==0x00ff00u);
    assert(pixels[1]==0xff00ffu); /* unrelated row preserved */
    assert(!shell_bridge_raster_commit(7,OXP_SHELL_MASK_NATIVE_PARTIAL));
    assert(!shell_bridge_raster_commit(7,16u));
    /* A subsequent complete frame must work even after individual rows
     * have different front-bank indices. */
    row.layer=OXP_SHELL_LAYER_NATIVE;
    row.pixels[0]=0x001fu;
    for(unsigned y=0;y<OXP_SHELL_RASTER_H;++y){
        row.y=y;
        assert(shell_bridge_raster_row(7,&row));
    }
    assert(shell_bridge_raster_commit(7,OXP_SHELL_MASK_NATIVE));
    shell_bridge_compose_native(&s);
    assert(pixels[1]==0x0000ffu && pixels[5*32+1]==0x0000ffu);
    shell_bridge_watchdog(307);
    assert(shell_bridge_active());
    shell_bridge_watchdog(309);
    assert(!shell_bridge_active() && !shell_bridge_has_window_plan() && shell_bridge_take_dirty());
    assert(!shell_bridge_raster_ready(0));
    assert(!shell_bridge_window_order(order,DESKTOP_WINDOWS,&length));
    assert(!shell_bridge_route(&mouse,false,768));
    assert(shell_bridge_submit(9,&cmd,1,400));
    shell_bridge_owner_exit(8);
    assert(shell_bridge_active());
    shell_bridge_owner_exit(9);
    assert(!shell_bridge_active());
    puts("PASS: Ring 3 shell frame validation, input routing, failure fallback, watchdog");
}
#elif defined(OPENXP_TEST_SHELL_INTERACTION)
#include "../apps/native/shell_interaction.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    int held=-1;
    assert(oxp_shell_finish_press(&held,1)==-1 && held==-1);
    held=1;
    assert(oxp_shell_finish_press(&held,1)==1 && held==-1);
    held=1;
    assert(oxp_shell_finish_press(&held,400)==-1 && held==-1);
    held=302;
    assert(oxp_shell_finish_press(&held,301)==-1 && held==-1);
    held=302;
    assert(oxp_shell_finish_press(&held,302)==302 && held==-1);
    held=105;
    assert(oxp_shell_finish_press(&held,-1)==-1 && held==-1);
    assert(oxp_shell_finish_press(0,100)==-1);
    puts("PASS: press/release activation, drag cancellation, stale press reset");
    return 0;
}
#else
#error "Compile this suite with an OPENXP_TEST_* selector"
#endif
