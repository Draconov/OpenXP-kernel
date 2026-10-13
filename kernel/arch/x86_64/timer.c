#include "arch/x86_64/timer.h"
#include "sched/scheduler.h"
#include "input/input.h"

/* The real PIC/PIT functions are purposely absent from host unit tests. */
uint16_t timer_divisor_for_hz(unsigned hz) {
    if (hz == 0) return 0;
    uint64_t d = ((uint64_t)PIT_INPUT_HZ + hz / 2u) / hz;
    if (d < 1u || d > UINT16_MAX) return 0;
    return (uint16_t)d;
}
unsigned timer_actual_hz(uint16_t divisor) {
    return divisor ? (PIT_INPUT_HZ + divisor / 2u) / divisor : 0;
}
uint64_t timer_ticks_to_milliseconds(uint64_t ticks, uint16_t divisor) {
    if (!divisor) return 0;
    /* Split the arithmetic to avoid 64-bit overflow on long uptimes. */
    const uint64_t scale = (uint64_t)divisor * 1000u;
    const uint64_t whole = ticks / PIT_INPUT_HZ;
    const uint64_t rem = ticks % PIT_INPUT_HZ;
    const uint64_t tail = (rem * scale) / PIT_INPUT_HZ;
    if (whole > (UINT64_MAX - tail) / scale) return UINT64_MAX;
    return whole * scale + tail;
}


#ifndef OPENXP_TIMER_HOST_TEST
static volatile uint64_t ticks;
static bool configured;
static uint16_t active_divisor;

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline void io_wait(void) {
    outb(0x80, 0);
}
static inline bool interrupts_enabled(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0" : "=r"(flags));
    return (flags & (1ULL << 9)) != 0;
}
/* This initialization deliberately does not restore IF or allow nested IRQs.
 * Only IRQ0 is ever unmasked; all other PIC interrupts remain masked. */
bool timer_init(unsigned requested_hz) {
    if (configured || interrupts_enabled()) return false;
    uint16_t divisor = timer_divisor_for_hz(requested_hz);
    if (!divisor) return false;
    outb(0x21, 0xff); outb(0xa1, 0xff);
    /* ICW1, ICW2, ICW3, ICW4: PIC offsets 0x20/0x28, standard cascade. */
    outb(0x20, 0x11); io_wait();
    outb(0xa0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();
    outb(0xa1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait();
    outb(0xa1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xa1, 0x01); io_wait();
    outb(0x21, 0xff);
    outb(0xa1, 0xff);
    /* PIT channel 0, lobyte/hibyte, mode 3 (square wave). */
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)divisor);
    outb(0x40, (uint8_t)(divisor >> 8));
    ticks = 0;
    active_divisor = divisor;
    configured = true;
    return true;
}
void timer_enable(void) {
    if (!configured) return;
    __asm__ volatile ("cli" ::: "memory");
    if(input_irq_ready()) {
        outb(0xa1,0xef); /* slave IRQ12 mouse */
        outb(0x21,0xf8); /* IRQ0, IRQ1 keyboard, IRQ2 cascade */
    } else outb(0x21,0xfe); /* Headless diagnostic boot: timer only */
    __asm__ volatile ("sti" ::: "memory");
}
void timer_disable(void) {
    __asm__ volatile ("cli" ::: "memory");
    outb(0x21, 0xff);
    outb(0xa1, 0xff);
}
uint64_t timer_ticks(void) { return ticks; }
uint64_t timer_monotonic_ms(void) {
    return timer_ticks_to_milliseconds(timer_ticks(), active_divisor);
}
uint64_t timer_irq_dispatch(uint64_t interrupted_rsp) {
    ++ticks;
    /* Acknowledge the master 8259 before enabling IF on a different thread. */
    outb(0x20, 0x20);
    return scheduler_on_timer_interrupt(interrupted_rsp, ticks);
}
#endif
