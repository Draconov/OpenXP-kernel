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
