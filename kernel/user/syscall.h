#ifndef OPENXP_USER_SYSCALL_H
#define OPENXP_USER_SYSCALL_H
#include <stdint.h>
#include "arch/x86_64/interrupts.h"
#define OPENXP_SYS_VERSION 0u
#define OPENXP_SYS_PUTC    1u
#define OPENXP_SYS_EXIT    2u
#define OPENXP_SYS_TICKS   3u
#define OPENXP_SYS_WRITE   4u
#define OPENXP_SYS_GETPID  5u
#define OPENXP_API_VERSION 0x00010000u
#define OPENXP_ENOSYS 38
#define OPENXP_EPERM 1
#define OPENXP_EFAULT 14
#define OPENXP_EINVAL 22
/* System V ABI: RAX = exit requested; RDX = syscall result. */
struct syscall_result { uint64_t exit; uint64_t result; };
struct syscall_result user_syscall_dispatch(const struct exception_frame *frame);
#endif
