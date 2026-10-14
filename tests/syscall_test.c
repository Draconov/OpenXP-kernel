#include "user/syscall.h"
#include "user/shell_abi.h"
#include "user/app_file.h"
#include "user/app_launch.h"
#include "arch/x86_64/rtc.h"
#include "user/wm_display.h"
#include "user/capipc.h"
#include "gui/native.h"
#include "mm/address_space.h"
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
static bool active;
static uint64_t exit_status;
static char emitted;
static address_space_t mock_space = { .cr3 = 4096 };
static char output[128];
static char mock_user_memory[64] = "Hello from user memory";
static app_file_request_t mock_file;
static size_t output_len;
const address_space_t *userspace_current_space(void) { return active ? &mock_space : NULL; }
bool aspace_range_valid(const address_space_t *s, uint64_t ptr, size_t len, bool writable) {
    if(s != &mock_space)return false;
    if(ptr>=2000 && ptr-2000<=sizeof(mock_file))return len<=sizeof(mock_file)-(size_t)(ptr-2000);
    return !writable && ptr>=1000 && ptr-1000<=64 && len<=64-(size_t)(ptr-1000);
}
bool aspace_copy_from_user(const address_space_t *s, void *dst, uint64_t ptr, size_t len) {
    if (!aspace_range_valid(s,ptr,len,false)) return false;
    if(ptr>=2000)memcpy(dst,(const char *)&mock_file+(ptr-2000),len);
    else memcpy(dst,mock_user_memory+(ptr-1000),len);
    return true;
}

