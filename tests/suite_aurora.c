/* OpenXP host test suite: aurora.
 * Select exactly one original test per compilation using -DOPENXP_TEST_NAME.
 * Individual test binaries and sanitizers remain unchanged. */
#if defined(OPENXP_TEST_AURORA13_EDIT)
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../apps/native/edit_session.h"
#include "../apps/native/paint_codec.h"
#include "../apps/native/paint_history.h"
#include "../kernel/user/clipboard.h"
#include "../kernel/gui/native_keymap.h"

static openxp_clipboard_t shared;
static int64_t clip_put(const void *p,size_t n){
    return openxp_clipboard_put(&shared,p,n)?(int64_t)n:-1;
}
static int64_t clip_get(void *p,size_t cap){
    size_t n=openxp_clipboard_take(&shared,p,cap);
    return n==(size_t)-1?-28:(int64_t)n;
}
static void keymap_test(void){
    input_event_t e={.kind=INPUT_KEY,.code=KEY_CHARACTER,.modifiers=KEYMOD_CTRL,.key='C'};
    assert(native_editor_keymap(&e)==OXP_KEY_COPY);
    e.key='X';assert(native_editor_keymap(&e)==OXP_KEY_CUT);
    e.key='v';assert(native_editor_keymap(&e)==OXP_KEY_PASTE);
    e.key='a';assert(native_editor_keymap(&e)==OXP_KEY_SELECT_ALL);
    e.key='z';assert(native_editor_keymap(&e)==OXP_KEY_UNDO);
    e.modifiers=KEYMOD_CTRL|KEYMOD_SHIFT;
    assert(native_editor_keymap(&e)==OXP_KEY_REDO);
    e.key='y';assert(native_editor_keymap(&e)==OXP_KEY_REDO);
    e.key='s';assert(native_editor_keymap(&e)==0);
    e.code=KEY_HOME;e.key=0;
    assert(native_editor_keymap(&e)==OXP_KEY_SELECT_DOC_HOME);
    e.modifiers=KEYMOD_CTRL;assert(native_editor_keymap(&e)==OXP_KEY_DOC_HOME);
    e.modifiers=KEYMOD_SHIFT;assert(native_editor_keymap(&e)==OXP_KEY_SELECT_HOME);
    e.modifiers=0;assert(native_editor_keymap(&e)==OXP_KEY_HOME);
    e.code=KEY_DELETE;assert(native_editor_keymap(&e)==OXP_KEY_DELETE);
    e.code=KEY_RIGHT;e.modifiers=KEYMOD_SHIFT;
    assert(native_editor_keymap(&e)==OXP_KEY_SELECT_RIGHT);
    e.modifiers=KEYMOD_ALT;assert(native_editor_keymap(&e)==0);
}
static void clipboard_test(void){
    const uint8_t a[]={ 'A','\n','B' };
    uint8_t b[4]={0};
    assert(openxp_clipboard_put(&shared,a,3));
    assert(openxp_clipboard_take(&shared,b,2)==(size_t)-1);
    assert(openxp_clipboard_take(&shared,b,4)==3);
    assert(!memcmp(a,b,3));
    assert(!openxp_clipboard_put(&shared,a,OPENXP_CLIPBOARD_CAP+1));
    assert(shared.length==3);
    assert(openxp_clipboard_put(&shared,0,0));
    assert(shared.length==0);
}
static void editing_test(void){
    char s[960]="hello world";
    size_t n=11,cur=11,anchor=11;
    oxp_edit_history_t hist={0};
    oxp_history_init(&hist,s,n,cur,anchor);
#define EDIT(k) oxp_edit_key((k),s,&n,&cur,&anchor,sizeof s,80,&hist,clip_put,clip_get)
    assert(EDIT(OXP_KEY_SELECT_ALL)==1);
    assert(cur==n&&anchor==0);
    assert(EDIT(OXP_KEY_COPY)==1);
    assert(shared.length==n);
    assert(!memcmp(shared.bytes,"hello world",n));
    assert(EDIT(OXP_KEY_CUT)==2);
    assert(n==0&&cur==0&&anchor==0&&s[0]==0);
    assert(EDIT(OXP_KEY_UNDO)==2 && n==11 && !strcmp(s,"hello world"));
    assert(EDIT(OXP_KEY_REDO)==2 && n==0);
    assert(EDIT(OXP_KEY_PASTE)==2 && n==11 && !strcmp(s,"hello world"));
    assert(EDIT(OXP_KEY_UNDO)==2 && n==0);
    assert(EDIT(OXP_KEY_REDO)==2 && n==11);
    assert(EDIT(OXP_KEY_DOC_HOME)==1 && cur==0 && anchor==0);
    assert(EDIT(OXP_KEY_SELECT_RIGHT)==1 && cur==1 && anchor==0);
    assert(EDIT('Y')==2 && !strcmp(s,"Yello world"));
    assert(EDIT(OXP_KEY_UNDO)==2 && !strcmp(s,"hello world"));
    /* Undo restores the caret belonging to the previous committed edit.
     * Move explicitly before branching. */
    assert(EDIT(OXP_KEY_DOC_HOME)==1);
    /* Redo branch must be invalidated by a new edit. */
    assert(EDIT('X')==2 && !strcmp(s,"Xhello world"));
    assert(EDIT(OXP_KEY_REDO)==1 && !strcmp(s,"Xhello world"));
    /* Break the old typing run with a movement, then test a new run. */
    assert(EDIT(OXP_KEY_HOME)==1);
    assert(EDIT(OXP_KEY_END)==1);
    /* Typing consecutive characters is one undo step. */
    assert(EDIT('A')==2);assert(EDIT('B')==2);
    assert(EDIT(OXP_KEY_UNDO)==2 && !strcmp(s,"Xhello world"));
    assert(EDIT(OXP_KEY_REDO)==2 && !strcmp(s,"Xhello worldAB"));
    assert(EDIT(OXP_KEY_SELECT_ALL)==1);
    assert(EDIT(8)==2 && n==0);
    assert(EDIT(OXP_KEY_UNDO)==2 && n==14);
    assert(EDIT(OXP_KEY_END)==1 && cur==14 && anchor==14);
    assert(EDIT(OXP_KEY_SELECT_HOME)==1 && cur==0 && anchor==14);
    assert(EDIT(OXP_KEY_DELETE)==2 && n==0);
    assert(EDIT(OXP_KEY_UNDO)==2 && n==14);
    /* Reject malicious clipboard control bytes without modifying content. */
    const uint8_t bad[]={ 'x',0,'y' };
    assert(openxp_clipboard_put(&shared,bad,3));
    size_t old_n=n;char old[960];memcpy(old,s,n+1);
    assert(EDIT(OXP_KEY_PASTE)==1 && old_n==n && !memcmp(old,s,n+1));
    /* Reject oversized replacement atomically. */
    n=958;for(size_t i=0;i<n;++i)s[i]='k';s[n]=0;cur=anchor=n;
    oxp_history_init(&hist,s,n,cur,anchor);
    assert(openxp_clipboard_put(&shared,(const uint8_t *)"abc",3));
    assert(EDIT(OXP_KEY_PASTE)==1 && n==958 && cur==958);
    assert(EDIT('v')==2 && n==959 && s[959]==0);
    assert(EDIT('q')==1 && n==959);
#undef EDIT
}
static void paint_test(void){
    uint8_t frame[OXP_PAINT_SIZE]={0};
    oxp_paint_history_t h={0};
    oxp_paint_hist_init(&h,frame);
    frame[0]=1;oxp_paint_hist_record(&h,frame);
    frame[1]=2;oxp_paint_hist_record(&h,frame);
    assert(oxp_paint_hist_step(&h,-1,frame) && frame[0]==1 && frame[1]==0);
    assert(oxp_paint_hist_step(&h,-1,frame) && frame[0]==0 && frame[1]==0);
    assert(!oxp_paint_hist_step(&h,-1,frame));
    assert(oxp_paint_hist_step(&h,+1,frame) && frame[0]==1);
    frame[2]=3;oxp_paint_hist_record(&h,frame);
    assert(!oxp_paint_hist_step(&h,+1,frame));
    for(int i=0;i<20;++i){frame[0]=(uint8_t)i;oxp_paint_hist_record(&h,frame);}
    for(int i=0;i<7;++i)assert(oxp_paint_hist_step(&h,-1,frame));
    assert(!oxp_paint_hist_step(&h,-1,frame));
}
int main(void){
    keymap_test();clipboard_test();editing_test();paint_test();
    puts("PASS: Aurora 13 keyboard, clipboard, editing history, selection and Paint strokes");
    return 0;
}
#elif defined(OPENXP_TEST_AURORA14_WORKFLOW)
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../apps/native/file_picker.h"

