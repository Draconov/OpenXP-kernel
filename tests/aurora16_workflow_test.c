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
