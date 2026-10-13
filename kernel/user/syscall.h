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
#define OPENXP_SYS_GUI_OPEN 6u
#define OPENXP_SYS_GUI_TEXT 7u
#define OPENXP_SYS_GUI_EVENT 8u
#define OPENXP_SYS_RAM_READ 9u
#define OPENXP_SYS_RAM_WRITE 10u
#define OPENXP_SYS_GUI_DRAW 11u
#define OPENXP_SYS_GUI_WAIT 12u
#define OPENXP_SYS_IPC_LISTEN 13u
#define OPENXP_SYS_IPC_ACCEPT 14u
#define OPENXP_SYS_IPC_CONNECT 15u
#define OPENXP_SYS_IPC_SEND 16u
#define OPENXP_SYS_IPC_RECV 17u
#define OPENXP_SYS_IPC_CLOSE 18u
#define OPENXP_SYS_IPC_WAIT 19u
#define OPENXP_SYS_WM_DISPLAY 20u
#define OPENXP_SYS_SHELL_EVENT 21u
#define OPENXP_SYS_SHELL_FRAME 22u
#define OPENXP_SYS_SHELL_ACTION 23u
#define OPENXP_SYS_SHELL_WINDOW 24u
#define OPENXP_EAGAIN 11
#define OPENXP_EPIPE 32
#define OPENXP_ENOSPC 28
#define OPENXP_API_VERSION 0x00010000u
#define OPENXP_ENOSYS 38
#define OPENXP_EPERM 1
#define OPENXP_EFAULT 14
#define OPENXP_EINVAL 22
/* System V ABI: RAX = exit requested; RDX = syscall result. */
struct syscall_result { uint64_t exit; uint64_t result; };
struct syscall_result user_syscall_dispatch(const struct exception_frame *frame);
#endif
