#include "sched/scheduler.h"
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
static void assert_no_leaks(void) {
    assert(active == 0);
    for (unsigned i = 0; i < MAX_MAPPINGS; ++i) assert(!mapped[i].va);
}
int main(void) {
    assert(scheduler_current_id() == 0);
    assert(!scheduler_create(never_called, NULL));
    assert(scheduler_init());
    assert(!scheduler_init());
    assert(scheduler_current_id() == 1);
    irq_test_set_enabled(true);
    assert(!scheduler_create(never_called, NULL));
    assert(scheduler_live_workers() == 0 && irq_are_enabled());
    assert(scheduler_reap() == 0);
    irq_test_set_enabled(false);
    assert(!scheduler_create(NULL, NULL));
    /* Force failure at every backing-frame or mapping step; verify rollback. */
    for (int failure = 1; failure <= 4; ++failure) {
        allocation_call = 0; fail_allocation_at = failure;
        assert(!scheduler_create(never_called, NULL));
        assert_no_leaks();
        fail_allocation_at = 0;
        map_call = 0; fail_map_at = failure;
        assert(!scheduler_create(never_called, NULL));
        assert_no_leaks();
        fail_map_at = 0;
    }
    uint64_t ids[SCHED_MAX_WORKERS];
    for (unsigned i = 0; i < SCHED_MAX_WORKERS; ++i) {
        ids[i] = scheduler_create(never_called, NULL);
        assert(ids[i] == i + 2);
    }
    assert(!scheduler_create(never_called, NULL));
    assert(active == (int)SCHED_MAX_WORKERS * SCHED_STACK_PAGES);
    assert(scheduler_live_workers() == SCHED_MAX_WORKERS);
    assert(!scheduler_reap());
    /* Host mock doesn't run stacks; it just tracks core round-robin state. */
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        for (unsigned i = 0; i < SCHED_MAX_WORKERS; ++i) {
            scheduler_yield();
            assert(scheduler_current_id() == ids[i]);
            assert(!scheduler_reap());
        }
        scheduler_yield();
        assert(scheduler_current_id() == 1);
    }
    assert(context_switches == 3u * (SCHED_MAX_WORKERS + 1u));
    assert(scheduler_switch_count() == context_switches);
    assert(next_saved_sp);
    /* Mark one worker finished and verify that only the boot thread reaps. */
    scheduler_yield();
    assert(scheduler_current_id() == ids[0]);
    scheduler_test_finish_current();
    scheduler_yield();
    assert(scheduler_live_workers() == SCHED_MAX_WORKERS - 1u);
    for (unsigned i = 0; i < SCHED_MAX_WORKERS + 1u && scheduler_current_id() != 1; ++i)
        scheduler_yield();
    assert(scheduler_current_id() == 1);
    assert(scheduler_reap() == 1);
    assert(active == (int)(SCHED_MAX_WORKERS - 1u) * SCHED_STACK_PAGES);
    /* Recycled slot must have a fresh monotonically increasing ID. */
    uint64_t fresh = scheduler_create(never_called, NULL);
    assert(fresh == SCHED_MAX_WORKERS + 2u);
    assert(active == (int)SCHED_MAX_WORKERS * SCHED_STACK_PAGES);
    /* Drain all workers through host-only completion marker. */
    for (unsigned round = 0; round < 2 * SCHED_MAX_WORKERS + 2; ++round) {
        scheduler_yield();
        if (scheduler_current_id() != 1) scheduler_test_finish_current();
        if (scheduler_current_id() == 1) break;
    }
    assert(scheduler_current_id() == 1);
    assert(scheduler_live_workers() == 0);
    assert(scheduler_reap() == SCHED_MAX_WORKERS);
    assert_no_leaks();
    unsigned count = (unsigned)context_switches;
    scheduler_yield();
    assert(context_switches == count);
    puts("PASS: guarded stack rollback, round-robin, safe reap, slot recycling");
    return 0;
}
