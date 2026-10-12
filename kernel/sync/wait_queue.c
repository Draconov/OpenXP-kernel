#include "sync/wait_queue.h"
#include "arch/x86_64/irq_state.h"
#include <stddef.h>

void wait_queue_init(wait_queue_t *q) {
    if (!q) return;
    *q = (wait_queue_t){0};
}
unsigned wait_queue_count(const wait_queue_t *q) {
    return (q && !irq_are_enabled()) ? q->count : 0;
}
static void remove_at(wait_queue_t *q, unsigned at) {
    for (unsigned i = at + 1; i < q->count; ++i) q->ids[i - 1] = q->ids[i];
    if (q->count) q->ids[--q->count] = 0;
}
static void remove_id(wait_queue_t *q, uint64_t id) {
    for (unsigned i = 0; i < q->count; ++i)
        if (q->ids[i] == id) { remove_at(q, i); return; }
}
bool wait_queue_sleep_locked(wait_queue_t *q) {
    if (!q || irq_are_enabled() || q->count >= WAIT_QUEUE_MAX) return false;
    const uint64_t id = scheduler_current_id();
    if (!id) return false;
    for (unsigned i = 0; i < q->count; ++i)
        if (q->ids[i] == id) return false;
    q->ids[q->count++] = id;
    /* No IRQ can run between recording the waiter and parking it. */
    bool ok = scheduler_park_current_locked();
    /* Other sched_wake callers are allowed: don't leave a stale waiter. */
    remove_id(q, id);
    return ok;
}
unsigned wait_queue_wake_one_locked(wait_queue_t *q) {
    if (!q || irq_are_enabled()) return 0;
    while (q->count) {
        uint64_t id = q->ids[0];
        remove_at(q, 0);
        if (scheduler_wake(id)) return 1;
    }
    return 0;
}
unsigned wait_queue_wake_all_locked(wait_queue_t *q) {
    if (!q || irq_are_enabled()) return 0;
    unsigned count = 0;
    while (q->count) count += wait_queue_wake_one_locked(q);
    return count;
}
