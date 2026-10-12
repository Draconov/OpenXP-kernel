#include "arch/x86_64/irq_state.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    irq_test_set_enabled(true);
    assert(irq_are_enabled());
    uint64_t outer = irq_save_disable();
    assert((outer & X86_RFLAGS_IF) && !irq_are_enabled());
    uint64_t inner = irq_save_disable();
    assert(!(inner & X86_RFLAGS_IF));
    irq_restore(inner);
    assert(!irq_are_enabled());
    irq_restore(outer);
    assert(irq_are_enabled());
    irq_test_set_enabled(false);
    irq_restore(irq_save_disable());
    assert(!irq_are_enabled());
    puts("PASS: single-CPU IF save/disable/restore and nested critical sections");
    return 0;
}
