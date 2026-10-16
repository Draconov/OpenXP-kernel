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
