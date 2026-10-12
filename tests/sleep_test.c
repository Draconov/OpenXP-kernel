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
    assert(!scheduler_sleep_ticks(1)); /* timer IRQs not enabled */
    uint64_t a = scheduler_create(never_called, NULL);
    uint64_t b = scheduler_create(never_called, NULL);
    assert(a && b && scheduler_live_workers() == 2);
    irq_test_set_enabled(true);
    assert(!scheduler_sleep_ticks(UINT64_C(1) << 63));
    assert(scheduler_sleep_ticks(0));
    assert(scheduler_sleep_ticks(5)); /* Host simulation parks boot. */
    assert(scheduler_live_workers() == 2);
    uint64_t sp_a = scheduler_on_timer_interrupt(0x10000, 1);
    assert(scheduler_current_id() == a && sp_a != 0x10000);
    assert(scheduler_sleep_ticks(2)); /* Worker A sleeps until tick 3. */
    uint64_t sp_b = scheduler_on_timer_interrupt(sp_a, 2);
    assert(scheduler_current_id() == b && sp_b != sp_a);
    assert(scheduler_block_current()); /* B waits for a manual wakeup. */
    /* No READY threads: scheduler returns to current parked stack. */
    assert(scheduler_on_timer_interrupt(sp_b, 2) == sp_b);
    assert(scheduler_current_id() == b);
    uint64_t sp_resume_a = scheduler_on_timer_interrupt(sp_b, 3);
    assert(scheduler_current_id() == a && sp_resume_a == sp_a);
    assert(!scheduler_sleep_ticks(UINT64_MAX));
    irq_test_set_enabled(false);
    assert(!scheduler_wake(0));
    assert(!scheduler_wake(a));
    assert(scheduler_wake(b));
    assert(!scheduler_wake(b));
    irq_test_set_enabled(true);
    uint64_t sp_resume_b = scheduler_on_timer_interrupt(sp_resume_a, 4);
    assert(scheduler_current_id() == b && sp_resume_b == sp_b);
    uint64_t sp_boot = scheduler_on_timer_interrupt(sp_resume_b, 5);
    assert(sp_boot == 0x10000 && scheduler_current_id() == 1);
    assert(scheduler_last_tick() == 5);
    /* An explicit wake can resume a sleeper before its deadline. */
    assert(scheduler_sleep_ticks(30));
    irq_test_set_enabled(false);
    assert(scheduler_wake(1));
    irq_test_set_enabled(true);
    for (uint64_t tick = 6; tick < 12 && scheduler_current_id() != 1; ++tick) {
        uint64_t id = scheduler_current_id();
        uint64_t rsp = id == a ? sp_a : (id == b ? sp_b : sp_boot);
        (void)scheduler_on_timer_interrupt(rsp, tick);
    }
    assert(scheduler_current_id() == 1);
    /* Verify unsigned wraparound of the deadline clock. */
    (void)scheduler_on_timer_interrupt(sp_boot, UINT64_MAX - 2u);
    uint64_t sleeper = scheduler_current_id();
    assert(scheduler_sleep_ticks(5)); /* Deadline wraps to tick 2. */
    for (uint64_t i = 0; i < 4; ++i) {
        uint64_t id = scheduler_current_id();
        uint64_t rsp = id == a ? sp_a : (id == b ? sp_b : sp_boot);
        uint64_t tick = UINT64_MAX - 1u + i;
        (void)scheduler_on_timer_interrupt(rsp, tick);
        assert(scheduler_current_id() != sleeper);
    }
    bool resumed = false;
    for (uint64_t tick = 2; tick < 9; ++tick) {
        uint64_t id = scheduler_current_id();
        uint64_t rsp = id == a ? sp_a : (id == b ? sp_b : sp_boot);
        (void)scheduler_on_timer_interrupt(rsp, tick);
        if (scheduler_current_id() == sleeper) { resumed = true; break; }
    }
    assert(resumed);
    puts("PASS: timed sleep, manual block/wake, no-ready park, deadline wraparound");
    return 0;
}
