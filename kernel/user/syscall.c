#include "user/syscall.h"
#include "user/clipboard.h"
#include "user/userspace.h"
#include "user/process.h"
#include "user/capipc.h"
#include "user/wm_display.h"
#include "arch/x86_64/timer.h"
#include "arch/x86_64/rtc.h"
#include "mm/pmm.h"
#include "serial.h"
#include "gui/native.h"
#include "gui/shell_bridge.h"
#include "fs/vfs.h"
#include "user/app_file.h"
#include "user/app_launch.h"
#include "fs/persist.h"
#include "fs/ramfs.h"
#include "arch/x86_64/irq_state.h"
#include <stddef.h>

static openxp_clipboard_t system_clipboard;
static volatile bool flush_requested,flush_error;
static uint64_t last_storage_revision;
bool user_storage_flush_pending(void){return flush_requested;}
void user_storage_mark_committed(uint64_t revision){
    last_storage_revision=revision;flush_requested=false;flush_error=false;
}
void user_storage_mark_error(void){flush_error=true;}

/* One intentionally bounded buffer syscall. User addresses are never directly
 * dereferenced in CPL0; page-permission checking precedes the entire write. */
struct syscall_result user_syscall_dispatch(const struct exception_frame *frame) {
    if ((frame->cs & 3u) != 3u || !userspace_active())
        return (struct syscall_result){0, (uint64_t)-OPENXP_EPERM};
    switch (frame->rax) {
    case OPENXP_SYS_RTC_DATE: {
        rtc_date_t today={0};
        if (!rtc_read_date(&today) || today.year>65535u || today.month>255u || today.day>255u)
            return (struct syscall_result){0,0};
        return (struct syscall_result){0,((uint64_t)today.year<<16)|((uint64_t)today.month<<8)|today.day};
    }
    case OPENXP_SYS_FREE_PAGES:
        return (struct syscall_result){0,pmm_free_pages()};
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
    case OPENXP_SYS_GUI_BITMAP: {
        const address_space_t *space=userspace_current_space();
        if(!space || !aspace_range_valid(space,frame->rdi,sizeof(native_bitmap_req_t),false))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        native_bitmap_req_t tile;
        if(!aspace_copy_from_user(space,&tile,frame->rdi,sizeof tile))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        return (struct syscall_result){0,native_bitmap(process_current_pid(),&tile)?0u:(uint64_t)-OPENXP_EINVAL};
    }
    case OPENXP_SYS_GUI_CANVAS_SIZE:
        return (struct syscall_result){0,native_canvas_size(process_current_pid())};
    case OPENXP_SYS_GUI_POINTER:
        return (struct syscall_result){0, native_pointer_enable(process_current_pid())?0u:(uint64_t)-OPENXP_EPERM};
    case OPENXP_SYS_GUI_EVENT:
        return (struct syscall_result){0,native_next_event(process_current_pid())};
    case OPENXP_SYS_GUI_WAIT:
        if (frame->rdi >= (UINT64_C(1) << 63))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        return (struct syscall_result){0,native_wait_event(process_current_pid(),frame->rdi)};
    case OPENXP_SYS_CLIPBOARD_SET: {
        const address_space_t *space=userspace_current_space();
        size_t n=(size_t)frame->rsi;
        if(!space||n>OPENXP_CLIPBOARD_CAP)return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        uint8_t copy[OPENXP_CLIPBOARD_CAP];
        if(n && (!aspace_range_valid(space,frame->rdi,n,false)||
                 !aspace_copy_from_user(space,copy,frame->rdi,n)))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        (void)openxp_clipboard_put(&system_clipboard,copy,n);
        return (struct syscall_result){0,n};
    }
    case OPENXP_SYS_CLIPBOARD_GET: {
        address_space_t *space=(address_space_t *)userspace_current_space();
        size_t n=system_clipboard.length;
        if(!space)return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        if(n>frame->rsi)return (struct syscall_result){0,(uint64_t)-OPENXP_ENOSPC};
        if(n && (!aspace_range_valid(space,frame->rdi,n,true)||
                 !aspace_copy_to_user(space,frame->rdi,system_clipboard.bytes,n)))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        return (struct syscall_result){0,n};
    }
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
    case OPENXP_SYS_APP_LAUNCH: {
        const address_space_t *space=userspace_current_space();
        if(!space || frame->rsi<5 || frame->rsi>=APP_FILE_PATH ||
           !aspace_range_valid(space,frame->rdi,(size_t)frame->rsi,false))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        char path[APP_FILE_PATH]={0};
        if(!aspace_copy_from_user(space,path,frame->rdi,(size_t)frame->rsi))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        for(size_t i=0;i<(size_t)frame->rsi;++i)
            if(!path[i])return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        if(!app_launch_allowed(path))return (struct syscall_result){0,(uint64_t)-OPENXP_EPERM};
        return (struct syscall_result){0,app_launch_submit(path)?0:(uint64_t)-OPENXP_EAGAIN};
    }
    case OPENXP_SYS_APP_LAUNCH_ARG: {
        const address_space_t *space=userspace_current_space();
        app_launch_request_t req;
        if(!space || !aspace_range_valid(space,frame->rdi,sizeof req,false) ||
           !aspace_copy_from_user(space,&req,frame->rdi,sizeof req))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        bool executable_end=false,argument_end=false;
        for(size_t i=0;i<sizeof req.executable;++i)
            if(req.executable[i]==0)executable_end=true;
        for(size_t i=0;i<sizeof req.argument;++i)
            if(req.argument[i]==0)argument_end=true;
        if(!executable_end || !argument_end || !req.argument[0])
            return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        if(!app_launch_allowed(req.executable))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EPERM};
        return (struct syscall_result){0,app_launch_submit_with_arg(req.executable,req.argument)?
            0:(uint64_t)-OPENXP_EAGAIN};
    }
    case OPENXP_SYS_APP_GET_ARG: {
        address_space_t *space=(address_space_t *)userspace_current_space();
        char argument[PROCESS_LAUNCH_ARG_MAX]={0};
        size_t n=process_current_launch_arg(argument,sizeof argument);
        if(!n)return (struct syscall_result){0,0};
        if(!space || frame->rsi<=n)return (struct syscall_result){0,(uint64_t)-OPENXP_ENOSPC};
        if(!aspace_range_valid(space,frame->rdi,n+1u,true) ||
           !aspace_copy_to_user(space,frame->rdi,argument,n+1u))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        return (struct syscall_result){0,n};
    }
    case OPENXP_SYS_STORAGE: {
        if(frame->rdi>1u)return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        if(!persist_is_mounted())return (struct syscall_result){0,0};
        if(frame->rdi==1u)flush_requested=true;
        uint64_t flags=OPENXP_STORAGE_PRESENT;
        if(ramfs_revision()!=last_storage_revision)flags|=OPENXP_STORAGE_DIRTY;
        if(flush_requested)flags|=OPENXP_STORAGE_QUEUED;
        if(flush_error)flags|=OPENXP_STORAGE_ERROR;
        return (struct syscall_result){0,flags};
    }
    case OPENXP_SYS_FS_OP: {
        const address_space_t *space=userspace_current_space();
        if(!space || frame->rdi<APP_FS_LIST || frame->rdi>APP_FS_STAT)
            return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        if(!aspace_range_valid(space,frame->rsi,sizeof(app_file_request_t),true))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        app_file_request_t req;
        if(!aspace_copy_from_user(space,&req,frame->rsi,sizeof(req)))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        bool path_ok=false,dest_ok=false;
        for(size_t i=0;i<APP_FILE_PATH;++i){
            if(req.path[i]==0)path_ok=true;
            if(req.dest[i]==0)dest_ok=true;
        }
        if(!path_ok||!dest_ok||req.size>APP_FILE_DATA)
            return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        uint64_t result=0;
        uint64_t saved=irq_save_disable();
        switch(frame->rdi){
        case APP_FS_LIST:
            result=(req.dest[0]=='D' && req.dest[1]==0 ?
                vfs_list_dir(req.path,req.index,req.path,sizeof req.path) :
                vfs_name_at(req.index,req.path,sizeof req.path))?
                0:(uint64_t)-OPENXP_EINVAL;
            if(!result)req.size=0;
            break;
        case APP_FS_READ: {
            size_t n=0;
            bool ok=vfs_read(req.path,req.data,APP_FILE_DATA,&n);
            req.size=ok?(uint32_t)n:0;
            result=ok?0:(uint64_t)-OPENXP_EINVAL;
            break;
        }
        case APP_FS_READ_SLICE: {
            size_t n=0;
            bool ok=vfs_read_slice(req.path,req.index,req.data,req.size,&n);
            req.size=ok?(uint32_t)n:0;
            result=ok?0:(uint64_t)-OPENXP_EINVAL;
            break;
        }
        case APP_FS_WRITE:
            result=vfs_write(req.path,req.data,req.size)?0:(uint64_t)-OPENXP_EPERM;
            break;
        case APP_FS_DELETE:
            result=vfs_remove(req.path)?0:(uint64_t)-OPENXP_EPERM;
            break;
        case APP_FS_RENAME:
            result=vfs_rename(req.path,req.dest)?0:(uint64_t)-OPENXP_EPERM;
            break;
        case APP_FS_COPY:
            result=vfs_copy(req.path,req.dest)?0:(uint64_t)-OPENXP_EPERM;
            break;
        case APP_FS_KIND:
            result=(uint64_t)vfs_kind(req.path);
            break;
        case APP_FS_MKDIR:
            result=vfs_mkdir(req.path)?0:(uint64_t)-OPENXP_EPERM;
            break;
        case APP_FS_RMDIR:
            result=vfs_rmdir(req.path)?0:(uint64_t)-OPENXP_EPERM;
            break;
        case APP_FS_STAT: {
            size_t bytes=0;vfs_kind_t kind=VFS_MISSING;
            bool ok=vfs_stat(req.path,&bytes,&kind) && bytes<=UINT32_MAX;
            if(ok){req.index=(uint32_t)kind;req.size=(uint32_t)bytes;}
            result=ok?0:(uint64_t)-OPENXP_EINVAL;
            break;
        }
        default:result=(uint64_t)-OPENXP_EINVAL;break;
        }
        irq_restore(saved);
        if(frame->rdi==APP_FS_LIST || frame->rdi==APP_FS_READ ||
           frame->rdi==APP_FS_READ_SLICE || frame->rdi==APP_FS_STAT){
            if(!result && !aspace_copy_to_user((address_space_t *)space,frame->rsi,&req,sizeof(req)))
                return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        }
        return (struct syscall_result){0,result};
    }
    case OPENXP_SYS_IPC_LISTEN:
    case OPENXP_SYS_IPC_ACCEPT:
    case OPENXP_SYS_IPC_CONNECT:
    case OPENXP_SYS_IPC_CLOSE:
    case OPENXP_SYS_IPC_WAIT: {
        uint64_t pid=process_current_pid(), handle=0;
        capipc_result_t r=CAPIPC_INVALID;
        if(frame->rax==OPENXP_SYS_IPC_LISTEN)r=capipc_listen(pid);
        else if(frame->rax==OPENXP_SYS_IPC_ACCEPT)r=capipc_accept(pid,&handle);
        else if(frame->rax==OPENXP_SYS_IPC_CONNECT)r=capipc_connect(pid,&handle);
        else if(frame->rax==OPENXP_SYS_IPC_CLOSE)r=capipc_close(pid,frame->rdi);
        else r=capipc_wait(pid,frame->rdi,frame->rsi);
        if(r==CAPIPC_OK && (frame->rax==OPENXP_SYS_IPC_ACCEPT || frame->rax==OPENXP_SYS_IPC_CONNECT))
            return (struct syscall_result){0,handle};
        if(r==CAPIPC_OK)return (struct syscall_result){0,0};
        int err=r==CAPIPC_EMPTY || r==CAPIPC_FULL?OPENXP_EAGAIN:
            r==CAPIPC_CLOSED?OPENXP_EPIPE:r==CAPIPC_NOSPACE?OPENXP_ENOSPC:
            r==CAPIPC_DENIED?OPENXP_EPERM:OPENXP_EINVAL;
        return (struct syscall_result){0,(uint64_t)-(uint64_t)err};
    }
    case OPENXP_SYS_IPC_SEND:
    case OPENXP_SYS_IPC_RECV: {
        uint64_t pid=process_current_pid();
        const address_space_t *space=userspace_current_space();
        /* handle in RDI, buffer in RSI, length/capacity in RDX. */
        if(!space || !frame->rdx || frame->rdx>CAPIPC_MSG_BYTES)
            return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        bool receive=frame->rax==OPENXP_SYS_IPC_RECV;
        if(!aspace_range_valid(space,frame->rsi,(size_t)frame->rdx,receive))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        uint8_t data[CAPIPC_MSG_BYTES]={0};size_t n=0;
        if(!receive && !aspace_copy_from_user(space,data,frame->rsi,(size_t)frame->rdx))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        capipc_result_t r=receive?capipc_receive(pid,frame->rdi,data,(size_t)frame->rdx,&n):
            capipc_send(pid,frame->rdi,data,(size_t)frame->rdx);
        if(r==CAPIPC_OK && receive && !aspace_copy_to_user((address_space_t *)space,frame->rsi,data,n))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        if(r==CAPIPC_OK)return (struct syscall_result){0,receive?n:frame->rdx};
        int err=r==CAPIPC_EMPTY || r==CAPIPC_FULL?OPENXP_EAGAIN:
            r==CAPIPC_CLOSED?OPENXP_EPIPE:r==CAPIPC_NOSPACE?OPENXP_ENOSPC:
            r==CAPIPC_DENIED?OPENXP_EPERM:OPENXP_EINVAL;
        return (struct syscall_result){0,(uint64_t)-(uint64_t)err};
    }
    case OPENXP_SYS_WM_DISPLAY: {
        /* Privilege is granted by the kernel to the immutable windowd
         * image. The accepted channel is also checked for a live peer;
         * the server cannot name an unrelated process or a kernel window. */
        uint64_t target_pid=0;
        if(!capipc_server_peer(process_current_pid(),frame->rdi,&target_pid))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EPERM};
        const address_space_t *space=userspace_current_space();
        if(frame->rdx!=sizeof(openxp_wm_display_cmd_t) || !space)
            return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        if(!aspace_range_valid(space,frame->rsi,sizeof(openxp_wm_display_cmd_t),false))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        openxp_wm_display_cmd_t command;
        if(!aspace_copy_from_user(space,&command,frame->rsi,sizeof(command)))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        return (struct syscall_result){0,
            native_wm_manage(target_pid,command.action,command.x,command.y,
                             command.width,command.height)?0u:(uint64_t)-OPENXP_EINVAL};
    }
    case OPENXP_SYS_NATIVE_INFO:
    case OPENXP_SYS_NATIVE_ROW: {
        if(!capipc_is_server(process_current_pid()))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EPERM};
        const address_space_t *space=userspace_current_space();
        uint32_t slot=(uint32_t)frame->rdi;
        if(!space || frame->rdi>=OXP_NATIVE_SLOTS)
            return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        if(frame->rax==OPENXP_SYS_NATIVE_INFO){
            if(frame->rdx!=sizeof(oxp_native_info_t) ||
               !aspace_range_valid(space,frame->rsi,sizeof(oxp_native_info_t),true))
                return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
            native_window_t snapshot=native_snapshot_slot(slot);
            oxp_native_info_t info={0};
            info.active=snapshot.active?1u:0u;
            info.painted=native_has_surface(slot)?1u:0u;
            info.generation=snapshot.content_generation;
            info.surface_w=snapshot.surface_w;
            info.surface_h=snapshot.surface_h;
            for(unsigned i=0;i<sizeof(info.title);++i)info.title[i]=snapshot.title[i];
            for(unsigned i=0;i<sizeof(info.text);++i)info.text[i]=snapshot.text[i];
            return (struct syscall_result){0,
                aspace_copy_to_user((address_space_t *)space,frame->rsi,&info,sizeof(info))
                    ?0u:(uint64_t)-OPENXP_EFAULT};
        }
        if(!aspace_range_valid(space,frame->rsi,sizeof(oxp_native_row_t),true))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        oxp_native_row_t row={.slot=slot};
        /* RDX is the bounded scanline index. */
        if(frame->rdx>=OXP_NATIVE_H ||
           !native_copy_surface_row(slot,(unsigned)frame->rdx,row.pixels,OXP_NATIVE_W))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
        row.y=(uint32_t)frame->rdx;
        return (struct syscall_result){0,
            aspace_copy_to_user((address_space_t *)space,frame->rsi,&row,sizeof(row))
                ?0u:(uint64_t)-OPENXP_EFAULT};
    }
    case OPENXP_SYS_SHELL_RASTER_ROW:
    case OPENXP_SYS_SHELL_RASTER_COMMIT: {
        uint64_t pid=process_current_pid();
        if(!capipc_is_server(pid))return (struct syscall_result){0,(uint64_t)-OPENXP_EPERM};
        if(frame->rax==OPENXP_SYS_SHELL_RASTER_COMMIT)
            return (struct syscall_result){0,shell_bridge_raster_commit(pid,(uint32_t)frame->rdi)
                ?0u:(uint64_t)-OPENXP_EINVAL};
        const address_space_t *space=userspace_current_space();
        if(!space || frame->rsi!=sizeof(oxp_shell_raster_row_t) ||
           !aspace_range_valid(space,frame->rdi,sizeof(oxp_shell_raster_row_t),false))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        oxp_shell_raster_row_t row;
        if(!aspace_copy_from_user(space,&row,frame->rdi,sizeof(row)))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        return (struct syscall_result){0,shell_bridge_raster_row(pid,&row)
            ?0u:(uint64_t)-OPENXP_EINVAL};
    }
    case OPENXP_SYS_SHELL_EVENT:
    case OPENXP_SYS_SHELL_WINDOW:
    case OPENXP_SYS_SHELL_FRAME:
    case OPENXP_SYS_SHELL_ACTION: {
        /* The trusted, immutable windowd boot-image has the unique IPC
         * listener capability. Ordinary Ring 3 processes get EPERM. */
        uint64_t pid=process_current_pid();
        if(!capipc_is_server(pid))return (struct syscall_result){0,(uint64_t)-OPENXP_EPERM};
        const address_space_t *space=userspace_current_space();
        if(!space)return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        if(frame->rax==OPENXP_SYS_SHELL_ACTION)
            return (struct syscall_result){0,shell_bridge_action((uint32_t)frame->rdi,
                    (uint32_t)frame->rsi,frame->rdx)?0u:(uint64_t)-OPENXP_EINVAL};
        if(frame->rax==OPENXP_SYS_SHELL_EVENT){
            oxp_shell_event_t event={0};
            if(frame->rsi!=sizeof(event) ||
               !aspace_range_valid(space,frame->rdi,sizeof(event),true))
                return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
            if(!shell_bridge_pop(&event))return (struct syscall_result){0,0};
            if(!aspace_copy_to_user((address_space_t *)space,frame->rdi,&event,sizeof(event)))
                return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
            return (struct syscall_result){0,1};
        }
        if(frame->rax==OPENXP_SYS_SHELL_WINDOW){
            oxp_shell_window_t info={0};
            if(frame->rdx!=sizeof(info) ||
               !aspace_range_valid(space,frame->rsi,sizeof(info),true))
                return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
            if(!shell_bridge_window((uint32_t)frame->rdi,&info))
                return (struct syscall_result){0,(uint64_t)-OPENXP_EINVAL};
            if(!aspace_copy_to_user((address_space_t *)space,frame->rsi,&info,sizeof(info)))
                return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
            return (struct syscall_result){0,0};
        }
        size_t count=(size_t)frame->rsi;
        if(!count || count>OXP_SHELL_DRAW_MAX ||
           !aspace_range_valid(space,frame->rdi,count*sizeof(oxp_shell_draw_t),false))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        oxp_shell_draw_t buffer[OXP_SHELL_DRAW_MAX];
        if(!aspace_copy_from_user(space,buffer,frame->rdi,count*sizeof(oxp_shell_draw_t)))
            return (struct syscall_result){0,(uint64_t)-OPENXP_EFAULT};
        return (struct syscall_result){0,shell_bridge_submit(pid,buffer,(uint32_t)count,timer_ticks())?0u:(uint64_t)-OPENXP_EINVAL};
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
