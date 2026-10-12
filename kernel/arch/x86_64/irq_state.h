#ifndef OPENXP_X86_64_IRQ_STATE_H
#define OPENXP_X86_64_IRQ_STATE_H
#include <stdbool.h>
#include <stdint.h>
/* Local-CPU interrupt critical sections. NOT a multi-core spinlock.
 * Keep sections short: never block, yield, or call HLT while IF=0.
 * Restore the token in reverse (LIFO) order, on the same CPU. */
#define X86_RFLAGS_IF (1ULL << 9)
uint64_t irq_save_disable(void);
void irq_restore(uint64_t saved_flags);
bool irq_are_enabled(void);
#ifdef OPENXP_IRQ_HOST_TEST
void irq_test_set_enabled(bool enabled);
#endif
#endif
