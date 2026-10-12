/* POSIX-threaded host harness: simulates one IRQ-masked CPU's atomic
 * condition checks and sleeping. Tests the real kernel sync/IPC C sources. */
#include "sync/wait_queue.h"
#include "sync/event.h"
#include "sync/mutex.h"
#include "sync/ipc.h"
#include "arch/x86_64/irq_state.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdint.h>

#define TEST_THREADS 7
static pthread_mutex_t kernel_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sleeping[TEST_THREADS + 1];
static bool blocked[TEST_THREADS + 1];
static bool woken[TEST_THREADS + 1];
static _Thread_local uint64_t current_id = 1;
static _Thread_local bool if_enabled = true;
static void as_thread(uint64_t id) { assert(id && id <= TEST_THREADS); current_id = id; if_enabled = true; }
bool irq_are_enabled(void) { return if_enabled; }
uint64_t irq_save_disable(void) {
    uint64_t old = if_enabled ? X86_RFLAGS_IF : 0;
    if (if_enabled) { pthread_mutex_lock(&kernel_lock); if_enabled = false; }
    return old;
}
void irq_restore(uint64_t flags) {
    if (flags & X86_RFLAGS_IF) { assert(!if_enabled); if_enabled = true; pthread_mutex_unlock(&kernel_lock); }
    else assert(!if_enabled);
}
uint64_t scheduler_current_id(void) { return current_id; }
bool scheduler_park_current_locked(void) {
    assert(!if_enabled && current_id <= TEST_THREADS);
    blocked[current_id] = true;
    while (!woken[current_id]) pthread_cond_wait(&sleeping[current_id], &kernel_lock);
    woken[current_id] = false;
    blocked[current_id] = false;
    return true;
}
bool scheduler_wake(uint64_t id) {
    assert(!if_enabled);
    if (!id || id > TEST_THREADS || !blocked[id] || woken[id]) return false;
    woken[id] = true;
    pthread_cond_signal(&sleeping[id]);
    return true;
}
static void wait_for_count(wait_queue_t *q, unsigned expected) {
    for (unsigned tries = 0; tries < 1000000; ++tries) {
        uint64_t f = irq_save_disable();
        unsigned got = wait_queue_count(q);
        irq_restore(f);
        if (got == expected) return;
        sched_yield();
    }
    assert(!"waiter registration timed out");
}
static wait_queue_t global_wait;
static _Atomic unsigned completed;
static void *basic_waiter(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    uint64_t f = irq_save_disable();
    assert(wait_queue_sleep_locked(&global_wait));
    irq_restore(f);
    atomic_fetch_add(&completed, 1u);
    return NULL;
}
static event_t e;
static void *event_waiter(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    assert(event_wait(&e));
    atomic_fetch_add(&completed, 1u);
    return NULL;
}
static mutex_t m;
static unsigned protected_counter;
static void *mutex_waiter(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    assert(mutex_lock(&m));
    ++protected_counter;
    assert(mutex_unlock(&m));
    return NULL;
}
static void *mutex_stress_waiter(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    for (unsigned i = 0; i < 200; ++i) {
        assert(mutex_lock(&m));
        ++protected_counter;
        assert(mutex_unlock(&m));
        if (!(i % 3u)) sched_yield();
    }
    return NULL;
}
static ipc_queue_t q;
static _Atomic unsigned receive_value;
static void *consumer(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    unsigned value = 0;
    size_t got = 0;
    assert(ipc_receive(&q, &value, sizeof(value), &got) == IPC_OK);
    assert(got == sizeof(value));
    atomic_store(&receive_value, value);
    return NULL;
}
static void *producer(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    unsigned value = 5678;
    assert(ipc_send(&q, &value, sizeof(value)) == IPC_OK);
    return NULL;
}
static void *closed_consumer(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    unsigned v; size_t n;
    assert(ipc_receive(&q, &v, sizeof(v), &n) == IPC_CLOSED);
    return NULL;
}
static void *closed_producer(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    unsigned v = 500;
    assert(ipc_send(&q, &v, sizeof(v)) == IPC_CLOSED);
    return NULL;
}
int main(void) {
    as_thread(1);
    for (unsigned i = 1; i <= TEST_THREADS; ++i) assert(!pthread_cond_init(&sleeping[i], NULL));
    assert(!pthread_mutex_lock(&kernel_lock));
    assert(!pthread_mutex_unlock(&kernel_lock));
    pthread_t a, b;
    wait_queue_init(&global_wait);
    assert(!pthread_create(&a, NULL, basic_waiter, (void *)(uintptr_t)2));
    wait_for_count(&global_wait, 1);
    assert(!pthread_create(&b, NULL, basic_waiter, (void *)(uintptr_t)3));
    wait_for_count(&global_wait, 2);
    uint64_t f = irq_save_disable();
    assert(wait_queue_wake_one_locked(&global_wait) == 1);
    assert(wait_queue_count(&global_wait) == 1);
    assert(wait_queue_wake_all_locked(&global_wait) == 1);
    irq_restore(f);
    pthread_join(a, NULL); pthread_join(b, NULL);
    assert(atomic_load(&completed) == 2);

    event_init(&e, false);
    atomic_store(&completed, 0);
    assert(!pthread_create(&a, NULL, event_waiter, (void *)(uintptr_t)2));
    assert(!pthread_create(&b, NULL, event_waiter, (void *)(uintptr_t)3));
    wait_for_count(&e.waiters, 2);
    event_set(&e); event_reset(&e); /* wake remains valid even after reset */
    pthread_join(a, NULL); pthread_join(b, NULL);
    assert(atomic_load(&completed) == 2 && !event_is_set(&e));
    event_set(&e);
    assert(event_is_set(&e) && event_wait(&e));
    event_reset(&e);
    mutex_init(&m);
    assert(mutex_try_lock(&m) && !mutex_try_lock(&m));
    assert(!pthread_create(&a, NULL, mutex_waiter, (void *)(uintptr_t)2));
    assert(!pthread_create(&b, NULL, mutex_waiter, (void *)(uintptr_t)3));
    wait_for_count(&m.waiters, 2);
    assert(mutex_unlock(&m));
    pthread_join(a, NULL); pthread_join(b, NULL);
    assert(protected_counter == 2 && !mutex_unlock(&m));
    pthread_t stress[4];
    for (unsigned i = 0; i < 4; ++i)
        assert(!pthread_create(&stress[i], NULL, mutex_stress_waiter,
                               (void *)(uintptr_t)(i + 2u)));
    for (unsigned i = 0; i < 4; ++i) pthread_join(stress[i], NULL);
    assert(protected_counter == 802);


    ipc_init(&q);
    size_t got = 0;
    unsigned value = 0;
    assert(ipc_try_receive(&q, &value, sizeof(value), &got) == IPC_WOULD_BLOCK);
    assert(ipc_try_send(&q, &value, IPC_MESSAGE_BYTES + 1u) == IPC_BAD_ARGUMENT);
    assert(!pthread_create(&a, NULL, consumer, (void *)(uintptr_t)2));
    wait_for_count(&q.readers, 1);
    value = 1234;
    assert(ipc_try_send(&q, &value, sizeof(value)) == IPC_OK);
    pthread_join(a, NULL);
    assert(atomic_load(&receive_value) == 1234);

    for (unsigned i = 0; i < IPC_QUEUE_CAPACITY; ++i)
        assert(ipc_try_send(&q, &i, sizeof(i)) == IPC_OK);
    assert(ipc_try_send(&q, &value, sizeof(value)) == IPC_WOULD_BLOCK);
    assert(!pthread_create(&a, NULL, producer, (void *)(uintptr_t)2));
    wait_for_count(&q.writers, 1);
    for (unsigned i = 0; i < IPC_QUEUE_CAPACITY; ++i) {
        unsigned n = 999;
        assert(ipc_try_receive(&q, &n, 2, &got) == IPC_BUFFER_TOO_SMALL);
        assert(got == sizeof(n));
        assert(ipc_try_receive(&q, &n, sizeof(n), &got) == IPC_OK);
        assert(n == i);
    }
    pthread_join(a, NULL);
    assert(ipc_try_receive(&q, &value, sizeof(value), &got) == IPC_OK && value == 5678);
    assert(ipc_try_receive(&q, &value, sizeof(value), &got) == IPC_WOULD_BLOCK);

    ipc_init(&q);
    assert(!pthread_create(&a, NULL, closed_consumer, (void *)(uintptr_t)2));
    wait_for_count(&q.readers, 1);
    ipc_close(&q);
    pthread_join(a, NULL);
    assert(ipc_try_send(&q, &value, sizeof(value)) == IPC_CLOSED);
    assert(ipc_try_receive(&q, &value, sizeof(value), &got) == IPC_CLOSED);

    ipc_init(&q);
    for (unsigned i = 0; i < IPC_QUEUE_CAPACITY; ++i)
        assert(ipc_try_send(&q, &i, sizeof(i)) == IPC_OK);
    assert(!pthread_create(&a, NULL, closed_producer, (void *)(uintptr_t)2));
    wait_for_count(&q.writers, 1);
    ipc_close(&q);
    pthread_join(a, NULL);
    for (unsigned i = 0; i < IPC_QUEUE_CAPACITY; ++i) {
        assert(ipc_try_receive(&q, &value, sizeof(value), &got) == IPC_OK);
        assert(value == i);
    }
    assert(ipc_try_receive(&q, &value, sizeof(value), &got) == IPC_CLOSED);
    puts("PASS: wait FIFO, manual-reset event, mutex contention, IPC FIFO/backpressure/close");
    return 0;
}
