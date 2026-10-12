#include "sync/event.h"
#include "arch/x86_64/irq_state.h"
#include <stddef.h>
void event_init(event_t *e, bool initially_signaled) {
    if (!e) return;
    *e = (event_t){ .signaled = initially_signaled };
}
void event_set(event_t *e) {
    if (!e) return;
    uint64_t flags = irq_save_disable();
    ++e->generation;
    e->signaled = true;
    (void)wait_queue_wake_all_locked(&e->waiters);
    irq_restore(flags);
}
void event_reset(event_t *e) {
    if (!e) return;
    uint64_t flags = irq_save_disable();
    e->signaled = false;
    irq_restore(flags);
}
bool event_is_set(event_t *e) {
    if (!e) return false;
    uint64_t flags = irq_save_disable();
    bool state = e->signaled;
    irq_restore(flags);
    return state;
}
bool event_wait(event_t *e) {
    if (!e || !irq_are_enabled()) return false;
    uint64_t flags = irq_save_disable();
    uint64_t seen = e->generation;
    while (!e->signaled && e->generation == seen) {
        if (!wait_queue_sleep_locked(&e->waiters)) {
            irq_restore(flags);
            return false;
        }
    }
    irq_restore(flags);
    return true;
}
