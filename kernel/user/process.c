#include "user/process.h"
#include "user/userspace.h"
#include "user/elf.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/irq_state.h"
#include "sched/scheduler.h"
#include "arch/x86_64/timer.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include <stdint.h>
#include <stddef.h>

#define PROCESS_STACK_PAGES 4u
#define PROCESS_STACK_GUARDS 2u
#define PROCESS_STACK_STRIDE (PROCESS_STACK_PAGES + PROCESS_STACK_GUARDS)
#define PROCESS_STACK_OFFSET (3ULL * 1024 * 1024)
#define USER_STACK_PAGES 4u
#define USER_STACK_OFFSET 0x110000ULL

struct process {
    process_info_t info;
    address_space_t space;
    uint64_t entry;
    uint64_t user_stack_top;
    uint64_t owner_thread, kernel_continuation;
    uint64_t kernel_stack_guard;
    uint64_t kernel_stack_pages[PROCESS_STACK_PAGES];
};
static struct process processes[PROCESS_MAX_SLOTS];
static uint64_t next_pid;
static uint64_t completion_counter;
static uint64_t current_thread_id(void) {
#ifdef OPENXP_PROCESS_HOST_TEST
    return 1;
#else
    return scheduler_current_id();
#endif
}
static struct process *running_here(void) {
    uint64_t id = current_thread_id();
    if (!id) return NULL;
    for (unsigned i = 0; i < PROCESS_MAX_SLOTS; ++i)
        if (processes[i].owner_thread == id && id &&
            processes[i].info.state == PROCESS_RUNNING) return &processes[i];
    return NULL;
}
static struct process *owned_here(void) {
    uint64_t id = current_thread_id();
    if (!id) return NULL;
    for (unsigned i = 0; i < PROCESS_MAX_SLOTS; ++i)
        if (processes[i].owner_thread == id && id) return &processes[i];
    return NULL;
}
uint64_t process_current_resume_sp(void) {
    struct process *p = owned_here();
    return p ? p->kernel_continuation : 0;
}
static bool initialized;

static bool interrupts_masked(void) { return !irq_are_enabled(); }
static struct process *lookup(uint64_t pid) {
    if (!pid) return NULL;
    for (unsigned i = 0; i < PROCESS_MAX_SLOTS; ++i)
        if (processes[i].info.pid == pid) return &processes[i];
    return NULL;
}
static uint64_t kernel_stack_guard(unsigned slot) {
    return vmm_region_base() + PROCESS_STACK_OFFSET +
           (uint64_t)slot * PROCESS_STACK_STRIDE * VMM_PAGE_SIZE;
}
static void drop_kernel_stack(struct process *p) {
    for (unsigned i = 0; i < PROCESS_STACK_PAGES; ++i)
        if (p->kernel_stack_pages[i]) {
            uint64_t va = p->kernel_stack_guard + (i + 1ULL) * VMM_PAGE_SIZE;
            (void)vmm_unmap_page(va);
            (void)pmm_free_page(p->kernel_stack_pages[i]);
            p->kernel_stack_pages[i] = 0;
        }
}
static bool make_kernel_stack(struct process *p, unsigned slot) {
    uint64_t base = vmm_region_base();
    if (!base || base > UINT64_MAX - PROCESS_STACK_OFFSET -
            PROCESS_MAX_SLOTS * PROCESS_STACK_STRIDE * VMM_PAGE_SIZE) return false;
    p->kernel_stack_guard = kernel_stack_guard(slot);
    uint64_t after = p->kernel_stack_guard +
        (PROCESS_STACK_PAGES + 1ULL) * VMM_PAGE_SIZE;
    if (vmm_query_page(p->kernel_stack_guard, NULL, NULL) ||
        vmm_query_page(after, NULL, NULL)) return false;
    for (unsigned i = 0; i < PROCESS_STACK_PAGES; ++i) {
        uint64_t va = p->kernel_stack_guard + (i + 1ULL) * VMM_PAGE_SIZE;
        if (vmm_query_page(va, NULL, NULL)) goto fail;
        uint64_t pa = pmm_alloc_zeroed_page();
        if (!pa) goto fail;
        if (!vmm_map_page(va, pa, VMM_PAGE_WRITE)) {
            (void)pmm_free_page(pa);
            goto fail;
        }
        p->kernel_stack_pages[i] = pa;
    }
    return true;
fail:
    drop_kernel_stack(p);
    return false;
}
/* Verify that each process's transition stack is still entirely mapped
 * writable+NX and that neither neighboring guard page is present. This
 * catches accidental kernel mapping damage before an iretq to Ring 3. */
