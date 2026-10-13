#include "gui/desktop.h"
#include "fs/ramfs.h"
#include "gui/native.h"
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
    assert(((click>>10)&1023u)==25u && (click&1023u)==60u);
    assert(native_next_event(90)==0 && native_next_event(92)==0);
    native_owner_exit(91);desktop_native_show(&fast,1,false);
    assert(!fast.windows[DESKTOP_NATIVE2].open);
    assert(fast.windows[DESKTOP_NATIVE].open);
    assert(fast.windows[DESKTOP_NATIVE3].open);
    puts("PASS: desktop, file editing, calendar, and multi-process native window input routing");
}
