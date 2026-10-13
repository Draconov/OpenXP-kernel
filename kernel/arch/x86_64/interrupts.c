#include "interrupts.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/timer.h"
#include "mm/heap.h"
#include "serial.h"
#include "diag/crash.h"
#include "user/userspace.h"
#include <stddef.h>

struct idt_gate {
    uint16_t offset_low, selector;
    uint8_t ist, attributes;
    uint16_t offset_mid;
    uint32_t offset_high, reserved;
} __attribute__((packed));
struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));
_Static_assert(sizeof(struct idt_gate) == 16, "x86-64 IDT gate must be 16 bytes");
_Static_assert(sizeof(struct idt_pointer) == 10, "x86-64 IDTR must be 10 bytes");
_Static_assert(sizeof(struct exception_frame) == 176, "IRQ assembly frame mismatch");
_Static_assert(TIMER_VECTOR == 32, "irq.S vector must match PIC offset");
static struct idt_gate idt[256];

#define DECLARE_ISR(n) extern void isr_##n(void)
DECLARE_ISR(0); DECLARE_ISR(1); DECLARE_ISR(2); DECLARE_ISR(3);
DECLARE_ISR(4); DECLARE_ISR(5); DECLARE_ISR(6); DECLARE_ISR(7);
DECLARE_ISR(8); DECLARE_ISR(9); DECLARE_ISR(10); DECLARE_ISR(11);
DECLARE_ISR(12); DECLARE_ISR(13); DECLARE_ISR(14); DECLARE_ISR(15);
DECLARE_ISR(16); DECLARE_ISR(17); DECLARE_ISR(18); DECLARE_ISR(19);
DECLARE_ISR(20); DECLARE_ISR(21); DECLARE_ISR(22); DECLARE_ISR(23);
DECLARE_ISR(24); DECLARE_ISR(25); DECLARE_ISR(26); DECLARE_ISR(27);
DECLARE_ISR(28); DECLARE_ISR(29); DECLARE_ISR(30); DECLARE_ISR(31);
extern void isr_unexpected(void);
extern void irq_timer_entry(void);
extern void irq_keyboard_entry(void);
extern void irq_mouse_entry(void);
extern void irq_syscall_entry(void);

static void idt_set(unsigned vector, void (*handler)(void), uint16_t cs, uint8_t ist) {
    const uint64_t addr = (uint64_t)(uintptr_t)handler;
    idt[vector] = (struct idt_gate){
        .offset_low = (uint16_t)addr, .selector = cs, .ist = ist,
        .attributes = 0x8e, /* present, ring 0, interrupt gate */
        .offset_mid = (uint16_t)(addr >> 16),
        .offset_high = (uint32_t)(addr >> 32), .reserved = 0
    };
}
void interrupts_init(void) {
    __asm__ volatile ("cli" ::: "memory");
    const uint16_t cs = GDT_KERNEL_CS;
    for (unsigned i = 0; i < 256; ++i) idt_set(i, isr_unexpected, cs, 0);
    void (*const exceptions[32])(void) = {
        isr_0, isr_1, isr_2, isr_3, isr_4, isr_5, isr_6, isr_7,
        isr_8, isr_9, isr_10, isr_11, isr_12, isr_13, isr_14, isr_15,
        isr_16, isr_17, isr_18, isr_19, isr_20, isr_21, isr_22, isr_23,
        isr_24, isr_25, isr_26, isr_27, isr_28, isr_29, isr_30, isr_31
    };
    for (unsigned i = 0; i < 32; ++i) {
        uint8_t ist = i == 8 ? 1 : (i == 2 ? 2 : (i == 18 ? 3 : 0));
        idt_set(i, exceptions[i], cs, ist);
    }
    idt_set(TIMER_VECTOR, irq_timer_entry, cs, 0);
    idt_set(33, irq_keyboard_entry, cs, 0); /* PIC IRQ1 */
    idt_set(44, irq_mouse_entry, cs, 0);    /* PIC IRQ12 */
    idt_set(0x80, irq_syscall_entry, cs, 0);
    idt[0x80].attributes = 0xee; /* Present 64-bit interrupt gate, DPL3 */
    const struct idt_pointer ptr = { .limit = sizeof(idt) - 1, .base = (uint64_t)(uintptr_t)idt };
    __asm__ volatile ("lidt %0" : : "m"(ptr) : "memory");
    serial_puts("IDT installed (256 gates, IRQ0 ready, interrupts disabled)\nOPENXP_IDT_OK\n");
}

__attribute__((noreturn)) void exception_dispatch(const struct exception_frame *frame) {
    uint64_t cr2 = 0;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    /* Faults from CPL3 are confined to the one user task; supervisor faults
     * remain fatal and continue through the full kernel crash report. */
    if ((frame->cs & 3u) == 3u && userspace_active())
        userspace_fault(frame->vector, cr2);
    crash_report_exception(frame, cr2);
#ifdef OPENXP_EXCEPTION_SELFTEST
    if (frame->vector == 6) serial_puts("OPENXP_EXCEPTION_OK\n");
#endif
#ifdef OPENXP_GUARD_SELFTEST
    if (frame->vector == 14 && cr2 == heap_guard_low())
        serial_puts("OPENXP_GUARD_OK\n");
#endif
    kernel_halt();
}