static bool check_kernel_stack(const struct process *p) {
    if (!p->kernel_stack_guard ||
        vmm_query_page(p->kernel_stack_guard, NULL, NULL) ||
        vmm_query_page(p->kernel_stack_guard +
                       (PROCESS_STACK_PAGES + 1ULL) * VMM_PAGE_SIZE, NULL, NULL))
        return false;
    for (unsigned i = 0; i < PROCESS_STACK_PAGES; ++i) {
        uint64_t va = p->kernel_stack_guard + (i + 1ULL) * VMM_PAGE_SIZE;
        uint64_t pa = 0;
        unsigned flags = 0;
        if (!p->kernel_stack_pages[i] ||
            !vmm_query_page(va, &pa, &flags) ||
            pa != p->kernel_stack_pages[i] ||
            !(flags & VMM_PAGE_WRITE) || (flags & VMM_PAGE_EXEC)) return false;
    }
    return true;
}
static void discard(struct process *p) {
    if (p->space.cr3) (void)aspace_destroy(&p->space);
    drop_kernel_stack(p);
    *p = (struct process){0};
}
bool process_init(void) {
    if (initialized || !vmm_region_base() || !vmm_user_base() ||
        !interrupts_masked()) return false;
    for (unsigned i = 0; i < PROCESS_MAX_SLOTS; ++i)
        processes[i] = (struct process){0};
    next_pid = 1;
    completion_counter = 0;
    initialized = true;
    return true;
}
uint64_t process_create_elf(const void *image, size_t length) {
    if (!initialized || !interrupts_masked() || running_here() ||
        !image || !length || next_pid == UINT64_MAX) return 0;
    unsigned slot = 0;
    while (slot < PROCESS_MAX_SLOTS && processes[slot].info.state != PROCESS_EMPTY) ++slot;
    if (slot == PROCESS_MAX_SLOTS) return 0;
    struct process *p = &processes[slot];
    *p = (struct process){0};
    /* Install kernel stack before creating the private root, which inherits
     * the current supervisor-only kernel branch. */
    if (!make_kernel_stack(p, slot)) goto fail;
    if (!aspace_create(&p->space)) goto fail;
    if (!elf64_load(&p->space, image, length, &p->entry)) goto fail;
    const uint64_t base = vmm_user_base();
    if (!base || base > UINT64_MAX - USER_STACK_OFFSET -
                              (USER_STACK_PAGES + 1u) * VMM_PAGE_SIZE) goto fail;
    const uint64_t stack_start = base + USER_STACK_OFFSET + VMM_PAGE_SIZE;
    if (aspace_query(&p->space, stack_start - VMM_PAGE_SIZE, NULL, NULL) ||
        aspace_query(&p->space, stack_start + USER_STACK_PAGES * VMM_PAGE_SIZE, NULL, NULL)) goto fail;
    for (unsigned i = 0; i < USER_STACK_PAGES; ++i) {
        const uint64_t va = stack_start + (uint64_t)i * VMM_PAGE_SIZE;
        if (aspace_query(&p->space, va, NULL, NULL)) goto fail;
        uint64_t pa = pmm_alloc_zeroed_page();
        if (!pa) goto fail;
        if (!aspace_map(&p->space, va, pa, VMM_PAGE_WRITE)) {
            (void)pmm_free_page(pa);
            goto fail;
        }
    }
    p->user_stack_top = stack_start + USER_STACK_PAGES * VMM_PAGE_SIZE - 16;
    p->info = (process_info_t){ .pid = next_pid++, .state = PROCESS_READY };
    return p->info.pid;
fail:
    discard(p);
    return 0;
}
bool process_state_terminal(process_state_t s) {
    return s == PROCESS_EXITED || s == PROCESS_FAULTED;
}
/* A timer-backed wait, not a spin loop. The one-tick resolution is a
 * deliberate limitation until per-process wait queues are available.
 * Reads are protected by local IRQ masking on the single supported CPU. */
