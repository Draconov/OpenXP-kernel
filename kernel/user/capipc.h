#ifndef OPENXP_USER_CAPIPC_H
#define OPENXP_USER_CAPIPC_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Experimental one-core capability IPC for the trusted boot-image window
 * service. All entries and payloads are kernel-owned. No shared user pages. */
#define CAPIPC_MAX_ENDPOINTS 12u
#define CAPIPC_QUEUE_CAP 4u
#define CAPIPC_MSG_BYTES 64u
#define CAPIPC_SERVER_WAIT_HANDLE 0u

typedef enum {
    CAPIPC_OK=0, CAPIPC_EMPTY, CAPIPC_FULL, CAPIPC_CLOSED,
    CAPIPC_INVALID, CAPIPC_DENIED, CAPIPC_NOSPACE
} capipc_result_t;
void capipc_init(void);
/* Only trusted kernel code can authorize the server image's PID. */
bool capipc_grant_server(uint64_t pid);
capipc_result_t capipc_listen(uint64_t pid);
capipc_result_t capipc_connect(uint64_t pid, uint64_t *handle);
capipc_result_t capipc_accept(uint64_t pid, uint64_t *handle);
capipc_result_t capipc_send(uint64_t pid,uint64_t handle,const void *buf,size_t len);
capipc_result_t capipc_receive(uint64_t pid,uint64_t handle,void *buf,size_t cap,size_t *len);
capipc_result_t capipc_close(uint64_t pid,uint64_t handle);
void capipc_process_exit(uint64_t pid);
/* Atomically checks pending messages/connections then parks on the single-core
 * scheduler. timeout_ticks=0 blocks indefinitely; syscall bounds it. */
capipc_result_t capipc_wait(uint64_t pid,uint64_t handle,uint64_t timeout_ticks);
#endif
