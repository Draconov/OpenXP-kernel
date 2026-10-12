#include "user/syscall.h"
#include "user/userspace.h"
#include "user/process.h"
#include "arch/x86_64/timer.h"
#include "serial.h"
#include <stddef.h>

/* One intentionally bounded buffer syscall. User addresses are never directly
 * dereferenced in CPL0; page-permission checking precedes the entire write. */
struct syscall_result user_syscall_dispatch(const struct exception_frame *frame) {
    if ((frame->cs & 3u) != 3u || !userspace_active())
        return (struct syscall_result){0, (uint64_t)-OPENXP_EPERM};
    switch (frame->rax) {
    case OPENXP_SYS_VERSION:
        return (struct syscall_result){0, OPENXP_API_VERSION};
    case OPENXP_SYS_PUTC:
        if (frame->rdi > 127) return (struct syscall_result){0, (uint64_t)-OPENXP_EPERM};
        serial_putc((char)frame->rdi);
        return (struct syscall_result){0, 0};
    case OPENXP_SYS_WRITE: {
        const address_space_t *space = userspace_current_space();
        if (!space || frame->rsi > 1024) return (struct syscall_result){0, (uint64_t)-OPENXP_EINVAL};
        if (!aspace_range_valid(space, frame->rdi, (size_t)frame->rsi, false))
            return (struct syscall_result){0, (uint64_t)-OPENXP_EFAULT};
        char buffer[64];
        size_t offset = 0;
        while (offset < frame->rsi) {
            size_t amount = (size_t)(frame->rsi - offset);
            if (amount > sizeof(buffer)) amount = sizeof(buffer);
            if (!aspace_copy_from_user(space, buffer, frame->rdi + offset, amount))
                return (struct syscall_result){0, (uint64_t)-OPENXP_EFAULT};
            for (size_t i = 0; i < amount; ++i) serial_putc(buffer[i]);
            offset += amount;
        }
        return (struct syscall_result){0, frame->rsi};
    }
    case OPENXP_SYS_EXIT:
        userspace_finish(frame->rdi);
        return (struct syscall_result){1, 0};
    case OPENXP_SYS_GETPID:
        return (struct syscall_result){0, process_current_pid()};
    case OPENXP_SYS_TICKS:
        return (struct syscall_result){0, timer_ticks()};
    default:
        return (struct syscall_result){0, (uint64_t)-OPENXP_ENOSYS};
    }
}
