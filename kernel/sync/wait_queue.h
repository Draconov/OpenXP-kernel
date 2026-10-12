#ifndef OPENXP_WAIT_QUEUE_H
#define OPENXP_WAIT_QUEUE_H
#include <stdbool.h>
#include <stdint.h>
#include "sched/scheduler.h"

/* Bounded, nonallocating, single-CPU FIFO; caller manages object lifetime.
 * All *_locked operations REQUIRE local IRQs disabled. Blocking is only
 * legal in preemptive ring-0 thread context, never an interrupt handler. */
#define WAIT_QUEUE_MAX (SCHED_MAX_WORKERS + 1u)
typedef struct {
    uint64_t ids[WAIT_QUEUE_MAX];
    unsigned count;
} wait_queue_t;

void wait_queue_init(wait_queue_t *q);
unsigned wait_queue_count(const wait_queue_t *q); /* Requires IF=0. */
/* Atomically enqueue + park, with IF=0 on entry and on return. */
bool wait_queue_sleep_locked(wait_queue_t *q);
/* Returns number of live threads actually awakened. IF=0 required. */
unsigned wait_queue_wake_one_locked(wait_queue_t *q);
unsigned wait_queue_wake_all_locked(wait_queue_t *q);
#endif
