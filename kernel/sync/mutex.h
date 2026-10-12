#ifndef OPENXP_MUTEX_H
#define OPENXP_MUTEX_H
#include "sync/wait_queue.h"
/* Non-recursive, sleep-capable, unfair kernel mutex for the single CPU.
 * NO priority inheritance, owner death, timeouts, SMP or IRQ-safe blocking. */
typedef struct {
    uint64_t owner;
    wait_queue_t waiters;
} mutex_t;
void mutex_init(mutex_t *m);
bool mutex_try_lock(mutex_t *m);
bool mutex_lock(mutex_t *m); /* IF=1 required; may sleep. */
bool mutex_unlock(mutex_t *m);
#endif
