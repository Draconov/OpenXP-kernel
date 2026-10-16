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
