#include "user/userspace.h"
#include "user/syscall.h"
#include "fs/initrd.h"
#include "user/elf.h"
#include "mm/address_space.h"
#include "user/process.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "serial.h"
#include "sched/scheduler.h"
#include "arch/x86_64/timer.h"
#include "arch/x86_64/irq_state.h"
#include <stddef.h>
#include <stdint.h>

extern const uint8_t openxp_user_demo_start[], openxp_user_demo_end[];
extern const uint8_t openxp_user_fault_start[], openxp_user_fault_end[];
extern const uint8_t openxp_user_protection_start[], openxp_user_protection_end[];
extern const uint8_t openxp_user_preempt_start[], openxp_user_preempt_end[];
extern void *memcpy(void *, const void *, size_t);

#define CODE_OFFSET 0x100000ULL
#define STACK_OFFSET 0x110000ULL
#define STACK_PAGES 4u
#define STACK_GUARD_PAGES 1u
#define ELF_HEADER_BYTES 64u
#define ELF_PH_BYTES 56u
#define ELF_IMAGE_OFFSET (ELF_HEADER_BYTES + ELF_PH_BYTES)
#define IMAGE_LIMIT 4096u
static uint8_t elf_image[ELF_IMAGE_OFFSET + IMAGE_LIMIT];
/* Use ordinary sized stores rather than untrusted, unaligned casts. */
static void put16(uint8_t *b, size_t at, uint16_t n) {
    b[at] = (uint8_t)n; b[at+1] = (uint8_t)(n >> 8);
}
static void put32(uint8_t *b, size_t at, uint32_t n) {
    for (unsigned i=0;i<4;++i) b[at+i] = (uint8_t)(n >> (i*8));
}
static void put64(uint8_t *b, size_t at, uint64_t n) {
    for (unsigned i=0;i<8;++i) b[at+i] = (uint8_t)(n >> (i*8));
}
static bool make_demo_elf(const uint8_t *start, const uint8_t *end,
                          uint64_t va, size_t *image_size) {
    if ((uintptr_t)end <= (uintptr_t)start) return false;
    size_t size = (size_t)((uintptr_t)end - (uintptr_t)start);
    if (size == 0 || size > IMAGE_LIMIT) return false;
    for (unsigned i=0;i<ELF_IMAGE_OFFSET;++i) elf_image[i] = 0;
    elf_image[0] = 0x7f; elf_image[1] = 'E'; elf_image[2] = 'L'; elf_image[3] = 'F';
    elf_image[4] = 2; elf_image[5] = 1; elf_image[6] = 1;
    put16(elf_image,16,2);  /* ET_EXEC */
    put16(elf_image,18,62); /* EM_X86_64 */
    put32(elf_image,20,1);  /* EV_CURRENT */
    put64(elf_image,24,va);
    put64(elf_image,32,ELF_HEADER_BYTES);
    put16(elf_image,52,ELF_HEADER_BYTES);
    put16(elf_image,54,ELF_PH_BYTES);
    put16(elf_image,56,1);
    put32(elf_image,64,1); /* PT_LOAD */
    put32(elf_image,68,5); /* PF_R|PF_X */
    put64(elf_image,72,ELF_IMAGE_OFFSET);
    put64(elf_image,80,va);
    put64(elf_image,88,0);
    put64(elf_image,96,size);
    put64(elf_image,104,size);
    put64(elf_image,112,1); /* unaligned compact teaching image */
    memcpy(elf_image+ELF_IMAGE_OFFSET, start, size);
    *image_size = ELF_IMAGE_OFFSET+size;
    return true;
}

bool userspace_active(void) { return process_current_pid() != 0; }
const address_space_t *userspace_current_space(void) { return process_current_space(); }
void userspace_finish(uint64_t status) { process_finish(status); }
__attribute__((noreturn)) void userspace_fault(uint64_t vector, uint64_t address) {
    serial_puts("USER EXCEPTION pid="); serial_u64(process_current_pid());
    serial_puts(" vector="); serial_u64(vector);
    serial_puts(" address="); serial_hex64(address); serial_puts("\n");
    process_record_fault(vector, address);
    arch_user_return();
}

