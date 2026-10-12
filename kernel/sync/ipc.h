#ifndef OPENXP_IPC_H
#define OPENXP_IPC_H
#include "sync/wait_queue.h"
#include <stddef.h>
#include <stdint.h>
/* Embedded, FIFO, bounded message transport. No allocation, pointers across
 * threads, user processes, device drivers, external handles or serialization. */
#define IPC_QUEUE_CAPACITY 8u
#define IPC_MESSAGE_BYTES 64u
typedef enum {
    IPC_OK = 0, IPC_WOULD_BLOCK, IPC_CLOSED, IPC_BAD_ARGUMENT,
    IPC_BUFFER_TOO_SMALL, IPC_CANNOT_SLEEP
} ipc_result_t;
typedef struct {
    uint8_t bytes[IPC_QUEUE_CAPACITY][IPC_MESSAGE_BYTES];
    size_t lengths[IPC_QUEUE_CAPACITY];
    unsigned head, count;
    bool closed;
    wait_queue_t readers, writers;
} ipc_queue_t;
void ipc_init(ipc_queue_t *q);
void ipc_close(ipc_queue_t *q);
/* Nonblocking variants work in an IF=0 critical section, but must not be
 * invoked concurrently from a hardware ISR while mutating shared data. */
ipc_result_t ipc_try_send(ipc_queue_t *q, const void *data, size_t length);
ipc_result_t ipc_try_receive(ipc_queue_t *q, void *buffer, size_t capacity, size_t *length);
/* Blocking variants require IF=1, can park until data/space/close. */
ipc_result_t ipc_send(ipc_queue_t *q, const void *data, size_t length);
ipc_result_t ipc_receive(ipc_queue_t *q, void *buffer, size_t capacity, size_t *length);
#endif
