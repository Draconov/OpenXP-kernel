#include "sched/scheduler.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/irq_state.h"
#ifndef OPENXP_SCHED_HOST_TEST
#include "arch/x86_64/gdt.h"
#include "mm/address_space.h"
#endif
#include <stddef.h>
#include <stdint.h>

/* The first slot is the boot thread. Its stack is managed by Limine.
 * Preemption is strictly an experimental single-core x86-64 feature.
 * The interrupt-entry GPR/iret context is deliberately separate from the
 * old cooperative ret context. We NEVER mix the two after changeover. */
#define SLOT_COUNT (SCHED_MAX_WORKERS + 1u)
#define STACK_STRIDE_PAGES (SCHED_STACK_PAGES + 2u)
#define WORKER_STACK_AREA_OFFSET (2ULL * 1024 * 1024)

enum state { UNUSED, READY, RUNNING, SLEEPING, BLOCKED, FINISHED };
struct thread {
    uint64_t id;
    uint64_t saved_sp;
    uint64_t wake_tick;
    uint64_t pages[SCHED_STACK_PAGES];
    uint64_t virtual_guard;
    uint64_t address_root, kernel_rsp0; /* CPU state for next interrupt return */
    thread_entry_fn entry;
    void *arg;
    enum state state;
};
static struct thread tasks[SLOT_COUNT];
static unsigned current_slot;
static bool initialized;
static uint64_t next_id, switches;
static volatile uint64_t preempt_switches;
static bool preemptive;
static uint64_t last_tick;
static uint64_t default_root, default_rsp0;
#ifdef OPENXP_SCHED_HOST_TEST
static uint64_t simulated_root, simulated_rsp0;
#endif


extern void arch_context_switch(uint64_t *old_sp, uint64_t new_sp);
/* Mutations of runnable state or worker memory are protected by IF=0.
 * There is only one supported CPU, so interrupt masking is sufficient for
 * this prototype; it MUST be revisited before SMP support. */
static bool local_irqs_disabled(void) { return !irq_are_enabled(); }

/* Always entered through 'ret' from arch_context_switch. We synthesize a
 * fake caller return slot to satisfy the SysV x86-64 stack ABI. */
__attribute__((noreturn)) static void thread_finished(void);
static void thread_trampoline(void) {
    struct thread *t = &tasks[current_slot];
    t->entry(t->arg);
    thread_finished();
}

#ifdef OPENXP_SCHED_HOST_TEST
/* Host tests use actual writable mock stacks, while still exercising the
 * real virtual mapping and physical frame acquisition/rollback interfaces. */
static uint8_t test_stacks[SCHED_MAX_WORKERS][SCHED_STACK_PAGES * VMM_PAGE_SIZE]
    __attribute__((aligned(16)));
static uint64_t stack_top(unsigned slot) {
    return (uint64_t)(uintptr_t)(test_stacks[slot - 1] + sizeof(test_stacks[slot - 1]));
}
#else
static uint64_t stack_top(unsigned slot) {
    return tasks[slot].virtual_guard + (SCHED_STACK_PAGES + 1ULL) * VMM_PAGE_SIZE;
}
#endif

bool scheduler_init(void) {
    if (initialized || !vmm_region_base()) return false;
    for (unsigned i = 0; i < SLOT_COUNT; ++i) tasks[i] = (struct thread){0};
    current_slot = 0;
    tasks[0].id = 1;
    tasks[0].state = RUNNING;
    next_id = 2;
    switches = 0;
    preempt_switches = 0;
    preemptive = false;
    last_tick = 0;
    /* CR3 and RSP0 are only captured when switching to IRQ preemption. */
    default_root = default_rsp0 = 0;
    initialized = true;
    return true;
}

