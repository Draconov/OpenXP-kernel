#include "oxp.h"
void oxp_main(void){
    static const char title[]="Native ABI selftest";
    static const char content[]="Ring 3 native GUI and RAM storage";
    char readback[80]={0};
    if(oxp_open(title) || oxp_text(content))oxp_exit(1);
    if(oxp_event())oxp_exit(2);
    /* No blocking GUI_WAIT here: the ABI smoke test runs before the PIT
     * interrupts are enabled. The dedicated waitprobe tests blocking under
     * the IRQ-driven, preemptible scheduler. */
    if(oxp_write(content,sizeof(content)-1)!=sizeof(content)-1)oxp_exit(3);
    if(oxp_read(readback,sizeof(readback))!=sizeof(content)-1)oxp_exit(4);
    for(unsigned i=0;i<sizeof(content)-1;++i)if(readback[i]!=content[i])oxp_exit(5);
    oxp_exit(0);
}
