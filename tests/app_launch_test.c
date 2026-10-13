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
    assert(app_launch_submit("bin/wordpad"));
    app_launch_reset();assert(!app_launch_take(out,sizeof out));
    assert(app_launch_submit("bin/filemanager"));
    assert(app_launch_take(out,sizeof out));
    assert(!strcmp(out,"bin/filemanager"));
    puts("PASS: isolated app launch allowlist, queue, consumption and reset");
    return 0;
}
