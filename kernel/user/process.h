#ifndef OPENXP_USER_PROCESS_H
#define OPENXP_USER_PROCESS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mm/address_space.h"

/* Single-core experimental Ring 3 process manager. Each process owns a
 * separate private CR3 and guarded kernel-entry stack. Preemptible mode
 * requires a dedicated kernel scheduler worker and only the legacy PIC/PIT. */
#define PROCESS_MAX_SLOTS 4u

typedef enum {
    PROCESS_EMPTY = 0,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_EXITED,
    PROCESS_FAULTED
} process_state_t;

typedef struct {
    uint64_t pid;
    process_state_t state;
    uint64_t status;
    uint64_t fault_vector;
    uint64_t fault_address;
    uint64_t completion_sequence; /* nonzero after EXIT or a user fault */
} process_info_t;

bool process_init(void);
/* Returns nonzero monotonically increasing PID or zero on failure. */
uint64_t process_create_elf(const void *image, size_t length);
/* Runs until the program calls EXIT or faults; returns a status snapshot. */
bool process_run(uint64_t pid, process_info_t *result);
/* Requires IRQs disabled and a preemptive kernel worker. Ring 3 runs IF=1. */
bool process_run_preemptible(uint64_t pid, process_info_t *result);
/* Called by the assembly return trampoline after EXIT/fault. */
uint64_t process_current_resume_sp(void);
bool process_get_info(uint64_t pid, process_info_t *info);
/* Enumerate a process slot for diagnostic tools; IRQs must be disabled. */
bool process_slot_info(size_t slot, process_info_t *info);
/* Wait on the boot/controller thread with IF=1 and timer preemption active.
 * Sleeps for one PIT tick between checks. Return false on timeout, missing
 * PID, invalid context or failure to park; result is unchanged on failure.
 * max_ticks is a relative timeout in (0, 2^63). Does not reap the process. */
bool process_wait_ticks(uint64_t pid, uint64_t max_ticks, process_info_t *result);
bool process_state_terminal(process_state_t state);
/* Release all mapped user pages, page tables, and guarded kernel stack. */
bool process_reap(uint64_t pid);
/* Cancel a READY process that has never run (e.g. failed worker creation). */
bool process_cancel_ready(uint64_t pid);
uint64_t process_current_pid(void);
const address_space_t *process_current_space(void);
/* Only valid from a currently running ring-3 process's synchronous syscall. */
void process_finish(uint64_t status);
void process_record_fault(uint64_t vector, uint64_t fault_address);
#endif
