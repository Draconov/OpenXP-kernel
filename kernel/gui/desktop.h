#ifndef OPENXP_GUI_DESKTOP_H
#define OPENXP_GUI_DESKTOP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "video/fb.h"
#include "shell/shell.h"
#include "input/input.h"
#include "gui/native.h"

enum { DESKTOP_TERMINAL=0, DESKTOP_FILES=1, DESKTOP_ABOUT=2, DESKTOP_NOTEPAD=3, DESKTOP_CALCULATOR=4, DESKTOP_PAINT=5, DESKTOP_MONITOR=6, DESKTOP_SETTINGS=7, DESKTOP_MINES=8, DESKTOP_CLOCK=9, DESKTOP_SNAKE=10, DESKTOP_RUN=11, DESKTOP_WORDPAD=12, DESKTOP_CALENDAR=13, DESKTOP_NATIVE=14, DESKTOP_NATIVE2=15, DESKTOP_NATIVE3=16, DESKTOP_WINDOWS=17 };
typedef struct { int x,y,w,h; bool open,minimized; } desktop_window_t;
typedef struct {
    fb_surface_t *surface;
    desktop_window_t windows[DESKTOP_WINDOWS];
    uint32_t wm_seen_geometry[NATIVE_MAX_WINDOWS];
    uint32_t wm_seen_focus[NATIVE_MAX_WINDOWS];
    int order[DESKTOP_WINDOWS]; /* low to high: focused window last */
    bool start_open,dragging,mouse_down,right_down,showing_desktop;
    bool saved_minimized[DESKTOP_WINDOWS];
    unsigned menu_selection;
    int drag_window,last_x,last_y,mouse_x,mouse_y;
    int selected_file;
    char file_folder[48];
    char file_preview[512];
    size_t file_preview_len;
    unsigned file_scroll;
    bool file_preview_text;
    uint64_t last_file_click;
    char output[56][90];
    unsigned line_total;
    shell_t shell;
    char note[2048]; size_t note_length,note_cursor;
    bool note_saved,word_saved,paint_saved;
    char word[4096]; size_t word_length,word_cursor;
    unsigned word_scale; bool word_bold;
    unsigned calendar_year,calendar_month,calendar_day;
    unsigned today_year,today_month,today_day;
    bool calendar_has_today;
    uint64_t calc_entry,calc_accumulator;
    char calc_operator; bool calc_has_accumulator,calc_new_entry,calc_error;
    /* Built-in Paint canvas and brush; RAM-only, 160x100 pixels. */
    uint8_t paint[160*100];
    unsigned paint_color;
    bool paint_active,paint_dirty;
    int paint_min_x,paint_min_y,paint_max_x,paint_max_y;
    int paint_last_x,paint_last_y;
    /* Appearance presets change the wallpaper until reboot. */
    unsigned appearance;
    /* 9x9 Mines game; -1 denotes a mine and 0..8 neighbor counts. */
    int8_t mine_values[81];
    uint8_t mine_cells[81]; /* 0 hidden, 1 revealed, 2 flagged */
    unsigned mine_flags,mine_revealed,mine_games;
    bool mine_started,mine_lost,mine_won;
    /* Clock: monotonic time, 100 PIT ticks per second. No wall-clock claim. */
    uint64_t clock_ticks;
    uint64_t watch_started,watch_elapsed,watch_limit;
    bool watch_running,watch_countdown,watch_alarm;
    /* Snake: fixed 20 x 14 arena, deterministic per-game RNG. */
    uint16_t snake_segments[280];
    uint16_t snake_length,snake_food;
    uint8_t snake_direction,snake_next_direction;
    uint32_t snake_rng,snake_score;
    uint64_t snake_last_step;
    bool snake_started,snake_paused,snake_lost,snake_won;
    /* XP-style Run dialog. Commands are bounded, no filesystem execution. */
    char run_input[64],run_status[76];
    size_t run_length;

    /* The 9x13 software cursor has its own saved underlay. Movement never
       needs a complete redraw of the 1024x768 framebuffer. */
    uint32_t cursor_background[9*13];
    int cursor_saved_x,cursor_saved_y;
    bool cursor_painted;
} desktop_t;
void desktop_init(desktop_t *d,fb_surface_t *s,shell_services_t services);
/* Privileged execution of decisions made by the native Ring 3 shell. */
bool desktop_shell_open(desktop_t *d,uint32_t index);
bool desktop_shell_close(desktop_t *d);
void desktop_shell_desktop(desktop_t *d);
bool desktop_shell_task(desktop_t *d,uint32_t index);
bool desktop_shell_focus(desktop_t *d,uint32_t index);
const char *desktop_shell_title(desktop_t *d,uint32_t index);
void desktop_native_show(desktop_t *d,unsigned slot,bool showing);
/* Reconcile trusted kernel window metadata with the existing compositor.
 * Still Ring 0 composition; windowd authorizes remote move/focus commands. */
void desktop_native_sync(desktop_t *d,unsigned slot,const native_window_t *native);
void desktop_calendar_set_today(desktop_t *d,unsigned year,unsigned month,unsigned day);
unsigned desktop_calendar_days(unsigned year,unsigned month);
unsigned desktop_calendar_weekday(unsigned year,unsigned month,unsigned day);
void desktop_draw(desktop_t *d);
/* Recompose only the rectangle vacated/occupied by a dragged window. */
void desktop_redraw_moved_window(desktop_t *d,int oldx,int oldy,int index);
void desktop_redraw_terminal(desktop_t *d);
/* Refresh only the paint pixels dirtied by a brush stroke. */
void desktop_redraw_paint(desktop_t *d);
void desktop_redraw_active(desktop_t *d);
void desktop_redraw_taskbar(desktop_t *d);
void desktop_mouse(desktop_t *d,int x,int y,bool down);
void desktop_right_mouse(desktop_t *d,int x,int y,bool down);
void desktop_cursor_update(desktop_t *d);
void desktop_key_event(desktop_t *d,const input_event_t *event);
void desktop_key(desktop_t *d,char c);
void desktop_tick(desktop_t *d,uint64_t ticks);
int desktop_top_window(const desktop_t *d);
#endif
