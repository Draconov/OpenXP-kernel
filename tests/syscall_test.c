#include "user/syscall.h"
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
static const char mock_user_memory[64] = "Hello from user memory";
static size_t output_len;
const address_space_t *userspace_current_space(void) { return active ? &mock_space : NULL; }
bool aspace_range_valid(const address_space_t *s, uint64_t ptr, size_t len, bool writable) {
    return s == &mock_space && !writable && ptr >= 1000 && len <= 64 && ptr-1000 <= 64-len;
}
bool aspace_copy_from_user(const address_space_t *s, void *dst, uint64_t ptr, size_t len) {
    if (!aspace_range_valid(s,ptr,len,false)) return false;
    memcpy(dst,mock_user_memory+(ptr-1000),len);
    return true;
}

uint64_t process_current_pid(void) { return active ? 17 : 0; }
bool userspace_active(void) { return active; }
void userspace_finish(uint64_t status) { exit_status = status; active = false; }
void serial_putc(char ch) { emitted = ch; if (output_len < sizeof(output)) output[output_len++] = ch; }
uint64_t timer_ticks(void) { return 1234; }
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
    f.rax = 0xffff;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_ENOSYS);
    f.rax = OPENXP_SYS_EXIT;
    f.rdi = 77;
    r = user_syscall_dispatch(&f);
    assert(r.exit && exit_status == 77 && !active);
    puts("PASS: syscall authorization, scalar ABI, exit, rejected requests");
    return 0;
}
