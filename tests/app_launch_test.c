#include "user/app_launch.h"
#include "arch/x86_64/irq_state.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
    char out[48];app_launch_reset();
    assert(!app_launch_allowed("bin/windowd"));
    assert(!app_launch_allowed("bin/fault"));
    assert(!app_launch_allowed("bin/../paint"));
    assert(!app_launch_submit("bin/evil"));
    assert(!app_launch_take(out,sizeof out));
    assert(!app_launch_take(out,3));
    assert(app_launch_submit("bin/paint"));
    assert(!app_launch_submit("bin/wordpad"));
    assert(app_launch_take(out,sizeof out));
    assert(!strcmp(out,"bin/paint"));
    assert(!app_launch_take(out,sizeof out));
    assert(app_launch_allowed("system apps/notepad.exe"));
    assert(app_launch_allowed("windows/system32/notepad.exe"));
    assert(!app_launch_allowed("windows/system32/evil.exe"));
    assert(app_launch_submit("system apps/notepad.exe"));
    assert(app_launch_take(out,sizeof out));
    assert(!strcmp(out,"bin/notepad"));
    assert(app_launch_submit("bin/wordpad"));
    app_launch_reset();assert(!app_launch_take(out,sizeof out));
    assert(app_launch_submit("bin/filemanager"));
    assert(app_launch_take(out,sizeof out));
    assert(!strcmp(out,"bin/filemanager"));
    app_launch_request_t queued={0};
    assert(app_launch_submit_with_arg("bin/notepad","home/example.txt"));
    assert(!app_launch_submit_with_arg("bin/paint","home/drawing.oxp"));
    assert(app_launch_take_request(&queued));
    assert(!strcmp(queued.executable,"bin/notepad"));
    assert(!strcmp(queued.argument,"home/example.txt"));
    assert(!app_launch_take_request(&queued));
    assert(!app_launch_submit_with_arg("bin/notepad","home/../secret.txt"));
    assert(!app_launch_submit_with_arg("bin/notepad","/etc/passwd"));
    assert(!app_launch_submit_with_arg("bin/notepad","home/"));
    assert(!app_launch_submit_with_arg("bin/notepad","home/foo\\bar"));
    assert(!app_launch_submit_with_arg("bin/windowd","home/example.txt"));
    assert(app_launch_submit("bin/paint"));
    assert(app_launch_take_request(&queued));
    assert(!queued.argument[0]);
    puts("PASS: isolated app launch allowlist, queue, consumption and reset");
    return 0;
}
