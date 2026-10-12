#ifndef OPENXP_EVENT_H
#define OPENXP_EVENT_H
#include "sync/wait_queue.h"
/* Manual-reset notification event. A set operation wakes all current waiters
 * and leaves the event signaled until reset. Generation handles set+reset
 * before a woken waiter gets CPU time. No timeouts or auto-reset yet. */
typedef struct {
    wait_queue_t waiters;
    uint64_t generation;
    bool signaled;
} event_t;
void event_init(event_t *e, bool initially_signaled);
void event_set(event_t *e);
void event_reset(event_t *e);
bool event_is_set(event_t *e);
/* Blocking wait requires interrupts enabled in normal kernel thread context. */
bool event_wait(event_t *e);
#endif
