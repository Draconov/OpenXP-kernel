#include "sync/ipc.h"
#include "arch/x86_64/irq_state.h"
#include <stddef.h>

/* Avoid freestanding libc and sharing unowned pointers across threads. */
static void bytes_copy(uint8_t *dst, const uint8_t *src, size_t length) {
    for (size_t i = 0; i < length; ++i) dst[i] = src[i];
}
void ipc_init(ipc_queue_t *q) { if (q) *q = (ipc_queue_t){0}; }
void ipc_close(ipc_queue_t *q) {
    if (!q) return;
    uint64_t flags = irq_save_disable();
    q->closed = true;
    (void)wait_queue_wake_all_locked(&q->readers);
    (void)wait_queue_wake_all_locked(&q->writers);
    irq_restore(flags);
}
static ipc_result_t send_common(ipc_queue_t *q, const void *data, size_t len, bool wait) {
    if (!q || len > IPC_MESSAGE_BYTES || (len && !data)) return IPC_BAD_ARGUMENT;
    if (wait && !irq_are_enabled()) return IPC_CANNOT_SLEEP;
    uint64_t flags = irq_save_disable();
    while (!q->closed && q->count == IPC_QUEUE_CAPACITY) {
        if (!wait) { irq_restore(flags); return IPC_WOULD_BLOCK; }
        if (!wait_queue_sleep_locked(&q->writers)) {
            irq_restore(flags); return IPC_CANNOT_SLEEP;
        }
    }
    if (q->closed) { irq_restore(flags); return IPC_CLOSED; }
    unsigned tail = (q->head + q->count) % IPC_QUEUE_CAPACITY;
    bytes_copy(q->bytes[tail], (const uint8_t *)data, len);
    q->lengths[tail] = len;
    ++q->count;
    (void)wait_queue_wake_one_locked(&q->readers);
    irq_restore(flags);
    return IPC_OK;
}
static ipc_result_t receive_common(ipc_queue_t *q, void *buffer, size_t cap,
                                   size_t *len, bool wait) {
    if (!q || !len || (cap && !buffer)) return IPC_BAD_ARGUMENT;
    if (wait && !irq_are_enabled()) return IPC_CANNOT_SLEEP;
    uint64_t flags = irq_save_disable();
    while (!q->count) {
        if (q->closed) { irq_restore(flags); return IPC_CLOSED; }
        if (!wait) { irq_restore(flags); return IPC_WOULD_BLOCK; }
        if (!wait_queue_sleep_locked(&q->readers)) {
            irq_restore(flags); return IPC_CANNOT_SLEEP;
        }
    }
    const size_t msg_len = q->lengths[q->head];
    *len = msg_len;
    if (cap < msg_len) { irq_restore(flags); return IPC_BUFFER_TOO_SMALL; }
    bytes_copy((uint8_t *)buffer, q->bytes[q->head], msg_len);
    q->lengths[q->head] = 0;
    q->head = (q->head + 1u) % IPC_QUEUE_CAPACITY;
    --q->count;
    (void)wait_queue_wake_one_locked(&q->writers);
    irq_restore(flags);
    return IPC_OK;
}
ipc_result_t ipc_try_send(ipc_queue_t *q, const void *data, size_t len) {
    return send_common(q, data, len, false);
}
ipc_result_t ipc_send(ipc_queue_t *q, const void *data, size_t len) {
    return send_common(q, data, len, true);
}
ipc_result_t ipc_try_receive(ipc_queue_t *q, void *data, size_t cap, size_t *len) {
    return receive_common(q, data, cap, len, false);
}
ipc_result_t ipc_receive(ipc_queue_t *q, void *data, size_t cap, size_t *len) {
    return receive_common(q, data, cap, len, true);
}