bool aspace_copy_to_user(address_space_t *s,uint64_t dst,const void *src,size_t len) {
    if(!aspace_range_valid(s,dst,len,true))return false;
    memcpy((char *)&mock_file+(dst-2000),src,len);return true;
}
uint64_t process_current_pid(void) { return active ? 17 : 0; }
bool userspace_active(void) { return active; }
void userspace_finish(uint64_t status) { exit_status = status; active = false; }
void serial_putc(char ch) { emitted = ch; if (output_len < sizeof(output)) output[output_len++] = ch; }
uint64_t timer_ticks(void) { return 1234; }
uint64_t pmm_free_pages(void) { return 437; }
bool rtc_read_date(rtc_date_t *out) { if(!out)return false;*out=(rtc_date_t){2026,10,13};return true; }
int main(void) {
    struct exception_frame f = {0};
    struct syscall_result r;
    f.cs = 0x23;
    f.rax = OPENXP_SYS_VERSION;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EPERM);
    active = true;
    f.cs = 0x08;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EPERM);
    f.cs = 0x23;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == OPENXP_API_VERSION);
    f.rax=OPENXP_SYS_RTC_DATE;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result == ((2026u<<16)|(10u<<8)|13u));
    f.rax=OPENXP_SYS_FREE_PAGES;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==437);
    f.rax = OPENXP_SYS_PUTC;
    f.rdi = 'X';
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 0 && emitted == 'X');
    f.rdi = 200;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EPERM && emitted == 'X');
    f.rax = OPENXP_SYS_WRITE;
    f.rdi = 1000;
    f.rsi = 22;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 22 && output_len == 23);
    assert(!memcmp(output+1,"Hello from user memory",22));
    f.rdi = 999;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EFAULT && output_len == 23);
    f.rdi = 1000;
    f.rsi = 1025;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EINVAL && output_len == 23);
    f.rax = OPENXP_SYS_GETPID;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 17);
    f.rax = OPENXP_SYS_TICKS;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 1234);
    native_reset();
    f.rax = OPENXP_SYS_GUI_OPEN;
    f.rdi = 999;f.rsi = 5;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EFAULT);
    f.rdi = 1000;f.rsi = 5;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 0 && native_snapshot().active);
    f.rax=OPENXP_SYS_GUI_CANVAS_SIZE;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==((uint64_t)320<<32|180u));
    native_set_window_size(0,1024u,726u);
    r=user_syscall_dispatch(&f);
    assert(!r.exit && (unsigned)(r.result>>32)<320u && (unsigned)r.result==180u);
    assert(native_next_event(17)==NATIVE_EVENT_RESIZE);
    f.rax = OPENXP_SYS_GUI_TEXT;
    f.rsi = 22;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 0 && !strcmp(native_snapshot().text,"Hello from user memory"));
    f.rsi = 4096;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EINVAL);
    f.rax=OPENXP_SYS_GUI_DRAW;
    f.rdi=999;f.rsi=1;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EFAULT);
    f.rdi=1000;f.rsi=0;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EINVAL);
    f.rsi=NATIVE_DRAW_MAX+1;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EINVAL);
    native_push_event('Q');
    f.rax = OPENXP_SYS_GUI_EVENT;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 'Q');
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 0);
    f.rax=OPENXP_SYS_GUI_WAIT;
    f.rdi=UINT64_MAX;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EINVAL);
    f.rdi=0;
    native_push_event('W');
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result=='W');
    /* User-mode path operations copy the entire request and return bounded data. */
    f.rax=OPENXP_SYS_FS_OP;f.rsi=2000;f.rdi=APP_FS_WRITE;
    memset(&mock_file,0,sizeof mock_file);
    strcpy(mock_file.path,"home/syscall.txt");
    memcpy(mock_file.data,"TEST",4);mock_file.size=4;
    r=user_syscall_dispatch(&f);assert(r.result==0);
    f.rdi=APP_FS_READ;
    memset(mock_file.data,0,sizeof mock_file.data);
    r=user_syscall_dispatch(&f);
    assert(r.result==0 && mock_file.size==4 && !memcmp(mock_file.data,"TEST",4));
    f.rdi=APP_FS_RENAME;strcpy(mock_file.dest,"home/newname.txt");
    r=user_syscall_dispatch(&f);assert(r.result==0);
    f.rdi=APP_FS_COPY;strcpy(mock_file.path,"home/newname.txt");
    strcpy(mock_file.dest,"home/cloned.txt");
    r=user_syscall_dispatch(&f);assert(r.result==0);
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rdi=APP_FS_DELETE;strcpy(mock_file.path,"home/cloned.txt");
    r=user_syscall_dispatch(&f);assert(r.result==0);
    f.rdi=APP_FS_KIND;strcpy(mock_file.path,"home/newname.txt");
    r=user_syscall_dispatch(&f);assert(r.result==2);
    f.rdi=APP_FS_DELETE;r=user_syscall_dispatch(&f);assert(r.result==0);
    f.rdi=APP_FS_WRITE;strcpy(mock_file.path,"bin/hello");
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rdi=APP_FS_LIST;mock_file.index=0;
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EINVAL || r.result==0);
    f.rdi=APP_FS_READ;mock_file.path[0]='h';
    memset(mock_file.path,'A',sizeof mock_file.path);
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EINVAL);
    f.rax=OPENXP_SYS_APP_LAUNCH;
    memcpy(mock_user_memory,"bin/paint",9);
    f.rdi=1000;f.rsi=9;
    r=user_syscall_dispatch(&f);assert(r.result==0);
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EAGAIN);
    memcpy(mock_user_memory,"bin/fault",9);
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EPERM);
    char pending[48];assert(app_launch_take(pending,sizeof pending));
    assert(!strcmp(pending,"bin/paint"));
    f.rax = 0xffff;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_ENOSYS);
    /* A normal user task may not register as the trusted window server. */
    f.rax = OPENXP_SYS_IPC_LISTEN;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EPERM);
    f.rax = OPENXP_SYS_IPC_CONNECT;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EPIPE);
    f.rax = OPENXP_SYS_IPC_SEND; f.rdi=0; f.rsi=999; f.rdx=4;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EFAULT);
    f.rsi=1000;f.rdx=65;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EINVAL);
    /* Only an accepted channel owned by the authorized window server may
     * move/focus/close a connected client's *actual* native window. */
    f.rax=OPENXP_SYS_WM_DISPLAY;f.rdi=0;f.rsi=1000;
    f.rdx=sizeof(openxp_wm_display_cmd_t);
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    /* Untrusted apps cannot replace the desktop input policy or frame. */
    for(uint64_t forbidden=OPENXP_SYS_SHELL_EVENT;
        forbidden<=OPENXP_SYS_SHELL_WINDOW;++forbidden){
        f.rax=forbidden;
        r=user_syscall_dispatch(&f);
        assert(!r.exit && r.result==(uint64_t)-OPENXP_EPERM);
    }
    f.rax=OPENXP_SYS_SHELL_RASTER_ROW;
    f.rdi=1000;f.rsi=sizeof(oxp_shell_raster_row_t);
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rax=OPENXP_SYS_SHELL_RASTER_COMMIT;
    f.rdi=OXP_SHELL_MASK_BACKGROUND;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rax=OPENXP_SYS_NATIVE_INFO;
    f.rdi=0;f.rsi=2000;f.rdx=sizeof(oxp_native_info_t);
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rax=OPENXP_SYS_NATIVE_ROW;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rax=OPENXP_SYS_WM_DISPLAY;
    capipc_init();
    assert(capipc_grant_server(17) && capipc_listen(17)==CAPIPC_OK);
    /* The service is authorized but must still pass a valid, readable
     * 2056-byte scanline. An unprivileged or forged pointer never works. */
    f.rax=OPENXP_SYS_SHELL_RASTER_ROW;
    f.rdi=1000;f.rsi=sizeof(oxp_shell_raster_row_t);
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EFAULT);
    f.rax=OPENXP_SYS_SHELL_RASTER_COMMIT;
    f.rdi=OXP_SHELL_MASK_BACKGROUND;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EINVAL);
    f.rax=OPENXP_SYS_NATIVE_INFO;
    f.rdi=4;f.rsi=2000;f.rdx=sizeof(oxp_native_info_t);
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EINVAL);
    f.rdi=0;
    r=user_syscall_dispatch(&f);
    assert(r.result==0);
    oxp_native_info_t info;
    memcpy(&info,&mock_file,sizeof(info));
    assert(info.active==1 && info.generation!=0);
    f.rax=OPENXP_SYS_NATIVE_ROW;
    f.rdx=0;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EINVAL); /* not painted yet */
    native_draw_cmd_t sample={.op=NATIVE_DRAW_CLEAR,.color=0x00ff00};
    assert(native_draw(17,&sample,1));
    f.rax=OPENXP_SYS_NATIVE_ROW;
    r=user_syscall_dispatch(&f);
    assert(r.result==0);
    oxp_native_row_t row;
    memcpy(&row,&mock_file,sizeof(row));
    assert(row.slot==0 && row.y==0 && row.pixels[0]==0x07e0u);
    f.rdx=OXP_NATIVE_H;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EINVAL);
    f.rax=OPENXP_SYS_WM_DISPLAY;
    f.rsi=1000;
    uint64_t client_handle=0,server_handle=0;
    assert(capipc_connect(18,&client_handle)==CAPIPC_OK);
    assert(capipc_accept(17,&server_handle)==CAPIPC_OK);
    assert(native_open(18,"Client",6));
    openxp_wm_display_cmd_t cmd={OPENXP_WM_SET_RECT,90,60,240,180};
    memcpy(mock_user_memory,&cmd,sizeof(cmd));
    f.rdi=client_handle;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rdi=server_handle;f.rdx=19;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EINVAL);
    f.rdx=sizeof(cmd);
    r=user_syscall_dispatch(&f);
    assert(r.result==0 && native_snapshot_slot(1).wm_x==90 &&
           native_snapshot_slot(1).wm_y==60);
    cmd.action=OPENXP_WM_RAISE;
    memcpy(mock_user_memory,&cmd,sizeof(cmd));
    r=user_syscall_dispatch(&f);
    assert(r.result==0 && native_snapshot_slot(1).wm_focus_serial);
    assert(native_snapshot_slot(0).wm_geometry_serial==0);
    cmd.action=OPENXP_WM_REQUEST_CLOSE;
    memcpy(mock_user_memory,&cmd,sizeof(cmd));
    r=user_syscall_dispatch(&f);
    assert(r.result==0 && native_next_event(18)==NATIVE_EVENT_CLOSE);
    assert(capipc_close(18,client_handle)==CAPIPC_OK);
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rax = OPENXP_SYS_EXIT;
    f.rdi = 77;
    r = user_syscall_dispatch(&f);
    assert(r.exit && exit_status == 77 && !active && !native_snapshot().active);
    puts("PASS: syscall authorization, scalar ABI, exit, rejected requests");
    return 0;
}