uint64_t scheduler_create(thread_entry_fn entry, void *arg) {
    if (!initialized || !entry || next_id == UINT64_MAX ||
        !local_irqs_disabled()) return 0;
    unsigned slot = 1;
    while (slot < SLOT_COUNT && tasks[slot].state != UNUSED) ++slot;
    if (slot == SLOT_COUNT) return 0;
    struct thread *t = &tasks[slot];
    uint64_t region = vmm_region_base();
    if (!region || region > UINT64_MAX - WORKER_STACK_AREA_OFFSET -
            (uint64_t)SLOT_COUNT * STACK_STRIDE_PAGES * VMM_PAGE_SIZE) return 0;
    const uint64_t guard = region + WORKER_STACK_AREA_OFFSET +
        (uint64_t)(slot - 1u) * STACK_STRIDE_PAGES * VMM_PAGE_SIZE;
    if (vmm_query_page(guard, NULL, NULL) ||
        vmm_query_page(guard + (SCHED_STACK_PAGES + 1u) * VMM_PAGE_SIZE, NULL, NULL)) return 0;
    /* All-or-nothing mapping; do not publish READY until the stack works. */
    for (unsigned i = 0; i < SCHED_STACK_PAGES; ++i) {
        uint64_t va = guard + (i + 1ULL) * VMM_PAGE_SIZE;
        if (vmm_query_page(va, NULL, NULL)) goto rollback;
        uint64_t pa = pmm_alloc_zeroed_page();
        if (!pa) goto rollback;
        if (!vmm_map_page(va, pa, VMM_PAGE_WRITE)) {
            (void)pmm_free_page(pa);
            goto rollback;
        }
        t->pages[i] = pa;
    }
    t->virtual_guard = guard;
    uint64_t top = stack_top(slot);
    if (top & 15u) goto rollback;
    t->address_root = default_root;
    t->kernel_rsp0 = default_rsp0;
    if (preemptive) {
        /* IRQ frame must match irq.S and the *full* long-mode hardware
         * return frame: 15 GPRs, vector/error, RIP, CS, RFLAGS, RSP, SS.
         * Even when returning to CPL0, IRETQ pops RSP and SS. Omitting
         * those fields makes it read the upper unmapped stack guard page.
         * RSP==top-8 meets the SysV entry ABI. The fake return address
         * is a safety fallback and is not normally executed. */
        *(uint64_t *)(uintptr_t)(top - 8ULL) = (uint64_t)(uintptr_t)thread_finished;
        struct exception_frame *frame = (struct exception_frame *)(uintptr_t)
            (top - 8ULL - sizeof(struct exception_frame));
        *frame = (struct exception_frame){
            .vector = 32, .error = 0,
            .rip = (uint64_t)(uintptr_t)thread_trampoline,
            .cs = 0x08, .rflags = 0x202, /* reserved bit + IF */
            .rsp = top - 8ULL, .ss = 0x10 /* kernel data selector */
        };
        t->saved_sp = (uint64_t)(uintptr_t)frame;
    } else {
        /* Cooperative switch: r15, r14, r13, r12, rbp, rbx, ret RIP,
         * fake caller RIP. Restoring via ret -> SysV 8-mod-16 stack. */
        uint64_t *frame = (uint64_t *)(uintptr_t)(top - 8ULL * sizeof(uint64_t));
        for (unsigned i = 0; i < 6; ++i) frame[i] = 0;
        frame[6] = (uint64_t)(uintptr_t)thread_trampoline;
        frame[7] = (uint64_t)(uintptr_t)thread_finished;
        t->saved_sp = (uint64_t)(uintptr_t)frame;
    }
    t->entry = entry;
    t->arg = arg;
    t->id = next_id++;
    t->state = READY;
    return t->id;
rollback:
    for (unsigned i = 0; i < SCHED_STACK_PAGES; ++i) {
        if (t->pages[i]) {
            uint64_t va = guard + (i + 1ULL) * VMM_PAGE_SIZE;
            (void)vmm_unmap_page(va);
            (void)pmm_free_page(t->pages[i]);
            t->pages[i] = 0;
        }
    }
    return 0;
}

void scheduler_yield(void) {
    if (!initialized || preemptive || !local_irqs_disabled()) return;
    unsigned next = current_slot;
    /* Scan starting after current: FIFO round-robin over fixed slots. */
    for (unsigned step = 1; step < SLOT_COUNT; ++step) {
        unsigned candidate = (current_slot + step) % SLOT_COUNT;
        if (tasks[candidate].state == READY) {
            next = candidate;
            break;
        }
    }
    if (next == current_slot) return;
    struct thread *old = &tasks[current_slot];
    struct thread *new_thread = &tasks[next];
    if (old->state == RUNNING) old->state = READY;
    new_thread->state = RUNNING;
    current_slot = next;
    ++switches;
    arch_context_switch(&old->saved_sp, new_thread->saved_sp);
}

__attribute__((noreturn)) static void thread_finished(void) {
    /* A worker may not deallocate its own active stack. Mark dead, switch
     * away, and let the boot thread reclaim all of its pages later. */
    tasks[current_slot].state = FINISHED;
    if (preemptive) {
        /* Leave the CPU idle on this stack until the NEXT timer IRQ.
         * A boot thread with IF=0 can reap the stack after switching away. */
        for (;;) __asm__ volatile ("hlt" ::: "memory");
    }
    scheduler_yield();
    for (;;) __asm__ volatile ("cli; hlt");
}

