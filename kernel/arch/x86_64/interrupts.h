#ifndef OPENXP_X86_64_INTERRUPTS_H
#define OPENXP_X86_64_INTERRUPTS_H
#include <stdint.h>
/* Installs all 256 IDT entries and handles CPU exceptions 0..31.
 * IRQ0 is installed but remains masked until timer_enable(). */
void interrupts_init(void);

/* Layout matches registers pushed by the common assembly entry stub. */
struct exception_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector, error, rip, cs, rflags;
};
__attribute__((noreturn)) void exception_dispatch(const struct exception_frame *frame);
#endif
