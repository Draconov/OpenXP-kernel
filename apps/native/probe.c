#include "oxp.h"
void oxp_main(void){
    static const char title[]="Native ABI selftest";
    static const char content[]="Ring 3 native GUI and RAM storage";
    char readback[80]={0};
    if(oxp_open(title) || oxp_text(content))oxp_exit(1);
    if(oxp_event())oxp_exit(2);
    /* Exercise the actual CPL3->CPL0 atomic timed wait in smoke-native.
     * No user input is delivered in this headless test. */
    if(oxp_wait(3))oxp_exit(6);
    if(oxp_write(content,sizeof(content)-1)!=sizeof(content)-1)oxp_exit(3);
    if(oxp_read(readback,sizeof(readback))!=sizeof(content)-1)oxp_exit(4);
    for(unsigned i=0;i<sizeof(content)-1;++i)if(readback[i]!=content[i])oxp_exit(5);
    oxp_exit(0);
}