bool scheduler_enable_preemption(void) {
    if (!initialized || preemptive || current_slot != 0 ||
        !local_irqs_disabled() || scheduler_live_workers() != 0) return false;
    for (unsigned i = 1; i < SLOT_COUNT; ++i)
        if (tasks[i].state != UNUSED) return false;
    /* One kernel root and RSP0 serve all ordinary kernel threads. */
#ifndef OPENXP_SCHED_HOST_TEST
    default_root = aspace_current_root();
    default_rsp0 = gdt_kernel_stack_top();
    if (!default_root || !default_rsp0) return false;
#else
    default_root = 0x8000;
    default_rsp0 = 0xffff8000000f0000ULL;
#endif
    tasks[0].address_root = default_root;
    tasks[0].kernel_rsp0 = default_rsp0;
#ifdef OPENXP_SCHED_HOST_TEST
    simulated_root = default_root;
    simulated_rsp0 = default_rsp0;
#endif
    /* Caller guarantees IF=0 before enabling the timer/PIC. */
    preemptive = true;
    preempt_switches = 0;
    return true;
}
/* Switch the two privileged CPU execution fields as one IRQ-masked
 * scheduler operation. Host mode simulates their values without privileged
 * instructions, so tests exercise both the bind and timer-switch paths. */
static bool switch_arch_memory(uint64_t new_root, uint64_t new_rsp0) {
#ifdef OPENXP_SCHED_HOST_TEST
    simulated_root = new_root;
    simulated_rsp0 = new_rsp0;
#else
    uint64_t previous_rsp0 = gdt_kernel_stack_top();
    if (!gdt_set_kernel_stack(new_rsp0)) return false;
    if (!aspace_restore(new_root)) {
        (void)gdt_set_kernel_stack(previous_rsp0);
        return false;
    }
#endif
    return true;
}
/* Bind the running kernel thread to a Ring 3 process. No page tables or
 * kernel stacks are freed while this binding remains in place. */
bool scheduler_bind_current_memory(uint64_t cr3, uint64_t rsp0) {
    if (!preemptive || !initialized || !local_irqs_disabled() ||
        !cr3 || (cr3 & 4095u) || !rsp0 || (rsp0 & 15u) ||
        tasks[current_slot].state != RUNNING) return false;
    if (!switch_arch_memory(cr3, rsp0)) return false;
    tasks[current_slot].address_root = cr3;
    tasks[current_slot].kernel_rsp0 = rsp0;
    return true;
}

bool scheduler_unbind_current_memory(void) {
    return scheduler_bind_current_memory(default_root, default_rsp0);
}

#ifdef OPENXP_SCHED_HOST_TEST
void scheduler_test_active_memory(uint64_t *root, uint64_t *rsp0) {
    if (root) *root = simulated_root;
    if (rsp0) *rsp0 = simulated_rsp0;
}
bool scheduler_test_memory(uint64_t id, uint64_t *root, uint64_t *rsp0) {
    for (unsigned i = 0; i < SLOT_COUNT; ++i)
        if (tasks[i].id == id) {
            if (root) *root = tasks[i].address_root;
            if (rsp0) *rsp0 = tasks[i].kernel_rsp0;
            return true;
        }
    return false;
}
#endif

/* A monotonic tick clock at a single CPU. Deadline comparisons are correct
 * across uint64_t wrap when the requested delay is < 2^63 ticks. */
static bool deadline_reached(uint64_t now, uint64_t deadline) {
    return (now - deadline) < (UINT64_C(1) << 63);
}
uint64_t scheduler_last_tick(void) { return last_tick; }

uint64_t scheduler_on_timer_interrupt(uint64_t interrupted_rsp, uint64_t tick) {
    last_tick = tick;
    if (!preemptive || !initialized || !interrupted_rsp) return interrupted_rsp;
    /* Wake sleepers BEFORE choosing the next thread. IRQ0 runs at IF=0. */
    for (unsigned i = 0; i < SLOT_COUNT; ++i)
        if (tasks[i].state == SLEEPING &&
            deadline_reached(tick, tasks[i].wake_tick))
            tasks[i].state = READY;

    unsigned next = current_slot;
    for (unsigned step = 1; step < SLOT_COUNT; ++step) {
        unsigned candidate = (current_slot + step) % SLOT_COUNT;
        if (tasks[candidate].state == READY) { next = candidate; break; }
    }
    struct thread *old = &tasks[current_slot];
    /* With no other runnable task, a sleeping/blocked current thread may
     * return only into its sti/hlt/cli park loop; it cannot run normal code. */
    if (next == current_slot) {
        if (old->state == READY) old->state = RUNNING;
        return interrupted_rsp;
    }
    old->saved_sp = interrupted_rsp;
    if (old->state == RUNNING) old->state = READY;
    /* The interrupt handler is still executing on a kernel stack shared by
     * both CR3 roots. Switch paging and ring-3 entry stack before iretq. */
    if (tasks[next].address_root != tasks[current_slot].address_root ||
        tasks[next].kernel_rsp0 != tasks[current_slot].kernel_rsp0) {
        if (!switch_arch_memory(tasks[next].address_root, tasks[next].kernel_rsp0)) {
#ifndef OPENXP_SCHED_HOST_TEST
            for (;;) __asm__ volatile ("cli; hlt" ::: "memory");
#else
            return interrupted_rsp;
#endif
        }
    }
    tasks[next].state = RUNNING;
    current_slot = next;
    ++switches;
    ++preempt_switches;
    return tasks[next].saved_sp;
}

