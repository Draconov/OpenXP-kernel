#include "user/process.h"
#include "user/elf.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define MOCK_BASE 0xffff900000000000ULL
#define MOCK_USER 0x10000000000ULL
#define SLOT_PAGES 48u

static uint64_t mapped[SLOT_PAGES];
static uint64_t physical_next = 0x1000;
static unsigned frames, fail_after;
static uint64_t root_next = 0x100000;
static uint64_t active_root = 0x8000;
static uint64_t gdt_top = 0xffff8000000f0000ULL;
static unsigned restore_calls, stack_changes;
static struct { uint64_t cr3; unsigned owned_frames; } roots[64];
static int demo_mode;
static bool irq_enabled;
static uint64_t mock_ticks;
static unsigned sleep_calls;

bool irq_are_enabled(void) { return irq_enabled; }
uint64_t irq_save_disable(void) {
    uint64_t flags = irq_enabled ? (1ULL << 9) : 0;
    irq_enabled = false;
    return flags;
}
void irq_restore(uint64_t flags) { irq_enabled = (flags & (1ULL << 9)) != 0; }
uint64_t timer_ticks(void) { return mock_ticks; }
bool scheduler_sleep_ticks(uint64_t ticks) {
    assert(irq_enabled && ticks == 1);
    mock_ticks += ticks;
    ++sleep_calls;
    return true;
}
uint64_t vmm_region_base(void) { return MOCK_BASE; }
uint64_t vmm_user_base(void) { return MOCK_USER; }
static size_t page_index(uint64_t va) {
    assert(va >= MOCK_BASE + 3ULL * 1024 * 1024);
    size_t index = (size_t)((va - (MOCK_BASE + 3ULL * 1024 * 1024)) / VMM_PAGE_SIZE);
    assert(index < SLOT_PAGES && (va & (VMM_PAGE_SIZE-1)) == 0);
    return index;
}
bool vmm_query_page(uint64_t va, uint64_t *pa, unsigned *flags) {
    uint64_t p = mapped[page_index(va)];
    if (!p) return false;
    if (pa) *pa = p;
    if (flags) *flags = VMM_PAGE_WRITE;
    return true;
}
bool vmm_map_page(uint64_t va, uint64_t pa, unsigned flags) {
    (void)flags;
    size_t idx = page_index(va);
    if (!pa || mapped[idx]) return false;
    mapped[idx] = pa;
    return true;
}
bool vmm_unmap_page(uint64_t va) {
    size_t idx = page_index(va);
    if (!mapped[idx]) return false;
    mapped[idx] = 0;
    return true;
}
uint64_t pmm_alloc_zeroed_page(void) {
    if (fail_after && frames >= fail_after) return 0;
    uint64_t r = physical_next;
    physical_next += VMM_PAGE_SIZE;
    ++frames;
    return r;
}
bool pmm_free_page(uint64_t pa) {
    assert(pa && frames);
    --frames;
    return true;
}
uint64_t gdt_kernel_stack_top(void) { return gdt_top; }
bool gdt_set_kernel_stack(uint64_t top) {
    if (!top || (top & 15)) return false;
    gdt_top = top;
    ++stack_changes;
    return true;
}
bool aspace_create(address_space_t *s) {
    if (!s || s->cr3) return false;
    s->cr3 = root_next;
    root_next += VMM_PAGE_SIZE;
    ++frames;
    unsigned n = (unsigned)(s->cr3 / VMM_PAGE_SIZE);
    (void)n;
    for (unsigned i=0; i<64; ++i) if (!roots[i].cr3) {
        roots[i].cr3 = s->cr3; roots[i].owned_frames = 1; break;
    }
    return true;
}
bool aspace_destroy(address_space_t *s) {
    if (!s || !s->cr3 || s->cr3 == active_root) return false;
    /* Account for all fully or partially mapped user frames and the root. */
    for (unsigned i=0; i<64; ++i) if (roots[i].cr3 == s->cr3) {
        assert(frames >= roots[i].owned_frames);
        frames -= roots[i].owned_frames;
        roots[i].cr3 = 0; roots[i].owned_frames = 0;
        break;
    }
    s->cr3 = 0;
    return true;
}
bool elf64_load(address_space_t *s, const void *image, size_t len, uint64_t *entry) {
    if (!s->cr3 || !image || !len || *(const char *)image == 'X') return false;
    ++frames; /* mock image page */
    for (unsigned i=0; i<64; ++i) if (roots[i].cr3 == s->cr3) ++roots[i].owned_frames;
    *entry = MOCK_USER + 0x100000;
    return true;
}
bool aspace_query(const address_space_t *s, uint64_t va, uint64_t *pa, unsigned *flags) {
    (void)s; (void)pa; (void)flags;
    /* All requested stack locations are initially empty. */
    assert(va >= MOCK_USER);
    return false;
}
bool aspace_map(address_space_t *s, uint64_t va, uint64_t pa, unsigned flags) {
    (void)va; (void)flags;
    if (!s || !s->cr3 || !pa) return false;
    for (unsigned i=0; i<64; ++i) if (roots[i].cr3 == s->cr3) ++roots[i].owned_frames;
    return true;
}
uint64_t aspace_current_root(void) { return active_root; }
uint64_t aspace_activate(const address_space_t *s) {
    if (!s || !s->cr3) return 0;
    uint64_t prev = active_root;
    active_root = s->cr3;
    return prev;
}
bool aspace_restore(uint64_t prev) {
    ++restore_calls;
    active_root = prev;
    return true;
}
void arch_user_enter(uint64_t ip, uint64_t sp, uint64_t *continuation, unsigned irq) {
    assert(continuation && !irq);
    *continuation = 0x1000;
    assert(process_current_resume_sp() == *continuation);
    assert(ip == MOCK_USER+0x100000);
    assert(sp > MOCK_USER && !(sp & 15));
    assert(process_current_pid() != 0);
    assert(process_current_space() && process_current_space()->cr3 == active_root);
    assert(gdt_top != 0xffff8000000f0000ULL);
    if (demo_mode) process_record_fault(14, 0xf00d);
    else process_finish(7);
}
int main(void) {
    assert(!process_create_elf("ok", 2));
    assert(process_init());
    assert(!process_init());
    assert(!process_get_info(5, NULL));
    uint64_t pids[PROCESS_MAX_SLOTS];
    for (unsigned i = 0; i < PROCESS_MAX_SLOTS; ++i) {
        pids[i] = process_create_elf("valid", 5);
        assert(pids[i] == (uint64_t)(i + 1));
        process_info_t info;
        assert(process_get_info(pids[i], &info) && info.state == PROCESS_READY);
        assert(info.completion_sequence == 0);
        assert(!process_state_terminal(info.state));
    }
    assert(!process_create_elf("full", 4));
    assert(frames == PROCESS_MAX_SLOTS * (4 + 1 + 1 + 4));
    assert(!process_reap(pids[0]));
    process_info_t pending = {0};
    irq_enabled = true;
    assert(!process_wait_ticks(pids[0], 3, &pending));
    assert(sleep_calls == 3 && mock_ticks == 3);
    assert(!process_wait_ticks(pids[0], 0, &pending));
    assert(!process_wait_ticks(0, 10, &pending));
    assert(!process_wait_ticks(pids[0], UINT64_MAX, &pending));
    irq_enabled = false;
    /* A mapped former guard must prevent entry, even with a valid PID. */
    mapped[0] = 0xdead000;
    assert(!process_run(pids[0], NULL));
    mapped[0] = 0;
    process_info_t result;
    for (unsigned i = 0; i < PROCESS_MAX_SLOTS; ++i) {
        demo_mode = (i & 1);
        assert(process_run(pids[i], &result));
        assert(result.pid == pids[i] && result.status == (demo_mode ? 142 : 7));
        assert(result.state == (demo_mode ? PROCESS_FAULTED : PROCESS_EXITED));
        assert(process_state_terminal(result.state) && result.completion_sequence == i + 1);
        irq_enabled = true;
        process_info_t waited = {0};
        assert(process_wait_ticks(pids[i], 1, &waited) && waited.pid == pids[i]);
        assert(waited.completion_sequence == result.completion_sequence);
        irq_enabled = false;
        assert(!demo_mode || (result.fault_vector == 14 && result.fault_address == 0xf00d));
        assert(!process_current_pid() && active_root == 0x8000);
        assert(gdt_top == 0xffff8000000f0000ULL);
        assert(!process_run(pids[i], NULL));
        assert(process_reap(pids[i]));
        assert(!process_get_info(pids[i], &result));
    }
    assert(frames == 0 && restore_calls == PROCESS_MAX_SLOTS &&
           stack_changes == 2 * PROCESS_MAX_SLOTS);
    for (unsigned i=0; i<SLOT_PAGES; ++i) assert(!mapped[i]);
    assert(process_create_elf("Xbad", 4) == 0 && frames == 0);
    fail_after = 2;
    assert(!process_create_elf("ok", 2) && frames == 0);
    fail_after = 0;
    uint64_t new_pid = process_create_elf("ok", 2);
    assert(new_pid > pids[PROCESS_MAX_SLOTS-1]);
    assert(process_run(new_pid, NULL) && process_reap(new_pid) && frames == 0);
    puts("PASS: process table, unique PIDs, guarded stacks, CR3, exit/fault and full reclamation");
    return 0;
}
