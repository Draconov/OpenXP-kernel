#include "user/syscall.h"
#include "user/userspace.h"
#include "user/process.h"
#include "arch/x86_64/timer.h"
#include "serial.h"
#include "gui/native.h"
#include "fs/vfs.h"
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
    case OPENXP_SYS_GUI_OPEN:
    case OPENXP_SYS_GUI_TEXT: {
        const address_space_t *space=userspace_current_space();
        size_t limit=frame->rax==OPENXP_SYS_GUI_OPEN?NATIVE_TITLE_MAX:NATIVE_TEXT_MAX;
        if(!space||frame->rsi==0||frame->rsi>=limit)return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        if(!aspace_range_valid(space,frame->rdi,(size_t)frame->rsi,false))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        char buffer[NATIVE_TEXT_MAX];
        if(!aspace_copy_from_user(space,buffer,frame->rdi,(size_t)frame->rsi))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        bool ok=frame->rax==OPENXP_SYS_GUI_OPEN?
            native_open(process_current_pid(),buffer,(size_t)frame->rsi):
            native_set_text(process_current_pid(),buffer,(size_t)frame->rsi);
        return (struct syscall_result){0,ok?0:(uint64_t)-OPENXP_EPERM};
    }
    case OPENXP_SYS_GUI_DRAW: {
        /* One bounded, validated command batch; no framebuffer or kernel
         * address is ever mapped into the caller's address space. */
        const address_space_t *space=userspace_current_space();
        if(!space || !frame->rsi || frame->rsi>NATIVE_DRAW_MAX)
            return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        if(!aspace_range_valid(space,frame->rdi,
                              (size_t)frame->rsi*sizeof(native_draw_cmd_t),false))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        native_draw_cmd_t cmds[NATIVE_DRAW_MAX];
        if(!aspace_copy_from_user(space,cmds,frame->rdi,
                                  (size_t)frame->rsi*sizeof(native_draw_cmd_t)))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        return (struct syscall_result){0,
            native_draw(process_current_pid(),cmds,(size_t)frame->rsi)?0u:(uint64_t)-OPENXP_EINVAL};
    }
    case OPENXP_SYS_GUI_EVENT:
        return (struct syscall_result){0,native_next_event(process_current_pid())};
    case OPENXP_SYS_GUI_WAIT:
        if (frame->rdi >= (UINT64_C(1) << 63))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        return (struct syscall_result){0,native_wait_event(process_current_pid(),frame->rdi)};
    case OPENXP_SYS_RAM_READ:
    case OPENXP_SYS_RAM_WRITE: {
        /* Narrow capability: one fixed home document per program, no arbitrary
         * kernel file paths and no user-controlled filename pointers. */
        const address_space_t *space=userspace_current_space();
        const uint64_t pid=process_current_pid();
        if(!space||!pid||frame->rsi>2048)return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        if(!aspace_range_valid(space,frame->rdi,(size_t)frame->rsi,
                              frame->rax==OPENXP_SYS_RAM_READ))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        char bytes[2048];size_t n=0;
        if(frame->rax==OPENXP_SYS_RAM_READ) {
            if(!vfs_read("home/native-note.txt",bytes,sizeof(bytes),&n))
                return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
            if(n>frame->rsi)return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
            if(!aspace_copy_to_user((address_space_t *)space,frame->rdi,bytes,n))
                return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
            return (struct syscall_result){0,n};
        }
        if(!aspace_copy_from_user(space,bytes,frame->rdi,(size_t)frame->rsi))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        return (struct syscall_result){0,vfs_write("home/native-note.txt",bytes,(size_t)frame->rsi)?frame->rsi:(uint64_t)-OPENXP_EPERM};
    }
    case OPENXP_SYS_EXIT:
        native_owner_exit(process_current_pid());
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
