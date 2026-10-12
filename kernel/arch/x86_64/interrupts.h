#ifndef OPENXP_X86_64_INTERRUPTS_H
#define OPENXP_X86_64_INTERRUPTS_H
#include <stdint.h>
/* Installs all 256 IDT entries and handles CPU exceptions 0..31.
 * IRQ0 is installed but remains masked until timer_enable(). */
void interrupts_init(void);

/* The complete long-mode interrupt frame. In 64-bit mode hardware always
 * pushes RSP and SS, even for an interrupt that remains at CPL0. Keeping
 * these fields explicit prevents a synthetic first-run frame from omitting
 * the final two qwords that IRETQ unconditionally restores. */
struct exception_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector, error, rip, cs, rflags, rsp, ss;
};
__attribute__((noreturn)) void exception_dispatch(const struct exception_frame *frame);
#endif