static void association_test(void){
    assert(!strcmp(oxp_doc_program("home/readme.TXT"),"bin/notepad"));
    assert(!strcmp(oxp_doc_program("docs/system.log"),"bin/notepad"));
    assert(!strcmp(oxp_doc_program("home/config.ini"),"bin/notepad"));
    assert(!strcmp(oxp_doc_program("home/readme.md"),"bin/wordpad"));
    assert(!strcmp(oxp_doc_program("home/picture.oxp"),"bin/paint"));
    assert(oxp_doc_program("bin/windowd")==NULL);
    assert(oxp_doc_program("home/untrusted.exe")==NULL);
    assert(oxp_doc_program("home/evil.txt.exe")==NULL);
    assert(!strcmp(oxp_doc_mailbox("bin/wordpad"),"home/open-wordpad.cfg"));
    assert(!strcmp(oxp_doc_mailbox("bin/notepad"),"home/open-notepad.cfg"));
    assert(!strcmp(oxp_doc_mailbox("bin/paint"),"home/open-paint.cfg"));
    assert(oxp_doc_mailbox("bin/windowd")==NULL);
}
static void clipboard_test(void){
    oxp_file_clip_t c={0};char dst[96];
    oxp_clip_file_pack(&c,"docs/thing.txt",0);
    assert(oxp_clip_file_valid(&c,sizeof c) && !c.move);
    assert(!strcmp(c.path,"docs/thing.txt"));
    assert(!oxp_clip_file_valid(&c,sizeof c-1));
    c.marker[1]^=1;
    assert(!oxp_clip_file_valid(&c,sizeof c));c.marker[1]^=1;
    c.path[0]='.';c.path[1]='.';c.path[2]='/';
    /* Malicious clipboard paths never bypass the kernel's VFS safe-path
     * guard, and never allow moves out of the read-only source namespace. */
    oxp_clip_file_pack(&c,"home/document.txt",1);
    assert(oxp_clip_file_valid(&c,sizeof c)&&c.move);
    c.move=2;assert(!oxp_clip_file_valid(&c,sizeof c));
    assert(oxp_clip_destination(dst,"docs/thing.txt",0));
    assert(!strcmp(dst,"home/thing.txt"));
    assert(oxp_clip_destination(dst,"docs/thing.txt",1));
    assert(!strcmp(dst,"home/thing.copy.txt"));
    assert(oxp_clip_destination(dst,"docs/thing.txt",12));
    assert(!strcmp(dst,"home/thing.copy12.txt"));
    assert(!oxp_clip_destination(dst,"docs/",0));
    assert(!oxp_clip_destination(dst,"home/hello\\bad",0));
    char max[96]="home/";
    memset(max+5,'a',41);max[46]=0;
    assert(!oxp_clip_destination(dst,max,0));
}
static void click_test(void){
    oxp_double_click_t s={0};
    assert(!oxp_double_click(&s,"home/test.txt",100));
    assert(oxp_double_click(&s,"home/test.txt",130));
    assert(!oxp_double_click(&s,"home/test.txt",135));
    assert(!oxp_double_click(&s,"home/other.txt",136));
    assert(!oxp_double_click(&s,"home/test.txt",137));
    assert(!oxp_double_click(&s,"home/test.txt",180));
    assert(oxp_double_click(&s,"home/test.txt",181));
    s.armed=0;
    assert(!oxp_double_click(&s,"home/test.txt",UINT64_MAX-10));
    assert(oxp_double_click(&s,"home/test.txt",2));
}
static void picker_test(void){
    oxp_picker_t p={0};
    assert(oxp_picker_valid_name("notes.txt"));
    assert(!oxp_picker_valid_name("my document.md"));
    assert(!oxp_picker_valid_name(""));
    assert(!oxp_picker_valid_name("."));
    assert(!oxp_picker_valid_name(".."));
    assert(!oxp_picker_valid_name("foo/../../bar"));
    assert(!oxp_picker_valid_name("foo\\bad"));
    assert(!oxp_picker_valid_name("bad:name.txt"));
    assert(!oxp_picker_valid_name("bad?name.txt"));
    char too_long[55];memset(too_long,'a',53);too_long[53]=0;
    assert(!oxp_picker_valid_name(too_long));
    oxp_picker_begin(&p,1,"home/example.txt");
    assert(p.active&&p.save&&!strcmp(p.filename,"example.txt"));
    assert(oxp_picker_event(&p,OXP_KEY_SELECT_ALL,OXP_FT_TEXT)==0);
    assert(!p.filename[0]);
    assert(oxp_picker_event(&p,'a',OXP_FT_TEXT)==0);
    assert(!strcmp(p.filename,"a"));
    assert(oxp_picker_event(&p,8,OXP_FT_TEXT)==0);
    assert(!p.filename[0]);
    assert(oxp_picker_event(&p,27,OXP_FT_TEXT)==-1&&!p.active);
    oxp_picker_begin(&p,0,"home/x.oxp");
    assert(!p.save&&p.active&&!strcmp(p.filename,"x.oxp"));
}
int main(void){
    association_test();clipboard_test();click_test();picker_test();
    puts("PASS: Aurora 14 association, file clipboard, double click, and file picker regression tests");
}
#elif defined(OPENXP_TEST_AURORA15_WORKFLOW)
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../apps/native/file_picker.h"
#include "../apps/native/file_drop.h"
/* Pure host contract: drag source/target geometry, path routing, picker edit
 * focus, and per-launch workflow without a privileged graphics environment. */
