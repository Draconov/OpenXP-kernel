#ifndef OPENXP_USERSPACE_H
#define OPENXP_USERSPACE_H
#include <stdbool.h>
#include <stdint.h>
#include "mm/address_space.h"
/* Embedded static ELF64 demos: both synchronous legacy process runner
 * and experimental two-process preemption via the PIC/PIT scheduler. */
bool userspace_demo_run(unsigned mode);
bool userspace_preempt_demo_run(void);
bool userspace_initrd_demo_run(void);
bool userspace_active(void);
const address_space_t *userspace_current_space(void);
void userspace_finish(uint64_t exit_status);
__attribute__((noreturn)) void userspace_fault(uint64_t vector, uint64_t fault_address,
                                             uint64_t instruction_pointer, uint64_t user_stack_pointer);
void arch_user_enter(uint64_t user_ip, uint64_t user_stack_top,
                     uint64_t *kernel_continuation, unsigned enable_irqs);
__attribute__((noreturn)) void arch_user_return(void);
#endif
