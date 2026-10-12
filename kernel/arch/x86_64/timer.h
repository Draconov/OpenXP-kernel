#ifndef OPENXP_X86_64_TIMER_H
#define OPENXP_X86_64_TIMER_H
#include <stdbool.h>
#include <stdint.h>

/* v0.6 uses the 8259A PIC + PIT channel 0 on a single x86 QEMU CPU.
 * This is a legacy-PC backend, NOT an IOAPIC/LAPIC/HPET driver. */
#define TIMER_VECTOR 32u
#define TIMER_TARGET_HZ 100u
#define PIT_INPUT_HZ 1193182u

/* Pure helpers tested on the build host; returned ticks are an approximation. */
uint16_t timer_divisor_for_hz(unsigned hz);
unsigned timer_actual_hz(uint16_t divisor);
/* Exact rational PIT conversion, rounded down; saturates on overflow. */
uint64_t timer_ticks_to_milliseconds(uint64_t ticks, uint16_t divisor);
uint64_t timer_monotonic_ms(void);

/* All functions below run at CPL0. timer_init keeps the CPU's IF unchanged.
 * Call timer_enable only after the IDT and scheduler have been initialized. */
bool timer_init(unsigned requested_hz);
void timer_enable(void);
void timer_disable(void);
uint64_t timer_ticks(void);
/* Called only by the IRQ0 assembly entry with IF=0; sends EOI before exit. */
uint64_t timer_irq_dispatch(uint64_t interrupted_rsp);
#endif