/* Blocking is strictly forbidden from interrupt context, or with local IRQs
 * already disabled. sti;hlt executes atomically with respect to maskable IRQs
 * so the wakeup between the condition check and HLT cannot be lost. */
static void park_until_resumed(void) {
#ifndef OPENXP_SCHED_HOST_TEST
    while (tasks[current_slot].state == SLEEPING ||
           tasks[current_slot].state == BLOCKED)
        __asm__ volatile ("sti; hlt; cli" ::: "memory");
#endif
}

bool scheduler_sleep_current_locked(uint64_t delay) {
    if (!initialized || !preemptive || !local_irqs_disabled() || !delay ||
        delay >= (UINT64_C(1) << 63)) return false;
    struct thread *t = &tasks[current_slot];
    if (t->state != RUNNING) return false;
    t->wake_tick = last_tick + delay;
    t->state = SLEEPING;
    park_until_resumed();
    return true;
}

bool scheduler_sleep_ticks(uint64_t delay) {
    if (!initialized || !preemptive || !irq_are_enabled() ||
        delay >= (UINT64_C(1) << 63)) return false;
    if (!delay) return true;
    uint64_t flags = irq_save_disable();
    bool slept = scheduler_sleep_current_locked(delay);
    irq_restore(flags);
    return slept;
}

bool scheduler_park_current_locked(void) {
    if (!initialized || !preemptive || !local_irqs_disabled() ||
        tasks[current_slot].state != RUNNING) return false;
    tasks[current_slot].state = BLOCKED;
    park_until_resumed();
    return true;
}

bool scheduler_block_current(void) {
    if (!initialized || !preemptive || !irq_are_enabled()) return false;
    uint64_t flags = irq_save_disable();
    bool blocked = scheduler_park_current_locked();
    irq_restore(flags);
    return blocked;
}

bool scheduler_wake(uint64_t id) {
    if (!initialized || !id || !local_irqs_disabled()) return false;
    for (unsigned i = 0; i < SLOT_COUNT; ++i) {
        struct thread *t = &tasks[i];
        if (t->id != id) continue;
        if (t->state != BLOCKED && t->state != SLEEPING) return false;
        t->state = READY;
        return true;
    }
    return false;
}

uint64_t scheduler_preempt_switch_count(void) { return preempt_switches; }

unsigned scheduler_live_workers(void) {
    /* Stable snapshot: IRQ0 might finish a worker between loop iterations. */
    uint64_t flags = irq_save_disable();
    unsigned n = 0;
    if (initialized) for (unsigned i = 1; i < SLOT_COUNT; ++i)
        if (tasks[i].state == READY || tasks[i].state == RUNNING ||
            tasks[i].state == SLEEPING || tasks[i].state == BLOCKED) ++n;
    irq_restore(flags);
    return n;
}

unsigned scheduler_reap(void) {
    if (!initialized || current_slot != 0 || !local_irqs_disabled()) return 0;
    unsigned count = 0;
    for (unsigned slot = 1; slot < SLOT_COUNT; ++slot) {
        struct thread *t = &tasks[slot];
        if (t->state != FINISHED) continue;
        bool ok = true;
        for (unsigned i = 0; i < SCHED_STACK_PAGES; ++i) {
            if (!t->pages[i]) continue;
            uint64_t va = t->virtual_guard + (i + 1ULL) * VMM_PAGE_SIZE;
            if (!vmm_unmap_page(va)) { ok = false; continue; }
            if (!pmm_free_page(t->pages[i])) { ok = false; continue; }
            t->pages[i] = 0;
        }
        /* A failure is fatal to safe reuse; do not recycle a dirty slot. */
        if (!ok) continue;
        *t = (struct thread){0};
        ++count;
    }
    return count;
}
uint64_t scheduler_current_id(void) { return initialized ? tasks[current_slot].id : 0; }
uint64_t scheduler_switch_count(void) { return switches; }

#ifdef OPENXP_SCHED_HOST_TEST
void scheduler_test_finish_current(void) {
    if (initialized && current_slot != 0)
        tasks[current_slot].state = FINISHED;
}
#endif
