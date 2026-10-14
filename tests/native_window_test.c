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
    assert((unsigned)(size>>32)<320u && (unsigned)size==180u);
    assert(native_next_event(100)==NATIVE_EVENT_RESIZE);
    assert(native_has_surface(1)); /* no flickering during resize */
    assert(native_test_pixel(1,3,5)==0xf800);
    native_set_window_size(1,1024u,726u); /* idempotent */
    assert(native_next_event(100)==0);
    native_set_window_size(1,640u,190u);
    size=native_canvas_size(100);
    assert((unsigned)(size>>32)==320u && (unsigned)size<180u);
    assert(native_next_event(100)==NATIVE_EVENT_RESIZE);
    assert(native_canvas_size(42)==((uint64_t)320<<32|180u));
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