bool process_wait_ticks(uint64_t pid, uint64_t max_ticks, process_info_t *result) {
    if (!initialized || !pid || !result || !irq_are_enabled() ||
        max_ticks == 0 || max_ticks >= (UINT64_C(1) << 63) ||
        process_current_pid() != 0) return false;
    const uint64_t started = timer_ticks();
    for (;;) {
        uint64_t flags = irq_save_disable();
        process_info_t snapshot;
        bool found = process_get_info(pid, &snapshot);
        irq_restore(flags);
        if (!found) return false;
        if (process_state_terminal(snapshot.state)) {
            *result = snapshot;
            return true;
        }
        if ((uint64_t)(timer_ticks() - started) >= max_ticks) return false;
        if (!scheduler_sleep_ticks(1)) return false;
    }
}
bool process_get_info(uint64_t pid, process_info_t *info) {
    struct process *p = lookup(pid);
    if (!initialized || !p || !info || !interrupts_masked()) return false;
    *info = p->info;
    return true;
}
uint64_t process_current_pid(void) {
    struct process *p = running_here();
    return p ? p->info.pid : 0;
}
const address_space_t *process_current_space(void) {
    struct process *p = running_here();
    return p ? &p->space : NULL;
}
void process_finish(uint64_t status) {
    struct process *p = running_here();
    if (p) {
        p->info.status = status;
        p->info.state = PROCESS_EXITED;
        p->info.completion_sequence = ++completion_counter;
        if (!completion_counter) p->info.completion_sequence = ++completion_counter;
    }
}
void process_record_fault(uint64_t vector, uint64_t fault_address) {
    struct process *p = running_here();
    if (p) {
        p->info.status = 128 + vector;
        p->info.fault_vector = vector;
        p->info.fault_address = fault_address;
        p->info.state = PROCESS_FAULTED;
        p->info.completion_sequence = ++completion_counter;
        if (!completion_counter) p->info.completion_sequence = ++completion_counter;
    }
}
static bool run_process(uint64_t pid, process_info_t *result, bool preemptible) {
    struct process *p = lookup(pid);
    if (!initialized || !interrupts_masked() || running_here() || !p ||
        p->info.state != PROCESS_READY || owned_here() ||
        !check_kernel_stack(p)) return false;
    const uint64_t previous_rsp0 = gdt_kernel_stack_top();
    const uint64_t process_rsp0 = p->kernel_stack_guard +
        (PROCESS_STACK_PAGES + 1ULL) * VMM_PAGE_SIZE;
    const uint64_t kernel_cr3 = aspace_current_root();
    if (!kernel_cr3) return false;
    p->owner_thread = current_thread_id();
    if (!p->owner_thread) return false;
    if (preemptible) {
#ifndef OPENXP_PROCESS_HOST_TEST
        if (!scheduler_bind_current_memory(p->space.cr3, process_rsp0)) {
            p->owner_thread = 0;
            return false;
        }
#else
        p->owner_thread = 0;
        return false;
#endif
    } else {
        if (!gdt_set_kernel_stack(process_rsp0)) {
            p->owner_thread = 0;
            return false;
        }
        if (!aspace_activate(&p->space)) {
            (void)gdt_set_kernel_stack(previous_rsp0);
            p->owner_thread = 0;
            return false;
        }
    }
    p->info.state = PROCESS_RUNNING;
    arch_user_enter(p->entry, p->user_stack_top,
                    &p->kernel_continuation, preemptible ? 1u : 0u);
    /* The syscall or fault exit returns from Ring 3 with IF=0. The running
     * thread must restore both CPU roots BEFORE another timer IRQ. */
    if (preemptible) {
#ifndef OPENXP_PROCESS_HOST_TEST
        if (!scheduler_unbind_current_memory()) for (;;) __asm__ volatile ("cli; hlt");
#endif
    } else {
        if (!aspace_restore(kernel_cr3)) {
#ifndef OPENXP_PROCESS_HOST_TEST
            for (;;) __asm__ volatile ("cli; hlt");
#else
            return false;
#endif
        }
        if (!gdt_set_kernel_stack(previous_rsp0)) return false;
    }
    p->owner_thread = 0;
    p->kernel_continuation = 0;
    if (p->info.state == PROCESS_RUNNING) {
        p->info.state = PROCESS_FAULTED;
        p->info.completion_sequence = ++completion_counter;
        if (!completion_counter) p->info.completion_sequence = ++completion_counter;
        p->info.status = UINT64_MAX;
    }
    if (result) *result = p->info;
    return true;
}
bool process_run(uint64_t pid, process_info_t *result) {
    return run_process(pid, result, false);
}
bool process_run_preemptible(uint64_t pid, process_info_t *result) {
    return run_process(pid, result, true);
}
bool process_reap(uint64_t pid) {
    struct process *p = lookup(pid);
    if (!initialized || !interrupts_masked() || running_here() || !p ||
        p->owner_thread || (p->info.state != PROCESS_EXITED && p->info.state != PROCESS_FAULTED)) return false;
    /* A successful reap frees its PID, ELF image, user stack, page tables,
     * kernel transition stack and both unmapped kernel guard pages. */
    discard(p);
    return true;
}
