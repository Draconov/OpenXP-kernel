#include "sched/scheduler.h"
#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/irq_state.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_MAPPINGS 64
static struct { uint64_t va, pa; } mapped[MAX_MAPPINGS];
static bool frames[128];
static int active, allocation_call, fail_allocation_at, fail_map_at, map_call;
static uint64_t context_switches;
static uint64_t next_saved_sp;

uint64_t vmm_region_base(void) { return 0xffff990000000000ULL; }
bool vmm_query_page(uint64_t va, uint64_t *pa, unsigned *flags) {
    for (unsigned i = 0; i < MAX_MAPPINGS; ++i) {
        if (mapped[i].va == va) {
            if (pa) *pa = mapped[i].pa;
            if (flags) *flags = VMM_PAGE_WRITE;
            return true;
        }
    }
    return false;
}
uint64_t pmm_alloc_zeroed_page(void) {
    ++allocation_call;
    if (allocation_call == fail_allocation_at) return 0;
    for (unsigned i = 1; i < 128; ++i)
        if (!frames[i]) { frames[i] = true; ++active; return i * 4096ULL; }
    return 0;
}
bool pmm_free_page(uint64_t pa) {
    unsigned i = (unsigned)(pa / 4096);
    if (!pa || pa % 4096 || i >= 128 || !frames[i]) return false;
    frames[i] = false; --active; return true;
}
bool vmm_map_page(uint64_t va, uint64_t pa, unsigned flags) {
    assert(flags == VMM_PAGE_WRITE);
    assert((va & 4095) == 0);
    assert((pa & 4095) == 0);
    ++map_call;
    if (map_call == fail_map_at || vmm_query_page(va, NULL, NULL)) return false;
    for (unsigned i = 0; i < MAX_MAPPINGS; ++i) if (!mapped[i].va) {
        mapped[i].va = va; mapped[i].pa = pa; return true;
    }
    return false;
}
bool vmm_unmap_page(uint64_t va) {
    for (unsigned i = 0; i < MAX_MAPPINGS; ++i)
        if (mapped[i].va == va) { mapped[i].va = 0; mapped[i].pa = 0; return true; }
    return false;
}
void arch_context_switch(uint64_t *old_sp, uint64_t new_sp) {
    assert(new_sp && (new_sp & 15u) == 0);
    ++context_switches;
    next_saved_sp = new_sp;
    *old_sp = 0x1000;  /* fake boot/worker context: no stack movement */
}
static void never_called(void *arg) { (void)arg; assert(false); }
int main(void) {
    assert(scheduler_init());
    assert(scheduler_enable_preemption());
    assert(!scheduler_enable_preemption());
    assert(scheduler_on_timer_interrupt(0, 0) == 0);
    uint64_t a = scheduler_create(never_called, NULL);
    uint64_t b = scheduler_create(never_called, NULL);
    assert(a == 2 && b == 3);
    assert(scheduler_live_workers() == 2);
    assert(scheduler_reap() == 0); /* Unsafe while preemption is active. */
    assert(scheduler_current_id() == 1);
    assert(scheduler_switch_count() == 0);
    assert(scheduler_preempt_switch_count() == 0);
    uint64_t boot_rsp = 0x1234000;
    uint64_t a_rsp = scheduler_on_timer_interrupt(boot_rsp, 0);
    assert(a_rsp != boot_rsp && a_rsp != 0);
    assert(scheduler_current_id() == a);
    uint64_t root=0, rsp0=0;
    assert(scheduler_test_memory(a, &root, &rsp0));
    assert(root == 0x8000 && rsp0 == 0xffff8000000f0000ULL);
    assert(!scheduler_bind_current_memory(0x8001, 0xffff800000400000ULL));
    assert(scheduler_bind_current_memory(0x2000, 0xffff800000400000ULL));
    assert(scheduler_test_memory(a, &root, &rsp0));
    assert(root == 0x2000 && rsp0 == 0xffff800000400000ULL);
    scheduler_test_active_memory(&root, &rsp0);
    assert(root == 0x2000 && rsp0 == 0xffff800000400000ULL);
    struct exception_frame *first = (struct exception_frame *)(uintptr_t)a_rsp;
    assert(sizeof(*first) == 160);
    assert(first->vector == 32 && first->error == 0);
    assert(first->cs == 0x08 && first->rflags == 0x202);
    assert(first->rip != 0);
    assert(((uintptr_t)first + sizeof(*first)) % 16 == 8);
    uint64_t b_rsp = scheduler_on_timer_interrupt(a_rsp, 0);
    assert(b_rsp != a_rsp && scheduler_current_id() == b);
    assert(scheduler_test_memory(a, &root, &rsp0) && root == 0x2000);
    assert(scheduler_test_memory(b, &root, &rsp0) && root == 0x8000);
    scheduler_test_active_memory(&root, &rsp0);
    assert(root == 0x8000 && rsp0 == 0xffff8000000f0000ULL);
    uint64_t resumed_boot = scheduler_on_timer_interrupt(b_rsp, 0);
    assert(resumed_boot == boot_rsp && scheduler_current_id() == 1);
    assert(scheduler_unbind_current_memory());
    for (unsigned i = 0; i < 15; ++i)
        (void)scheduler_on_timer_interrupt(0x5555000 + i * 16, 0);
    assert(scheduler_preempt_switch_count() == 18);
    assert(scheduler_switch_count() == 18);
    assert(context_switches == 0); /* Never take the old ret-based path. */
    assert(active == (int)(2 * SCHED_STACK_PAGES));
    irq_test_set_enabled(true);
    assert(!scheduler_create(never_called, NULL));
    assert(scheduler_reap() == 0);
    assert(scheduler_live_workers() == 2 && irq_are_enabled());
    irq_test_set_enabled(false);
    /* Mark the first worker finished under timer scheduling. Running thread
     * must switch away; then the boot thread can safely reclaim its stack. */
    assert(scheduler_current_id() == 1);
    uint64_t to_a = scheduler_on_timer_interrupt(0x9876000, 0);
    assert(to_a != 0);
    assert(scheduler_current_id() == a);
    scheduler_test_finish_current();
    uint64_t to_b = scheduler_on_timer_interrupt(to_a, 0);
    assert(scheduler_current_id() == b && to_b != to_a);
    uint64_t to_boot = scheduler_on_timer_interrupt(to_b, 0);
    assert(to_boot == 0x9876000 && scheduler_current_id() == 1);
    assert(scheduler_live_workers() == 1);
    assert(scheduler_reap() == 1);
    assert(active == (int)SCHED_STACK_PAGES);
    uint64_t fresh = scheduler_create(never_called, NULL);
    assert(fresh == 4 && scheduler_live_workers() == 2);
    assert(active == (int)(2 * SCHED_STACK_PAGES));
    uint64_t to_fresh = scheduler_on_timer_interrupt(0x123000, 0);
    assert(to_fresh && scheduler_current_id() == fresh);
    scheduler_test_finish_current();
    (void)scheduler_on_timer_interrupt(to_fresh, 0);
    assert(scheduler_current_id() == b);
    scheduler_test_finish_current();
    (void)scheduler_on_timer_interrupt(to_b, 0);
    assert(scheduler_current_id() == 1);
    assert(scheduler_reap() == 2 && active == 0);
    assert(scheduler_live_workers() == 0);
    puts("PASS: IRQ frame layout, preemptive completion, reclamation, slot reuse");
    return 0;
}
