#include "diag/crash.h"
#include "arch/x86_64/interrupts.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static char output[8192];
static size_t used;
uint64_t scheduler_current_id(void) { return 42; }
static void append(const char *value) {
    size_t n = strlen(value);
    assert(used + n < sizeof(output));
    memcpy(output + used, value, n + 1);
    used += n;
}
void serial_puts(const char *value) { append(value); }
void serial_u64(uint64_t value) {
    char num[32]; (void)snprintf(num, sizeof(num), "%llu", (unsigned long long)value); append(num);
}
void serial_hex64(uint64_t value) {
    char num[32]; (void)snprintf(num, sizeof(num), "0x%016llx", (unsigned long long)value); append(num);
}
int main(void) {
    struct exception_frame frame = { .vector=14, .error=0x17,
        .rip=0x12345678, .cs=0x8, .rflags=0x202, .rax=0xfeedbeef };
    crash_report_exception(&frame, 0xcafeb000);
    assert(strstr(output, "vector=14 (page fault) thread=42"));
    assert(strstr(output, "cr2=0x00000000cafeb000"));
    assert(strstr(output, "protection write user instruction-fetch"));
    assert(strstr(output, "rax=0x00000000feedbeef"));
    assert(strstr(output, "OPENXP_CRASH_DUMP_OK"));
    used = 0; output[0] = '\0';
    frame.vector = 6;
    crash_report_exception(&frame, 0);
    assert(strstr(output, "invalid opcode"));
    assert(!strstr(output, "cr2="));
    used = 0; output[0] = '\0';
    crash_report_exception(NULL, 0);
    assert(strstr(output, "null frame"));
    puts("PASS: exception names, register diagnostics, page-fault decoding");
    return 0;
}
