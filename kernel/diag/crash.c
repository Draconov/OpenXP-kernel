#include "diag/crash.h"
#include "arch/x86_64/interrupts.h"
#include "sched/scheduler.h"
#include "serial.h"
#include <stddef.h>

static const char *exception_name(uint64_t vector) {
    static const char *const names[32] = {
        "divide-by-zero", "debug", "non-maskable interrupt", "breakpoint",
        "overflow", "bounds", "invalid opcode", "device not available",
        "double fault", "coprocessor segment", "invalid TSS", "segment not present",
        "stack segment", "general protection", "page fault", "reserved",
        "x87 floating point", "alignment check", "machine check", "SIMD floating point",
        "virtualization", "control protection", "reserved", "reserved",
        "reserved", "reserved", "reserved", "reserved",
        "hypervisor injection", "VMM communication", "security", "reserved"
    };
    return vector < 32 ? names[vector] : "unexpected interrupt";
}
static void print_reg(const char *name, uint64_t value) {
    serial_puts(" "); serial_puts(name); serial_puts("="); serial_hex64(value);
}
void crash_report_exception(const struct exception_frame *frame, uint64_t cr2) {
    if (!frame) { serial_puts("\nOPENXP_FATAL_EXCEPTION: null frame\n"); return; }
    serial_puts("\nOPENXP_FATAL_EXCEPTION vector="); serial_u64(frame->vector);
    serial_puts(" ("); serial_puts(exception_name(frame->vector)); serial_puts(")");
    serial_puts(" thread="); serial_u64(scheduler_current_id());
    print_reg("error", frame->error);
    print_reg("rip", frame->rip);
    print_reg("cs", frame->cs);
    print_reg("rflags", frame->rflags);
    print_reg("rsp", frame->rsp);
    print_reg("ss", frame->ss);
    serial_puts("\n");
    if (frame->vector == 14) {
        print_reg("cr2", cr2);
        serial_puts(" page-fault:");
        serial_puts((frame->error & 1) ? " protection" : " not-present");
        serial_puts((frame->error & 2) ? " write" : " read");
        if (frame->error & 4) serial_puts(" user");
        if (frame->error & 8) serial_puts(" reserved-bit");
        if (frame->error & 16) serial_puts(" instruction-fetch");
        if (frame->error & 32) serial_puts(" protection-key");
        if (frame->error & 64) serial_puts(" shadow-stack");
        serial_puts("\n");
    }
    print_reg("rax", frame->rax); print_reg("rbx", frame->rbx);
    print_reg("rcx", frame->rcx); print_reg("rdx", frame->rdx);
    serial_puts("\n");
    print_reg("rsi", frame->rsi); print_reg("rdi", frame->rdi);
    print_reg("rbp", frame->rbp); print_reg("r8", frame->r8);
    serial_puts("\n");
    print_reg("r9", frame->r9); print_reg("r10", frame->r10);
    print_reg("r11", frame->r11); print_reg("r12", frame->r12);
    serial_puts("\n");
    print_reg("r13", frame->r13); print_reg("r14", frame->r14);
    print_reg("r15", frame->r15);
    serial_puts("\nOPENXP_CRASH_DUMP_OK\n");
}