int main(void){
    assert(oxp_drop_source_row(120,49,91,320,180,0)==0);
    assert(oxp_drop_source_row(120,64,91,320,180,2)==3);
    assert(oxp_drop_source_row(20,64,91,320,180,0)==-1);
    assert(oxp_drop_source_row(315,64,91,320,180,0)==-1);
    assert(oxp_drop_documents_target(20,70,91,180));
    assert(!oxp_drop_documents_target(100,70,91,180));
    assert(!oxp_drop_documents_target(20,100,91,180));
    assert(!oxp_drop_documents_target(20,70,91,90));
    assert(oxp_drop_should_copy("docs/readme.txt"));
    assert(!oxp_drop_should_copy("home/note.txt"));
    oxp_picker_t p={0};
    oxp_picker_begin(&p,0,"home/readme.txt");
    assert(p.active && !p.filename_focus);
    assert(!oxp_picker_event(&p,9,OXP_FT_TEXT));
    assert(p.filename_focus);
    assert(!oxp_picker_event(&p,OXP_KEY_SELECT_ALL,OXP_FT_TEXT));
    assert(!p.filename[0]);
    const char *name="new.txt";
    for(const char *c=name;*c;++c)assert(!oxp_picker_event(&p,(uint32_t)*c,OXP_FT_TEXT));
    assert(!strcmp(p.filename,"new.txt"));
    assert(!oxp_picker_event(&p,OXP_SHIFT_TAB,OXP_FT_TEXT));
    assert(!p.filename_focus);
    assert(!oxp_picker_event(&p,'X',OXP_FT_TEXT));
    assert(!strcmp(p.filename,"new.txt"));
    puts("PASS: Aurora 15 drag-to-Documents, bounds, shared picker name editing");
    return 0;
}
#elif defined(OPENXP_TEST_AURORA16_WORKFLOW)
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../apps/native/file_workflow.h"
#include "../apps/native/wm_protocol.h"
int main(void){
    char path[96]={0};
    assert(oxp_clip_destination_in(path,"docs/report.txt","home/projects",0));
    assert(!strcmp(path,"home/projects/report.txt"));
    assert(oxp_clip_destination_in(path,"home/report.txt","my documents/projects",1));
    assert(!strcmp(path,"my documents/projects/report.copy.txt"));
    assert(!oxp_clip_destination_in(path,"x.txt","home/a/deep/folder/with/a/very/very/long/name",0));
    assert(!oxp_clip_destination_in(path,"foo.txt","home/projects",1000000u));
    assert(!strcmp(oxp_drop_editor("home/notes.txt","Notepad - native ELF64"),"bin/notepad"));
    assert(!strcmp(oxp_drop_editor("home/notes.md","WordPad - native ELF64"),"bin/wordpad"));
    assert(!strcmp(oxp_drop_editor("home/img.oxp","Paint - native ELF64"),"bin/paint"));
    assert(!oxp_drop_editor("home/img.oxp","Notepad - native ELF64"));
    assert(!oxp_drop_editor("bin/notepad","Notepad - native ELF64"));
    assert(!oxp_drop_editor(NULL,"Notepad - native ELF64"));
    assert(!oxp_drop_editor("home/notes.txt","Terminal - native ELF64"));
    assert(sizeof(wm_drag_packet_t)==64);
    assert(sizeof(wm_packet_t)==60);
    puts("PASS: Aurora 16 document drop routing, folder copy names and IPC payload bounds");
}
#elif defined(OPENXP_TEST_ADVANCED_INPUT)
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../apps/native/advanced_input.h"
#include "../apps/native/paint_codec.h"
#include "../apps/native/text_edit.h"
#include "../apps/native/text_layout.h"
#include "../apps/native/scroll_layout.h"
static uint32_t event(unsigned type,unsigned x,unsigned y){
    return type|(((x+16u)&1023u)<<10)|((y+43u)&1023u);
}
static uint8_t canvas[OXP_PAINT_SIZE];
static unsigned calls;
static void plot(unsigned x,unsigned y,void *unused){
    (void)unused;assert(x<OXP_PAINT_W&&y<OXP_PAINT_H);
    oxp_paint_put(canvas,x,y,5u);++calls;
}
int main(void){
    oxp_gesture_t g={0};
    uint32_t e=event(OXP_GESTURE_DOWN,30u,40u);
    assert(oxp_gesture_feed(&g,e)==OXP_GESTURE_DOWN);
    assert(g.active&&g.start.valid&&g.start.x==30u&&g.start.y==40u);
    assert(oxp_gesture_feed(&g,event(OXP_GESTURE_MOVE,80u,80u))==OXP_GESTURE_MOVE);
    assert(oxp_gesture_feed(&g,OXP_GESTURE_LEAVE)==0 && g.active);
    assert(oxp_gesture_feed(&g,OXP_GESTURE_UP|1023u|(1023u<<10))==OXP_GESTURE_UP);
    assert(!g.active && !oxp_gesture_point(OXP_GESTURE_UP|1023u|(1023u<<10)).valid);
    oxp_gesture_point_t a={30,40,1},b={50,40,1};
    g.start=a;assert(!oxp_gesture_click(&g,b,20u,30u,20u,20u));
    assert(oxp_gesture_click(&g,(oxp_gesture_point_t){35,45,1},20u,30u,20u,20u));
    assert(!oxp_gesture_click(&g,(oxp_gesture_point_t){35,45,0},20u,30u,20u,20u));
    /* Large pointer jumps leave a gap with discrete clicks; the new stroke
     * must connect the whole segment, including reversed and diagonal ones. */
    oxp_paint_stroke(0,0,63,39,plot,0);
    assert(oxp_paint_get(canvas,0,0)==5u&&oxp_paint_get(canvas,63,39)==5u);
    assert(calls>=64u);
    calls=0;oxp_paint_stroke(63,39,0,39,plot,0);
    assert(calls==64u);
    calls=0;oxp_paint_stroke(0,0,64,0,plot,0);assert(!calls);
    /* A selection replaces exactly [min(anchor,cursor),max(...)); caret and
     * NUL terminator must stay coherent after deletion/insertion. */
    char doc[32]="abcdef";size_t len=6u,cur=5u;
    assert(oxp_erase_span(doc,&len,&cur,2u));
    assert(len==3u&&cur==2u&&!strcmp(doc,"abf"));
    assert(oxp_insert(doc,&len,&cur,sizeof doc,'X'));
    assert(len==4u&&cur==3u&&!strcmp(doc,"abXf"));
    assert(oxp_text_offset_at("abc\ndef",7u,3u,2u,2u)==6u);
    assert(!oxp_erase_span(doc,&len,&cur,cur));
    assert(!oxp_erase_span(doc,&len,&cur,20u));
    assert(oxp_gesture_scroll_at(100u,50u,100u,20u,10u,30u)==15u);
    assert(oxp_gesture_scroll_at(0u,50u,100u,20u,10u,30u)==0u);
    assert(oxp_gesture_scroll_at(10000u,50u,100u,20u,10u,30u)==30u);
    assert(oxp_gesture_scroll_at(10000u,50u,10u,10u,0u,30u)==0u);
    puts("PASS: Aurora 12 captured pointer, brush strokes, selection, and scroll math");
    return 0;
}
#elif defined(OPENXP_TEST_NATIVE_POINTER)
#include "../apps/native/oxp.h"
#include "../apps/native/ui_pointer.h"
#include <assert.h>
#include <stdio.h>
#define EVT(kind,x,y) ((uint32_t)(kind)|(((uint32_t)(x)&1023u)<<10)|((uint32_t)(y)&1023u))
int main(void){
    oxp_pointer_model_t s;
    oxp_pointer_reset(&s);
    assert(s.hover==-1 && s.armed==-1 && s.focus==-1);
    assert(oxp_pointer_add(&s,(oxp_pointer_rect_t){10,12,70,20})==0);
    assert(oxp_pointer_add(&s,(oxp_pointer_rect_t){90,12,50,20})==1);
    assert(oxp_pointer_at(&s,26,55)==0); /* left/top window frame */
    assert(oxp_pointer_at(&s,106,55)==1);
    assert(oxp_pointer_at(&s,25,55)==-1);
    assert(oxp_pointer_at(&s,1023,1023)==-1);
    int changed=0;
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_MOVE,26,55),&changed)==-2);
    assert(changed && s.hover==0 && oxp_pointer_style(&s,0)==1);
    changed=0;
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_DOWN,30,57),&changed)==-2);
    assert(s.down && s.armed==0 && s.focus==0 && oxp_pointer_style(&s,0)==2);
    changed=0;
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_MOVE,106,55),&changed)==-2);
    assert(changed && s.hover==1 && oxp_pointer_style(&s,0)==1);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_UP,106,55),&changed)==-2);
    assert(!s.down && s.armed==-1); /* release over another control cancels */
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_UP,30,57),&changed)==-2);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_DOWN,30,57),&changed)==-2);
    assert(oxp_pointer_feed(&s,OXP_POINTER_LEAVE,&changed)==-2);
    assert(s.hover==-1);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_UP,1023,1023),&changed)==-2);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_DOWN,30,57),&changed)==-2);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_UP,31,58),&changed)==0);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_DOWN,250,140),&changed)==-2);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_UP,250,140),&changed)==-1);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_DOWN,250,140),&changed)==-2);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_UP,280,140),&changed)==-2);
    puts("PASS: native control hover, armed states, release, cancel, leave, generic content and coordinates");
    return 0;
}
#elif defined(OPENXP_TEST_NATIVE_POINTER_KERNEL)
#include "gui/native.h"
#include <assert.h>
#include <stdio.h>
#define EVT(kind,x,y) ((uint32_t)(kind)|(((uint32_t)(x)&1023u)<<10)|((uint32_t)(y)&1023u))
int main(void){
    native_reset();
    assert(!native_pointer_enable(777));
    assert(!native_pointer_enabled_slot(0));
    assert(native_open(123,"Mouse",5));
    assert(!native_pointer_enabled_slot(0)); /* legacy apps remain untouched */
    assert(native_pointer_enable(123));
    assert(native_pointer_enabled_slot(0));
    native_push_event_slot(0,EVT(NATIVE_EVENT_POINTER_DOWN,24,54));
    for(unsigned i=0;i<120u;++i)
        native_push_event_slot(0,EVT(NATIVE_EVENT_POINTER_MOVE,24+i,54));
    native_push_event_slot(0,'Q');
    native_push_event_slot(0,EVT(NATIVE_EVENT_POINTER_UP,143,54));
    assert(native_next_event(123)==EVT(NATIVE_EVENT_POINTER_DOWN,24,54));
    assert(native_next_event(123)==EVT(NATIVE_EVENT_POINTER_MOVE,143,54));
    assert(native_next_event(123)=='Q');
    assert(native_next_event(123)==EVT(NATIVE_EVENT_POINTER_UP,143,54));
    assert(!native_next_event(123));
    for(unsigned i=0;i<NATIVE_EVENT_CAP-1u;++i)native_push_event_slot(0,'a'+i);
    native_push_event_slot(0,EVT(NATIVE_EVENT_POINTER_MOVE,1,1)); /* drop if full */
    native_push_event_slot(0,EVT(NATIVE_EVENT_POINTER_UP,2,2)); /* must survive */
    unsigned saw_up=0;
    for(uint32_t e;(e=native_next_event(123))!=0;){
        if((e&NATIVE_EVENT_POINTER_MASK)==NATIVE_EVENT_POINTER_UP)saw_up++;
        assert((e&NATIVE_EVENT_POINTER_MASK)!=NATIVE_EVENT_POINTER_MOVE);
    }
    assert(saw_up==1);
    native_owner_exit(123);
    assert(native_open(456,"Fresh",5));
    assert(!native_pointer_enabled_slot(0)); /* don't leak subscription across PID reuse */
    puts("PASS: per-owner subscription, motion coalescing, control priority, reset on close");
    return 0;
}
#elif defined(OPENXP_TEST_NATIVE_TEXT_EDIT)
#include "../apps/native/text_edit.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    char t[5]="abc";size_t len=3,cursor=1;
    assert(oxp_insert(t,&len,&cursor,sizeof t,'X'));
    assert(!strcmp(t,"aXbc")&&len==4&&cursor==2);
    assert(!oxp_insert(t,&len,&cursor,sizeof t,'Y'));
    assert(oxp_backspace(t,&len,&cursor));
    assert(!strcmp(t,"abc")&&cursor==1);
    assert(oxp_delete(t,&len,&cursor));
    assert(!strcmp(t,"ac")&&len==2);
    cursor=0;assert(!oxp_backspace(t,&len,&cursor));
    cursor=len;assert(!oxp_delete(t,&len,&cursor));
    assert(!oxp_insert(t,&len,&cursor,0,'X'));
    puts("PASS: native text insertion, removal, cursor and capacity");
    return 0;
}
#elif defined(OPENXP_TEST_NATIVE_PAINT_CODEC)
#include "../apps/native/paint_codec.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    uint8_t p[OXP_PAINT_SIZE]={0};
    for(unsigned y=0;y<OXP_PAINT_H;++y)for(unsigned x=0;x<OXP_PAINT_W;++x)
        oxp_paint_put(p,x,y,(x+y)%8u);
    for(unsigned y=0;y<OXP_PAINT_H;++y)for(unsigned x=0;x<OXP_PAINT_W;++x)
        assert(oxp_paint_get(p,x,y)==(x+y)%8u);
    uint8_t first=p[0],last=p[OXP_PAINT_SIZE-1];
    oxp_paint_put(p,OXP_PAINT_W,0,7);
    oxp_paint_put(p,0,OXP_PAINT_H,7);
    oxp_paint_put(p,0,0,8);
    assert(p[0]==first&&p[OXP_PAINT_SIZE-1]==last);
    for(unsigned c=0;c<8;++c){oxp_paint_put(p,0,0,c);assert(oxp_paint_get(p,0,0)==c);}
    puts("PASS: 3-bit packed native Paint data, byte boundaries and bounds");
    return 0;
}
#else
#error "Compile this suite with an OPENXP_TEST_* selector"
#endif
