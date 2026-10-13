#ifndef OPENXP_SCHEDULER_H
#define OPENXP_SCHEDULER_H
#include <stdbool.h>
#include <stdint.h>

/* One-CPU kernel threads; experimental IRQ0 switching also supports CPL3.
 * Cooperative mode is the default (IF=0).
 * Experimental IRQ-preemption mode uses the 8259/PIT in QEMU only;
 * Ring 3 threads may bind a private CR3 and RSP0 before returning to user mode.
 * No SMP, user mode, FPU/SIMD or scheduler locks are supported. */
#define SCHED_MAX_WORKERS 6u
#define SCHED_STACK_PAGES 4u

typedef void (*thread_entry_fn)(void *);

bool scheduler_init(void);
/* Returns unique thread identifier, or 0 when no slot/memory is available. */
uint64_t scheduler_create(thread_entry_fn entry, void *arg);
/* Never switches if there is no other runnable thread. */
void scheduler_yield(void); /* Cooperative mode only; never from an IRQ. */
/* One-way changeover, only from boot thread with IF=0 and no workers. */
bool scheduler_enable_preemption(void);
/* Called from the running worker with IF=0 (never in a timer IRQ).
 * The root must map all kernel code/data and stacks as supervisor-only. */
bool scheduler_bind_current_memory(uint64_t root, uint64_t rsp0);
bool scheduler_unbind_current_memory(void);
/* IRQ0 interrupt-return context selector. Must be called with IF=0. */
uint64_t scheduler_on_timer_interrupt(uint64_t interrupted_rsp, uint64_t tick);
/* Timer ticks are 64-bit monotonic, modulo wrap. No wall-clock semantics. */
uint64_t scheduler_last_tick(void);
/* Requires preemptive mode and enabled IRQs; only from ordinary thread code.
 * A sleeper parks with sti;hlt;cli until its deadline and then resumes. */
bool scheduler_sleep_ticks(uint64_t delay);
/* Atomically arm a deadline and park while IF=0. Returns with IF=0.
 * Used by event queues that must not race a condition-check and sleep.
 * Never call from an IRQ, from a second CPU, or without timer preemption. */
bool scheduler_sleep_current_locked(uint64_t delay);
/* Manual blocking requires a different thread to wake this thread by ID. */
bool scheduler_block_current(void);
/* Atomic park: caller holds local IRQs disabled. On return IF remains off.
 * Intended only for synchronization primitives; never call in an IRQ. */
bool scheduler_park_current_locked(void);
/* Waking requires IF=0 on this single CPU. */
bool scheduler_wake(uint64_t thread_id);
uint64_t scheduler_preempt_switch_count(void);
/* Counts runnable, sleeping, or blocked workers, not just runnable workers. */
unsigned scheduler_live_workers(void);
/* Releases finished workers' guarded stacks from the boot thread ONLY
 * while interrupts are disabled, including in preemptive mode. Never free
 * a thread's current stack or call this from a timer IRQ. */
unsigned scheduler_reap(void);
uint64_t scheduler_current_id(void);
uint64_t scheduler_switch_count(void);
#ifdef OPENXP_SCHED_HOST_TEST
bool scheduler_test_memory(uint64_t id, uint64_t *root, uint64_t *rsp0);
void scheduler_test_active_memory(uint64_t *root, uint64_t *rsp0);
/* Deterministic host simulation hook; never compiled into kernel. */
void scheduler_test_finish_current(void);
#endif
#endif