static bool execute_demo(unsigned mode, process_info_t *info) {
    const uint8_t *start = mode == 1 ? openxp_user_fault_start :
        (mode == 2 ? openxp_user_protection_start : openxp_user_demo_start);
    const uint8_t *end = mode == 1 ? openxp_user_fault_end :
        (mode == 2 ? openxp_user_protection_end : openxp_user_demo_end);
    size_t image_size;
    if (!make_demo_elf(start, end, vmm_user_base() + CODE_OFFSET, &image_size)) return false;
    uint64_t pid = process_create_elf(elf_image, image_size);
    if (!pid) return false;
    bool ok = process_run(pid, info);
    if (!process_reap(pid)) return false;
    return ok;
}

bool userspace_demo_run(unsigned mode) {
    if (mode > 2 || !vmm_user_base()) return false;
    /* This boot experiment runs on the boot thread with timer IRQs off. */
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0" : "=r"(flags));
    if (flags & (1ULL << 9)) return false;
    if (!process_init()) return false;

    uint64_t before = pmm_free_pages();
    process_info_t first = {0}, second = {0};
    serial_puts("OpenXP: launching process with private CR3, guarded RSP0\n");
    if (!execute_demo(mode, &first)) return false;
    bool success = mode == 1 ? first.state == PROCESS_FAULTED &&
                          first.status == 134 && first.fault_vector == 6 :
        mode == 2 ? first.state == PROCESS_FAULTED && first.status == 142 &&
                    first.fault_vector == 14 &&
                    first.fault_address == 0xffffffff80000000ULL :
                    first.state == PROCESS_EXITED && first.status == 0;
    if (!success || pmm_free_pages() != before || process_current_pid()) return false;
    if (mode == 0) {
        /* A distinct PID must be issued after freeing the first slot; the
         * process resources and guarded stack must return to their baseline. */
        if (!execute_demo(0, &second) || second.pid <= first.pid ||
            second.state != PROCESS_EXITED || second.status != 0 ||
            pmm_free_pages() != before) return false;
        serial_puts("OPENXP_PROCESS_PID_OK\nOPENXP_PROCESS_REAP_OK\n");
        serial_puts("OPENXP_PROCESS_STACK_OK\n");
        serial_puts("OPENXP_ELF64_OK\nOPENXP_PRIVATE_CR3_OK\nOPENXP_USERSPACE_OK\n");
    } else if (mode == 1) serial_puts("OPENXP_USER_FAULT_OK\n");
    else serial_puts("OPENXP_USER_PROTECTION_OK\n");
    return true;
}

/* Two independent embedded ELF64 instances run on distinct preemptible
 * kernel workers. This test cannot pass if IRQ0 fails to alternate between
 * the boot thread and the Ring 3 workers. It is deliberately QEMU-only. */
struct preempt_demo_worker {
    uint64_t pid;
    process_info_t result;
    bool complete, success;
};
static struct preempt_demo_worker preempt_demo_workers[2];
static void run_preempt_user(void *arg) {
    struct preempt_demo_worker *w = (struct preempt_demo_worker *)arg;
    uint64_t flags = irq_save_disable();
    w->success = process_run_preemptible(w->pid, &w->result);
    w->complete = true;
    irq_restore(flags);
}

