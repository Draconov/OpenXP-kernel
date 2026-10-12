#include "sync/mutex.h"
#include "arch/x86_64/irq_state.h"
#include <stddef.h>
void mutex_init(mutex_t *m) { if (m) *m = (mutex_t){0}; }
bool mutex_try_lock(mutex_t *m) {
    if (!m) return false;
    uint64_t flags = irq_save_disable();
    uint64_t id = scheduler_current_id();
    bool acquired = id && !m->owner;
    if (acquired) m->owner = id;
    irq_restore(flags);
    return acquired;
}
bool mutex_lock(mutex_t *m) {
    if (!m || !irq_are_enabled()) return false;
    uint64_t flags = irq_save_disable();
    const uint64_t id = scheduler_current_id();
    if (!id || m->owner == id) { irq_restore(flags); return false; }
    while (m->owner) {
        if (!wait_queue_sleep_locked(&m->waiters)) {
            irq_restore(flags);
            return false;
        }
    }
    m->owner = id;
    irq_restore(flags);
    return true;
}
bool mutex_unlock(mutex_t *m) {
    if (!m) return false;
    uint64_t flags = irq_save_disable();
    const uint64_t id = scheduler_current_id();
    bool owned = id && m->owner == id;
    if (owned) {
        m->owner = 0;
        (void)wait_queue_wake_one_locked(&m->waiters);
    }
    irq_restore(flags);
    return owned;
}
