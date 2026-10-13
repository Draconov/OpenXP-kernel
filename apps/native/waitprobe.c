#include "oxp.h"
/* Must be launched with timer IRQ0 enabled in a preemptible Ring 3 worker.
 * A successful return without >=3 ticks elapsed would conceal a broken wait. */
void oxp_main(void) {
    static const char title[] = "GUI wait probe";
    if (oxp_open(title)) oxp_exit(1);
    if (oxp_event()) oxp_exit(2);
    uint64_t start = oxp_ticks();
    if (oxp_wait(3)) oxp_exit(3);  /* no user input in headless smoke */
    if (oxp_ticks() - start < 3) oxp_exit(4);
    oxp_exit(0);
}