bool userspace_preempt_demo_run(void) {
    if (irq_are_enabled() || !vmm_user_base() || process_current_pid()) return false;
    size_t image_size;
    if (!make_demo_elf(openxp_user_preempt_start, openxp_user_preempt_end,
                       vmm_user_base() + CODE_OFFSET, &image_size)) return false;
    const uint64_t before = pmm_free_pages();
    for (unsigned i=0;i<2;++i) {
        preempt_demo_workers[i] = (struct preempt_demo_worker){0};
        preempt_demo_workers[i].pid = process_create_elf(elf_image, image_size);
        if (!preempt_demo_workers[i].pid) return false;
    }
    uint64_t first = scheduler_create(run_preempt_user, &preempt_demo_workers[0]);
    uint64_t second = scheduler_create(run_preempt_user, &preempt_demo_workers[1]);
    if (!first || !second || first == second ||
        preempt_demo_workers[0].pid == preempt_demo_workers[1].pid) return false;
    uint64_t switches_before = scheduler_preempt_switch_count();
    uint64_t start_tick = timer_ticks();
    serial_puts("OpenXP: testing two preemptive CPL3 processes\n");
    timer_enable();
    process_info_t waited_a = {0}, waited_b = {0};
    /* The controller parks for PIT ticks while workers execute at CPL3.
     * A finished task's status stays available until the later reap. */
    bool waited = process_wait_ticks(preempt_demo_workers[0].pid, 120, &waited_a) &&
                  process_wait_ticks(preempt_demo_workers[1].pid, 120, &waited_b);
    while (scheduler_live_workers() && timer_ticks() - start_tick < 180)
        __asm__ volatile ("pause" ::: "memory");
    timer_disable();
    if (!waited || waited_a.completion_sequence == 0 ||
        waited_b.completion_sequence == 0 ||
        waited_a.completion_sequence == waited_b.completion_sequence ||
        scheduler_live_workers() || !preempt_demo_workers[0].complete ||
        !preempt_demo_workers[1].complete ||
        scheduler_preempt_switch_count() - switches_before < 4) return false;
    for (unsigned i = 0; i < 2; ++i) {
        struct preempt_demo_worker *w = &preempt_demo_workers[i];
        if (!w->success || w->result.state != PROCESS_EXITED ||
            w->result.status != 0 || w->result.pid != w->pid) return false;
    }
    if (scheduler_reap() != 2) return false;
    if (!process_reap(preempt_demo_workers[0].pid) ||
        !process_reap(preempt_demo_workers[1].pid) ||
        pmm_free_pages() != before) return false;
    serial_puts("OPENXP_USER_PREEMPT_OK\nOPENXP_USER_CR3_SWITCH_OK\nOPENXP_PROCESS_WAIT_OK\n");
    return true;
}

/* v1.4: launch genuine ELF64 images from a separately-built, read-only
 * initrd archive. A deliberately faulting application must not prevent a
 * later healthy application from starting. These launches are sequential
 * because QEMU-level preemptive CPL3 integration is still experimental. */
static bool run_initrd_file(const char *path, bool expect_fault) {
    const void *image = NULL;
    size_t length = 0;
    if (!initrd_lookup(path, &image, &length)) return false;
    uint64_t pid = process_create_elf(image, length);
    if (!pid) return false;
    process_info_t result = {0};
    bool ok = process_run(pid, &result);
    ok = ok && result.pid == pid && result.completion_sequence != 0 &&
        (expect_fault ? (result.state == PROCESS_FAULTED &&
                          result.fault_vector == 6 && result.status == 134) :
                        (result.state == PROCESS_EXITED && result.status == 0));
    if (!process_reap(pid)) return false;
    return ok;
}
bool userspace_initrd_demo_run(void) {
    if (irq_are_enabled() || !initrd_count() || process_current_pid()) return false;
    uint64_t before = pmm_free_pages();
    if (!run_initrd_file("bin/hello", false) || pmm_free_pages() != before)
        return false;
    if (!run_initrd_file("bin/fault", true) || pmm_free_pages() != before)
        return false;
    serial_puts("OPENXP_FAULT_ISOLATION_OK\n");
    if (!run_initrd_file("bin/hello", false) || pmm_free_pages() != before)
        return false;
    serial_puts("OPENXP_INITRD_ELF_OK\nOPENXP_INITRD_OK\n");
    return true;
}
