#include "../apps/native/shell_interaction.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    int held=-1;
    assert(oxp_shell_finish_press(&held,1)==-1 && held==-1);
    held=1;
    assert(oxp_shell_finish_press(&held,1)==1 && held==-1);
    held=1;
    assert(oxp_shell_finish_press(&held,400)==-1 && held==-1);
    held=302;
    assert(oxp_shell_finish_press(&held,301)==-1 && held==-1);
    held=302;
    assert(oxp_shell_finish_press(&held,302)==302 && held==-1);
    held=105;
    assert(oxp_shell_finish_press(&held,-1)==-1 && held==-1);
    assert(oxp_shell_finish_press(0,100)==-1);
    puts("PASS: press/release activation, drag cancellation, stale press reset");
    return 0;
}
