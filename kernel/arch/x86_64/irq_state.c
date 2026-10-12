#include "arch/x86_64/irq_state.h"
#ifdef OPENXP_IRQ_HOST_TEST
/* Simulates one CPU's IF flag; never executes privileged instructions. */
static bool test_enabled;
void irq_test_set_enabled(bool enabled) { test_enabled = enabled; }
bool irq_are_enabled(void) { return test_enabled; }
uint64_t irq_save_disable(void) {
    uint64_t flags = test_enabled ? X86_RFLAGS_IF : 0;
    test_enabled = false;
    return flags;
}
void irq_restore(uint64_t flags) { test_enabled = (flags & X86_RFLAGS_IF) != 0; }
#else
bool irq_are_enabled(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0" : "=r"(flags) : : "memory");
    return (flags & X86_RFLAGS_IF) != 0;
}
uint64_t irq_save_disable(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}
void irq_restore(uint64_t flags) {
    if (flags & X86_RFLAGS_IF)
        __asm__ volatile ("sti" ::: "memory");
    else
        __asm__ volatile ("cli" ::: "memory");
}
#endif
