/* OpenXP host test suite: apps.
 * Select exactly one original test per compilation using -DOPENXP_TEST_NAME.
 * Individual test binaries and sanitizers remain unchanged. */
#if defined(OPENXP_TEST_CALCULATOR_LAYOUT)
#include "../apps/native/calculator_layout.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    for(unsigned w=100u;w<=640u;w+=45u)for(unsigned h=90u;h<=360u;h+=30u){
        for(unsigned r=0;r<4u;++r)for(unsigned c=0;c<4u;++c){
            oxp_calc_rect b=oxp_calc_key(w,h,c,r);
            assert(b.w>=1u&&b.h>=1u);
            assert(b.x+b.w<=w&&b.y+b.h<=h);
            assert(oxp_calc_key_at(w,h,b.x+b.w/2u,b.y+b.h/2u)==(int)(r*4u+c));
        }
        assert(oxp_calc_key_at(w,h,0u,0u)==-1);
    }
    assert(oxp_calc_key(10,10,0,0).w==0u);
    puts("PASS: Aurora calculator artwork positions and click geometry");
    return 0;
}
#elif defined(OPENXP_TEST_NATIVE_LAYOUT)
#include "../kernel/gui/native_viewport.h"
#include "../apps/native/ui_layout.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

static void verify(unsigned window_w,unsigned window_h){
    oxp_native_canvas_t c=oxp_native_canvas(window_w,window_h);
    oxp_native_viewport_t viewport=oxp_native_viewport(0,0,window_w,window_h);
    assert(c.w>0 && c.w<=OXP_NATIVE_W && c.h>0 && c.h<=OXP_NATIVE_H);
    assert(viewport.w==window_w-22u && viewport.h==window_h-49u);
    uint64_t lhs=(uint64_t)c.w*viewport.h,rhs=(uint64_t)c.h*viewport.w;
    uint64_t rounding=viewport.w>viewport.h?viewport.w:viewport.h;
    assert(lhs<=rhs+rounding && rhs<=lhs+rounding);
    oxp_paint_layout_t paint=oxp_paint_layout(c.w,c.h);
    assert(paint.pixel>=1 && paint.pixel<=3);
    if(c.w>=110u)assert(paint.board.x+paint.board.w<paint.palette_x);
    for(unsigned i=0;i<8u;++i){
        oxp_ui_rect_t swatch=oxp_paint_palette_box(paint,i);
        assert(swatch.x+swatch.w<=c.w);
        if(c.h>=145u)assert(swatch.y+swatch.h<=c.h);
    }
    if(c.w>=76u)assert(paint.board.x+paint.board.w<=c.w);
    if(c.h>=95u)assert(paint.board.y+paint.board.h<=c.h);
    oxp_grid_layout_t mines=oxp_mines_layout(c.w,c.h);
    oxp_grid_layout_t snake=oxp_snake_layout(c.w,c.h);
    assert(mines.x+9u*mines.tile<=c.w);
    assert(mines.y+9u*mines.tile<=c.h);
    assert(snake.x+20u*snake.tile<=c.w);
    assert(snake.y+10u*snake.tile<=c.h);
    uint32_t cx=0,cy=0;
    oxp_native_pointer_to_canvas(viewport.x,viewport.y,window_w,window_h,&cx,&cy);
    assert(cx==16u && cy==43u);
    oxp_native_pointer_to_canvas(viewport.x+viewport.w-1u,viewport.y+viewport.h-1u,
                                 window_w,window_h,&cx,&cy);
    assert(cx>=16u && cx<16u+c.w && cy>=43u && cy<43u+c.h);
    oxp_native_pointer_to_canvas(viewport.x-1u,viewport.y,window_w,window_h,&cx,&cy);
    assert(cx==1023u && cy==1023u);
}
int main(void){
    verify(240,190); /* native minimum */
    verify(280,200); /* narrow restored */
    verify(470,300); /* default */
    verify(640,480); /* large restored */
    verify(1024,726); /* maximized */
    verify(640,190); /* wide and short */
    verify(1024,190); /* extreme layout: important controls remain reachable */
    for(unsigned w=240;w<=1024;w+=37)
        for(unsigned h=190;h<=726;h+=31)verify(w,h);
    puts("PASS: adaptive native canvas/layout, palette/game fit and pointer coordinates");
}
#elif defined(OPENXP_TEST_NATIVE_TEXT_LAYOUT)
#include "../apps/native/text_layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void check(const char *text,unsigned cols){
    size_t len=strlen(text);
    for(size_t i=0;i<=len;++i){
        oxp_text_position_t p=oxp_text_position(text,len,i,cols);
        assert(p.col<cols);
        assert(oxp_text_offset_at(text,len,cols,p.line,p.col)==i);
    }
}
int main(void){
    check("",8);check("12345678",8);check("123456789",8);
    check("12345678\n",8);check("\n",8);check("alpha\nbeta\n",4);
    check("The quick brown fox jumps over the lazy dog",7);
    for(unsigned cols=1;cols<=49;++cols){
        check("one long line without any line feeds, so words wrap.",cols);
        check("line one\nline two\n\nfinal line\n",cols);
    }
    puts("PASS: wrapped editor cursor/row coordinates and end-of-line positions");
}
#elif defined(OPENXP_TEST_NATIVE_SCROLL)
#include "../apps/native/scroll_layout.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    assert(oxp_scroll_limit(10,15)==0);
    assert(oxp_scroll_limit(30,7)==23);
    assert(oxp_scroll_clamp(99,30,7)==23);
    assert(oxp_scroll_step(0,-1,30,7)==0);
    assert(oxp_scroll_step(0,1,30,7)==1);
    assert(oxp_scroll_step(99,1,30,7)==23);
    assert(oxp_scroll_follow(0,16,8)==9);
    assert(oxp_scroll_follow(10,2,8)==2);
    assert(oxp_scroll_follow(10,12,8)==10);
    assert(oxp_scroll_thumb(0,10,15,100).h==100);
    assert(oxp_scroll_thumb(0,100,10,80).y==0);
    oxp_scroll_thumb_t end=oxp_scroll_thumb(90,100,10,80);
    assert(end.h>=10 && end.y+end.h==80);
    for(unsigned total=0;total<500;++total)
        for(unsigned visible=1;visible<80;++visible){
            oxp_scroll_thumb_t t=oxp_scroll_thumb(total,total,visible,37);
            assert(t.y+t.h<=37);
        }
    puts("PASS: bounded scroll math, caret following, thumb position and clamping");
    return 0;
}
#elif defined(OPENXP_TEST_NATIVE_WIDGETS)
#include "../apps/native/ui_widgets.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    oxp_ui_rect rect={10u,20u,80u,25u};
    assert(oxp_ui_contains(rect,10u,20u));
    assert(oxp_ui_contains(rect,89u,44u));
    assert(!oxp_ui_contains(rect,90u,44u));
    assert(!oxp_ui_contains(rect,89u,45u));
    assert(!oxp_ui_contains((oxp_ui_rect){10u,20u,0u,1u},10u,20u));
    assert(oxp_ui_clamp(0u,10u,20u)==10u);
    assert(oxp_ui_clamp(15u,10u,20u)==15u);
    assert(oxp_ui_clamp(30u,10u,20u)==20u);
    assert(oxp_ui_hit(OXP_CLICK | ((rect.x+16u)<<10) | (rect.y+43u),rect));
    assert(!oxp_ui_hit(0u,rect));
    uint8_t hdr[16]={'O','X','U','A',1,0,192,0,128,0,0,0,0,32,1,0};
    /* 192*128*3 = 73728 = 0x12000. */
    assert(oxp_ui_header_ok(hdr,192u,128u));
    hdr[10]=1;assert(!oxp_ui_header_ok(hdr,192u,128u));hdr[10]=0;
    hdr[0]='B';assert(!oxp_ui_header_ok(hdr,192u,128u));
    assert(oxp_ui_axis(0,100,63,6,6)==0);
    assert(oxp_ui_axis(99,100,63,6,6)==62);
    assert(oxp_ui_axis(1,10,63,6,6)<63);
    puts("PASS: Aurora widget rectangle geometry, clamp, input hit-testing");
    return 0;
}
#elif defined(OPENXP_TEST_NATIVE_GAMES)
#include "../apps/native/game_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    assert(oxp_leap(2000)&&!oxp_leap(1900)&&oxp_leap(2024)&&!oxp_leap(2025));
    assert(oxp_days(2024,2)==29&&oxp_days(2025,2)==28);
    assert(oxp_weekday(2026,10,13)==1); /* Tuesday */
    assert(oxp_weekday(2000,1,1)==5); /* Saturday */
    oxp_mines_t m;oxp_mines_reset(&m,53);
    assert(!m.started&&!m.revealed_count);
    oxp_mines_flag(&m,0);assert(m.flagged[0]&&m.flags==1);
    oxp_mines_flag(&m,0);assert(!m.flagged[0]&&!m.flags);
    oxp_mines_reveal(&m,40);
    assert(m.started && !m.mines[40] && m.revealed[40] && !m.ended);
    unsigned count=0;
    for(unsigned i=0;i<81;++i)if(m.mines[i]){++count;assert(!oxp_neighbours(i,40));}
    assert(count==10);
    oxp_mines_flag(&m,40);assert(m.flags==0);
    oxp_mines_reveal(&m,81);assert(m.revealed_count<=71);
    oxp_snake_t s;oxp_snake_reset(&s,9);
    assert(s.length==3 && s.dir==1 && !s.dead && !s.started);
    oxp_snake_turn(&s,3);assert(s.dir==1); /* no instant reversal */
    s.started=1;s.food=0;
    oxp_snake_step(&s);assert(s.body[0]==5*20+11 && s.length==3);
    oxp_snake_turn(&s,2);oxp_snake_step(&s);
    assert(s.body[0]==6*20+11);
    oxp_snake_reset(&s,1);s.started=1;
    for(unsigned n=0;n<30 && !s.dead;++n){s.food=0;oxp_snake_step(&s);}
    assert(s.dead);
    puts("PASS: native Mines/Snake/Calendar game logic");
}
#elif defined(OPENXP_TEST_NATIVE_WINDOW)
#include "gui/native.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    native_reset();
    assert(native_dirty_take() && !native_dirty_take());
    assert(!native_snapshot().active && !native_next_event(1));
    assert(!native_open(0,"Native",6));
    assert(!native_open(1,"",0));
    assert(!native_open(1,"\n",1));
    assert(native_open(42,"Calculator",10));
    assert(native_open(43,"Clock",5));
    assert(native_open(44,"Notepad",7));
    assert(!native_open(45,"Overflow",8));
    assert(native_slot_for_owner(42)==0);
    assert(native_slot_for_owner(43)==1);
    assert(native_slot_for_owner(44)==2);
    assert(native_slot_for_owner(45)==-1);
    assert(native_dirty_take() && !native_dirty_take());
    assert(!native_set_text(45,"no",2));
    assert(!native_set_text(42,"bad\001",4));
    assert(native_set_text(42,"hello\nworld",11));
    assert(native_set_text(43,"clock",5));
    assert(native_set_text(44,"note",4));
    native_window_t a=native_snapshot_slot(0),b=native_snapshot_slot(1),c=native_snapshot_slot(2);
    assert(a.active && a.owner==42 && !strcmp(a.title,"Calculator"));
    assert(!strcmp(a.text,"hello\nworld"));
    assert(!strcmp(b.text,"clock")&&!strcmp(c.text,"note"));
    assert(native_dirty_take());
    /* Real WM geometry and focus commands go only to an existing PID. */
    assert(!native_wm_manage(45,1,10,20,220,120));
    assert(!native_wm_manage(42,1,950,20,220,120));
    assert(!native_wm_manage(42,1,40,700,220,120));
    assert(!native_wm_manage(42,99,0,0,0,0));
    assert(native_wm_manage(42,1,10,20,220,120));
    uint32_t geom=native_snapshot_slot(0).wm_geometry_serial;
    assert(geom && native_snapshot_slot(0).wm_width==220);
    assert(native_wm_manage(42,2,0,0,0,0));
    assert(native_snapshot_slot(0).wm_focus_serial>geom);
    assert(native_snapshot_slot(1).wm_geometry_serial==0);
    assert(native_dirty_take());
    native_push_event_slot(1,'C');
    native_push_event_slot(2,NATIVE_EVENT_CLICK | (39u<<10) | 51u);
    assert(native_next_event(42)==0);
    assert(native_next_event(43)=='C');
    uint32_t click=native_next_event(44);
    assert((click&NATIVE_EVENT_CLICK)!=0 && ((click>>10)&1023u)==39 && (click&1023u)==51);
    assert(native_next_event(43)==0 && native_next_event(44)==0);
    for(unsigned i=0;i<100;++i)native_push_event_slot(0,'A'+i%26);
    native_close_request_slot(0);
    assert(native_next_event(42)==NATIVE_EVENT_CLOSE);
    native_push_event_slot(0,'R'); /* Further input must not displace close. */
    assert(native_next_event(42)==NATIVE_EVENT_CLOSE);
    native_close_request_slot(1);
    assert(native_next_event(43)==NATIVE_EVENT_CLOSE);
    native_owner_exit(45);assert(native_snapshot_slot(2).active);
    native_owner_exit(43);assert(!native_snapshot_slot(1).active);
    assert(native_snapshot_slot(0).active && native_snapshot_slot(2).active);
    /* The freed slot can be reused without disturbing other owners. */
    assert(native_open(100,"New",3) && native_slot_for_owner(100)==1);
    assert(native_set_text(100,"new",3));
    assert(native_next_event(100)==0);
    assert(native_snapshot_slot(1).wm_geometry_serial==0);
    assert(native_wm_manage(100,1,70,96,240,140));
    assert(native_snapshot_slot(1).wm_x==70);
    assert(native_wm_manage(100,3,0,0,0,0));
    assert(native_next_event(100)==NATIVE_EVENT_CLOSE);
    /* Reopen is allowed after a polite close request. */
    assert(native_open(100,"New",3));
    assert(native_snapshot_slot(NATIVE_MAX_WINDOWS).active==false);
    assert(native_dirty_take());

    /* Strict command validation and per-process surface isolation. */
    native_draw_cmd_t cmds[]={
        {NATIVE_DRAW_CLEAR,0,0,0,0,0x224466},
        {NATIVE_DRAW_FILL,3,5,2,2,0xff0000}
    };
    assert(!native_draw(45,cmds,2)); /* Owner has already been reaped. */
    assert(native_draw(100,cmds,2));
    assert(native_has_surface(1));
    assert(!native_has_surface(0));
    uint16_t row[NATIVE_SURFACE_W];
    assert(native_copy_surface_row(1,5,row,NATIVE_SURFACE_W));
    assert(row[3]==0xf800 && row[4]==0xf800 && row[2]!=row[3]);
    assert(native_test_pixel(1,3,5)==0xf800);
    assert(native_canvas_size(100)==((uint64_t)320<<32|180u));
    assert(native_canvas_size(45)==0); /* PID ownership, not raw slot */
    native_set_window_size(1,1024u,726u);
    uint64_t size=native_canvas_size(100);
    assert((unsigned)(size>>32)>320u && (unsigned)size==360u);
    assert(native_next_event(100)==NATIVE_EVENT_RESIZE);
    assert(native_has_surface(1)); /* no flickering during resize */
    assert(native_test_pixel(1,3,5)==0xf800);
    assert(native_snapshot_slot(1).surface_w==320u);
    assert(native_snapshot_slot(1).surface_h==180u);
    native_set_window_size(1,1024u,726u); /* idempotent */
    assert(native_next_event(100)==0);
    native_set_window_size(1,640u,190u);
    size=native_canvas_size(100);
    assert((unsigned)(size>>32)==NATIVE_SURFACE_W && (unsigned)size<180u);
    assert(native_next_event(100)==NATIVE_EVENT_RESIZE);
    assert(native_canvas_size(42)==((uint64_t)320<<32|180u));
    /* New backing pixels above the historic 320x180 limit must survive
     * through the process-owned scanline syscall, not just layout math. */
    native_set_window_size(1,1024u,726u);
    assert(native_next_event(100)==NATIVE_EVENT_RESIZE);
    uint32_t expanded_w=(uint32_t)(native_canvas_size(100)>>32);
    uint32_t expanded_h=(uint32_t)native_canvas_size(100);
    assert(expanded_w>320u && expanded_h>180u);
    native_draw_cmd_t high_pixel={NATIVE_DRAW_FILL,expanded_w-1u,expanded_h-1u,1,1,0x00ff00};
    assert(native_draw(100,&high_pixel,1));
    assert(native_snapshot_slot(1).surface_w==expanded_w);
    assert(native_snapshot_slot(1).surface_h==expanded_h);
    assert(native_test_pixel(1,expanded_w-1u,expanded_h-1u)==0x07e0u);
    /* The WM consumes the accumulated pixel-damage interval at snapshot,
     * so a subsequent typed glyph transfers only seven logical rows. */
    native_window_t full_damage=native_snapshot_take_damage_slot(1);
    assert(full_damage.damage_y0==0 && full_damage.damage_y1==NATIVE_SURFACE_H);
    native_window_t no_damage=native_snapshot_take_damage_slot(1);
    assert(no_damage.damage_y0==no_damage.damage_y1);
    native_draw_cmd_t glyph={NATIVE_DRAW_GLYPH,12,27,'A',1,0xffffff};
    assert(native_draw(100,&glyph,1));
    native_window_t glyph_damage=native_snapshot_take_damage_slot(1);
    assert(glyph_damage.damage_y0==27u && glyph_damage.damage_y1==34u);
    native_draw_cmd_t small[]={{NATIVE_DRAW_FILL,8,40,2,3,0x112233},
                               {NATIVE_DRAW_FILL,8,50,2,2,0x112233}};
    assert(native_draw(100,small,2));
    native_window_t merged_damage=native_snapshot_take_damage_slot(1);
    assert(merged_damage.damage_y0==40u && merged_damage.damage_y1==52u);

    uint16_t high_row[NATIVE_SURFACE_W];
    assert(native_copy_surface_row(1,expanded_h-1u,high_row,NATIVE_SURFACE_W));
    assert(high_row[expanded_w-1u]==0x07e0u);
    assert(!native_copy_surface_row(1,NATIVE_SURFACE_H,row,NATIVE_SURFACE_W));
    assert(!native_copy_surface_row(1,5,row,NATIVE_SURFACE_W-1));
    native_draw_cmd_t bad[]={
        {NATIVE_DRAW_FILL,0,0,1,1,0x00ffffff},
        {NATIVE_DRAW_FILL,NATIVE_SURFACE_W-1,2,2,2,0x000000}
    };
    assert(!native_draw(100,bad,2)); /* Reject entire batch, no partial write. */
    assert(native_test_pixel(1,0,0)!=0xffff);
    bad[1]=(native_draw_cmd_t){123u,0,0,1,1,0};
    assert(!native_draw(100,bad,2));
    assert(!native_draw(999,cmds,2));
    assert(!native_draw(100,cmds,0));
    assert(!native_draw(100,cmds,NATIVE_DRAW_MAX+1));
    assert(!native_draw(100,(native_draw_cmd_t[]){{NATIVE_DRAW_FILL,0,0,1,1,0xff000000}},1));
    native_owner_exit(100);
    assert(!native_has_surface(1));
    /* Recycled slot must start cleared, not leak predecessor pixels. */
    assert(native_open(101,"Another",7));
    /* RGBA bitmap upload: alpha, bounds, ownership and transparent key. */
    native_bitmap_req_t bt={0};
    bt.x=5;bt.y=5;bt.w=2;bt.h=1;
    bt.pixels[0]=0x00;bt.pixels[1]=0xf8;bt.pixels[2]=255; /* red */
    bt.pixels[3]=0x1f;bt.pixels[4]=0x00;bt.pixels[5]=0;   /* transparent blue */
    assert(native_bitmap(101,&bt));
    assert(native_test_pixel(1,5,5)==0xf800u);
    assert(native_test_pixel(1,6,5)==0u);
    bt.w=1;
    bt.pixels[0]=0xe0;bt.pixels[1]=0x07;bt.pixels[2]=128; /* half-green over red */
    assert(native_bitmap(101,&bt));
    uint16_t mixed=native_test_pixel(1,5,5);
    assert(mixed!=0xf800u && mixed!=0x07e0u && mixed!=0x0001u);
    bt.pixels[0]=1;bt.pixels[1]=0;bt.pixels[2]=255;
    assert(native_bitmap(101,&bt));
    assert(native_test_pixel(1,5,5)==0); /* transparent compositor key protection */
    bt.w=2;

    assert(!native_bitmap(999,&bt));
    bt.x=NATIVE_SURFACE_W-1u;assert(!native_bitmap(101,&bt));
    bt.x=5;bt.w=33;assert(!native_bitmap(101,&bt));
    bt.w=2;bt.h=33;assert(!native_bitmap(101,&bt));
    bt.h=1;
    native_draw_cmd_t little={NATIVE_DRAW_FILL,3,5,1,1,0xff0000};
    assert(native_draw(101,&little,1));
    assert(native_test_pixel(1,3,5)==0xf800);
    assert(native_test_pixel(1,4,5)==0);
    native_owner_exit(101);
    native_owner_exit(42);
    native_owner_exit(44);
    assert(native_dirty_take() && !native_dirty_take());
    for(unsigned i=0;i<NATIVE_MAX_WINDOWS;++i)assert(!native_snapshot_slot(i).active);
    assert(!native_set_text(42,"test",4));
    puts("PASS: multi-owner GUI slots, text isolation, event routing, capacity, close and reuse");
}
#elif defined(OPENXP_TEST_NATIVE_WAIT)
#include "gui/native.h"
#include "sched/scheduler.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
static uint64_t tid=9, wake_id, last_timeout;
static unsigned parks,wakes;
static unsigned action;
uint64_t scheduler_current_id(void) { return tid; }
bool scheduler_wake(uint64_t thread_id) {
    wake_id=thread_id; ++wakes; return thread_id==tid;
}
static bool simulated_sleep(uint64_t ticks) {
    last_timeout=ticks; ++parks;
    if(action==1)native_push_event_slot(0,'Z');
    if(action==2)native_owner_exit(123);
    return true;
}
bool scheduler_sleep_current_locked(uint64_t ticks) { return simulated_sleep(ticks); }
bool scheduler_park_current_locked(void) { return simulated_sleep(0); }
int main(void) {
    native_reset();
    assert(native_open(123,"Events",6));
    assert(!native_wait_event(123,UINT64_MAX));
    native_push_event_slot(0,'A');
    assert(native_wait_event(123,0)=='A' && parks==0);
    action=1;
    assert(native_wait_event(123,0)=='Z');
    assert(parks==1 && last_timeout==0 && wakes==1 && wake_id==9);
    assert(native_snapshot().waiter==0);
    action=0;
    assert(native_wait_event(123,5)==0);
    assert(parks==2 && last_timeout==5 && native_snapshot().waiter==0);
    action=2;
    assert(native_wait_event(123,0)==NATIVE_EVENT_CLOSE);
    assert(!native_snapshot().active && wakes==2);
    assert(native_open(124,"Reused",6));
    assert(!native_next_event(124));
    assert(native_wait_event(123,0)==NATIVE_EVENT_CLOSE);
    assert(native_slot_for_owner(124)==0);
    puts("PASS: pending events, atomic wake, timed wait, exit and slot isolation");
    return 0;
}
#else
#error "Compile this suite with an OPENXP_TEST_* selector"
#endif
