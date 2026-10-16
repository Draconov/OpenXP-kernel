/* OpenXP host test suite: kernel.
 * Select exactly one original test per compilation using -DOPENXP_TEST_NAME.
 * Individual test binaries and sanitizers remain unchanged. */
#if defined(OPENXP_TEST_CRASH)
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
#elif defined(OPENXP_TEST_IRQ_STATE)
#include "arch/x86_64/irq_state.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    irq_test_set_enabled(true);
    assert(irq_are_enabled());
    uint64_t outer = irq_save_disable();
    assert((outer & X86_RFLAGS_IF) && !irq_are_enabled());
    uint64_t inner = irq_save_disable();
    assert(!(inner & X86_RFLAGS_IF));
    irq_restore(inner);
    assert(!irq_are_enabled());
    irq_restore(outer);
    assert(irq_are_enabled());
    irq_test_set_enabled(false);
    irq_restore(irq_save_disable());
    assert(!irq_are_enabled());
    puts("PASS: single-CPU IF save/disable/restore and nested critical sections");
    return 0;
}
#elif defined(OPENXP_TEST_TIMER)
#include "arch/x86_64/timer.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

int main(void) {
    assert(timer_divisor_for_hz(0) == 0);
    assert(timer_divisor_for_hz(1) == 0); /* divisor too large */
    assert(timer_divisor_for_hz(18) == 0);
    assert(timer_divisor_for_hz(19) > 0);
    assert(timer_divisor_for_hz(100) == 11932);
    assert(timer_actual_hz(timer_divisor_for_hz(100)) == 100);
    assert(timer_divisor_for_hz(1000) == 1193);
    assert(timer_actual_hz(timer_divisor_for_hz(1000)) == 1000);
    assert(timer_divisor_for_hz(PIT_INPUT_HZ) == 1);
    assert(timer_divisor_for_hz(UINT32_MAX) == 0);
    assert(timer_actual_hz(0) == 0);
    uint16_t d = timer_divisor_for_hz(100);
    assert(timer_ticks_to_milliseconds(0, d) == 0);
    assert(timer_ticks_to_milliseconds(0, 0) == 0);
    assert(timer_ticks_to_milliseconds(100, d) >= 999);
    assert(timer_ticks_to_milliseconds(100, d) <= 1001);
    assert(timer_ticks_to_milliseconds(1000, d) >= 9999);
    assert(timer_ticks_to_milliseconds(UINT64_MAX, UINT16_MAX) == UINT64_MAX);
    assert(timer_ticks_to_milliseconds(UINT64_MAX, 1) > 0);
    puts("PASS: PIT divisor bounds, tick-to-ms monotonic conversion and saturation");
}
#elif defined(OPENXP_TEST_PCI)
#include "drivers/pci.h"
#include <assert.h>
#include <stdio.h>
static uint32_t mock(void *ctx,uint8_t bus,uint8_t slot,uint8_t fn,uint8_t reg){
    (void)ctx;
    if(bus==0 && slot==1 && fn==0){
        if(reg==0)return 0x12348086u;
        if(reg==8)return 0x06040000u;
        if(reg==0x18)return 0x00010100u; /* bridge -> bus 1 */
        if(reg==0x0c)return 0;
    }
    if(bus==1 && slot==2 && fn==0){
        if(reg==0)return 0x100e8086u;
        if(reg==8)return 0x02000000u; /* ethernet */
        if(reg==0x0c)return 0x00800000u; /* multifunction */
    }
    if(bus==1 && slot==2 && fn==1){
        if(reg==0)return 0x29348086u;
        if(reg==8)return 0x0c030000u; /* USB */
    }
    return 0xffffffffu;
}
static unsigned classes[256],visits;
static void collect(void *ctx,const pci_device_t *d){
    (void)ctx;assert(d->vendor==0x8086);
    ++visits;++classes[d->base_class];
}
int main(void){
    assert(pci_enumerate(0,0,collect,0)==0);
    assert(pci_enumerate(mock,0,collect,0)==3);
    assert(visits==3 && classes[6]==1 && classes[2]==1 && classes[12]==1);
    puts("PASS: read-only PCI bridge traversal and USB/network classification");
}
#elif defined(OPENXP_TEST_PMM)
/* Host-side memory manager tests using an HHDM backed by a RAM array. */
#include "mm/pmm.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>

alignas(4096) static uint8_t fake_ram[16 * 1024 * 1024];
int main(void) {
    /* 1 MiB of usable RAM, a reserved gap, then another 8 MiB. */
    struct limine_memmap_entry first = { .base = 0x1000, .length = 0x100000, .type = LIMINE_MEMMAP_USABLE };
    struct limine_memmap_entry gap = { .base = 0x101000, .length = 0x100000, .type = 2 };
    struct limine_memmap_entry second = { .base = 0x201000, .length = 0x800000, .type = LIMINE_MEMMAP_USABLE };
    struct limine_memmap_entry *entries[] = { &second, &gap, &first };
    struct limine_memmap_response map = { .revision = 0, .entry_count = 3, .entries = entries };
    assert(!pmm_init(NULL, (uint64_t)(uintptr_t)fake_ram));
    struct limine_memmap_entry conflict = { .base = 0x202000, .length = 0x10000, .type = LIMINE_MEMMAP_USABLE };
    struct limine_memmap_entry *overlap_entries[] = { &first, &second, &conflict };
    struct limine_memmap_response overlap_map = { .entry_count = 3, .entries = overlap_entries };
    assert(!pmm_init(&overlap_map, (uint64_t)(uintptr_t)fake_ram));
    memset(fake_ram, 0xcc, sizeof(fake_ram));
    assert(pmm_init(&map, (uint64_t)(uintptr_t)fake_ram));
    assert(pmm_total_pages() == 2304);
    uint64_t initial = pmm_free_pages();
    assert(initial < pmm_total_pages()); /* bitmap backing page(s) reserved */
    assert(!pmm_free_page(0x1000)); /* metadata is never handed out */
    assert(!pmm_free_page(0x101000)); /* reserved hole */
    assert(!pmm_free_page(0x201000)); /* never allocated */
    uint64_t a = pmm_alloc_zeroed_page();
    uint64_t b = pmm_alloc_zeroed_page();
    assert(a && b && a != b);
    assert((a & 4095) == 0 && (b & 4095) == 0);
    for (unsigned i = 0; i < 4096; ++i) assert(((uint8_t *)pmm_direct_map(a))[i] == 0);
    assert(pmm_free_pages() == initial - 2);
    assert(pmm_free_page(a));
    assert(!pmm_free_page(a));
    assert(!pmm_free_page(a + 1));
    assert(pmm_free_page(b));
    assert(pmm_free_pages() == initial);
    /* Exhaust allocator; no returned page may live in the reserved gap. */
    for (uint64_t i = 0; i < initial; ++i) {
        uint64_t page = pmm_alloc_page();
        assert(page && (page & 4095) == 0);
        assert((page >= 0x1000 && page < 0x101000) || (page >= 0x201000 && page < 0xa01000));
    }
    assert(pmm_free_pages() == 0);
    assert(pmm_alloc_page() == 0);
    puts("PASS: PMM allocation, zero-fill, region boundaries, metadata, double-free, exhaustion");
    return 0;
}
#elif defined(OPENXP_TEST_VMM)
/* Validate the page-table walker with a mocked direct physical memory map. */
#include "mm/vmm.h"
#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define MAX_FRAMES 512
alignas(4096) static uint8_t fake_ram[(MAX_FRAMES + 1) * 4096];
static bool allocated[MAX_FRAMES + 1];
static unsigned alive;
static unsigned allocation_calls;
static unsigned fail_on_call = UINT_MAX;

void *pmm_direct_map(uint64_t pa) {
    if (pa >= sizeof(fake_ram)) return NULL;
    return fake_ram + pa;
}
uint64_t pmm_alloc_zeroed_page(void) {
    if (allocation_calls++ == fail_on_call) return 0;
    for (unsigned i = 1; i <= MAX_FRAMES; ++i) {
        if (!allocated[i]) {
            allocated[i] = true;
            ++alive;
            memset(fake_ram + i * 4096, 0, 4096);
            return i * 4096ULL;
        }
    }
    return 0;
}
bool pmm_free_page(uint64_t pa) {
    if (!pa || (pa & 4095) || pa / 4096 > MAX_FRAMES || !allocated[pa / 4096]) return false;
    allocated[pa / 4096] = false;
    --alive;
    return true;
}
int main(void) {
    /* Create a mock boot hierarchy containing a 1-GiB huge page and a
     * normal 4-KiB leaf; clones must own all table pages, but not data. */
    uint64_t boot_root = pmm_alloc_zeroed_page();
    uint64_t pdpt = pmm_alloc_zeroed_page();
    uint64_t pd = pmm_alloc_zeroed_page();
    uint64_t pt = pmm_alloc_zeroed_page();
    uint64_t *r = pmm_direct_map(boot_root);
    uint64_t *a0 = pmm_direct_map(pdpt);
    uint64_t *a1 = pmm_direct_map(pd);
    uint64_t *a2 = pmm_direct_map(pt);
    r[300] = pdpt | 3;
    a0[2] = 0x40000000ULL | 0x83; /* huge: 1 GiB */
    a0[3] = pd | 3;
    a1[4] = 0x200000ULL | 0x83; /* huge: 2 MiB */
    a1[5] = pt | 3;
    a2[6] = 0x800000ULL | 0x8000000000000003ULL; /* NX leaf */
    unsigned before_clone = alive;
    uint64_t clone = 0;
    assert(vmm_test_clone_root(boot_root, &clone));
    uint64_t *cr = pmm_direct_map(clone);
    assert(clone != boot_root && (cr[300] & ~4095ULL) != pdpt);
    uint64_t *cpdpt = pmm_direct_map(cr[300] & 0x000ffffffffff000ULL);
    assert(cpdpt[2] == a0[2]);
    assert((cpdpt[3] & ~4095ULL) != pd);
    uint64_t *cpd = pmm_direct_map(cpdpt[3] & 0x000ffffffffff000ULL);
    assert(cpd[4] == a1[4]);
    assert((cpd[5] & ~4095ULL) != pt);
    uint64_t *cpt = pmm_direct_map(cpd[5] & 0x000ffffffffff000ULL);
    assert(cpt[6] == a2[6]);
    vmm_test_destroy_clone(clone);
    assert(alive == before_clone);
    /* Force a failed recursive clone after allocating several tables. */
    fail_on_call = allocation_calls + 2;
    assert(!vmm_test_clone_root(boot_root, &clone));
    assert(alive == before_clone); /* transactional rollback */
    fail_on_call = UINT_MAX;
    assert(pmm_free_page(pt) && pmm_free_page(pd) && pmm_free_page(pdpt) && pmm_free_page(boot_root));

    uint64_t root = pmm_alloc_zeroed_page();
    assert(root == 0x1000);
    assert(!vmm_test_attach(root, 255));
    assert(!vmm_test_attach(root, 511));
    assert(vmm_test_attach(root, 320));
    assert(!vmm_test_attach(root, 320));
    uint64_t base = vmm_region_base();
    uint64_t pa = pmm_alloc_zeroed_page();
    uint64_t other = pmm_alloc_zeroed_page();
    assert(!vmm_map_page(base + 1, pa, VMM_PAGE_WRITE));
    assert(!vmm_map_page(base + 4096, pa + 1, VMM_PAGE_WRITE));
    assert(!vmm_map_page(base + 4096, pa, 0x80));
    assert(!vmm_map_page(base + (1ULL << 39), pa, 0));
    assert(!vmm_map_page(base + 4096, 0, 0));
    assert(vmm_map_page(base + 4096, pa, VMM_PAGE_WRITE));
    assert(!vmm_map_page(base + 4096, pa, VMM_PAGE_WRITE));
    assert(vmm_map_page(base + 8192, other, VMM_PAGE_EXEC));
    uint64_t result = 0;
    unsigned flags = 0;
    assert(vmm_query_page(base + 4096, &result, &flags));
    assert(result == pa && flags == VMM_PAGE_WRITE);
    assert(vmm_query_page(base + 8192, &result, &flags));
    assert(result == other && flags == VMM_PAGE_EXEC);
    assert(!vmm_query_page(base, NULL, NULL)); /* simulated guard page */
    assert(!vmm_query_page(base + 12288, NULL, NULL));
    assert(vmm_unmap_page(base + 4096));
    assert(!vmm_unmap_page(base + 4096));
    assert(!vmm_query_page(base + 4096, NULL, NULL));
    assert(vmm_unmap_page(base + 8192));
    assert(!vmm_query_page(base + 8192, NULL, NULL));
    assert(alive == 3); /* root + original data frames; page tables reclaimed */
    assert(vmm_map_page(base + 4096, other, VMM_PAGE_WRITE));
    assert(vmm_unmap_page(base + 4096));
    assert(alive == 3);
    /* Page-table creation failure must not leak partially allocated tables. */
    fail_on_call = allocation_calls + 1;
    assert(!vmm_map_page(base + (1ULL << 30) + 4096, other, 0));
    assert(alive == 3);
    fail_on_call = UINT_MAX;
    /* CPL3 user-page mappings must have U/S at every paging level,
     * enforce W^X, and never expose existing supervisor memory. */
    assert(vmm_user_init());
    uint64_t user_base = vmm_user_base();
    assert(user_base && user_base < (1ULL << 47));
    assert(!vmm_user_init());
    assert(!vmm_user_query_page(user_base + 0x100000, NULL, NULL));
    assert(!vmm_user_map_page(user_base + 0x100000, pa, VMM_PAGE_WRITE | VMM_PAGE_EXEC));
    assert(!vmm_user_map_page(user_base + 0x100001, pa, VMM_PAGE_EXEC));
    assert(!vmm_user_map_page(base + 0x100000, pa, VMM_PAGE_EXEC));
    assert(!vmm_user_map_page(user_base + 0x400000, pa, VMM_PAGE_EXEC));
    assert(vmm_user_map_page(user_base + 0x100000, pa, VMM_PAGE_EXEC));
    assert(!vmm_user_map_page(user_base + 0x100000, pa, VMM_PAGE_EXEC));
    assert(vmm_user_query_page(user_base + 0x100000, &result, &flags));
    assert(result == pa && flags == VMM_PAGE_EXEC);
    uint64_t *user_root = pmm_direct_map(root);
    uint64_t pml4e = user_root[user_base >> 39];
    assert((pml4e & 7) == 7); /* present, writable, user */
    uint64_t *user_pdpt = pmm_direct_map(pml4e & 0x000ffffffffff000ULL);
    uint64_t pdpte = user_pdpt[(0x100000 >> 30) & 511];
    assert((pdpte & 7) == 7);
    uint64_t *user_pd = pmm_direct_map(pdpte & 0x000ffffffffff000ULL);
    uint64_t pde = user_pd[(0x100000 >> 21) & 511];
    assert((pde & 7) == 7);
    uint64_t *user_pt = pmm_direct_map(pde & 0x000ffffffffff000ULL);
    assert((user_pt[0x100000 >> 12] & 7) == 5); /* RX, non-writable */
    assert(vmm_user_map_page(user_base + 0x110000, other, VMM_PAGE_WRITE));
    assert(vmm_user_query_page(user_base + 0x110000, &result, &flags));
    assert(result == other && flags == VMM_PAGE_WRITE);
    assert(!vmm_user_query_page(user_base + 0x10f000, NULL, NULL)); /* guard */
    assert(vmm_user_unmap_page(user_base + 0x100000));
    assert(vmm_user_unmap_page(user_base + 0x110000));
    assert(!vmm_user_unmap_page(user_base + 0x100000));
    assert(alive == 3); /* page-table frames reclaimed */
    fail_on_call = allocation_calls + 1;
    assert(!vmm_user_map_page(user_base + 0x100000, pa, VMM_PAGE_EXEC));
    assert(alive == 3); /* rollback */
    fail_on_call = UINT_MAX;
    puts("PASS: VMM mapping, deep clones, huge pages, rollback, guards, table reclamation");
    return 0;
}
#elif defined(OPENXP_TEST_HEAP)
/* Reusable allocator tests on an aligned simulated region with guard bytes. */
#include "mm/heap.h"
#include <assert.h>
#include <stdint.h>
#include <stdalign.h>
#include <stdio.h>
#include <string.h>

alignas(4096) static uint8_t fake_ram[10 * 4096];
int main(void) {
    assert(kmalloc(32) == NULL);
    assert(!kfree((void *)(uintptr_t)0x1234));
    assert(!heap_init(NULL, 4096));
    assert(!heap_init(fake_ram + 4096, 100));
    assert(heap_init(fake_ram + 4096, 8 * 4096));
    assert(!heap_init(fake_ram + 4096, 4096));
    assert(heap_guard_low() == (uintptr_t)fake_ram);
    assert(heap_guard_high() == (uintptr_t)(fake_ram + 9 * 4096));
    assert(heap_capacity_bytes() == 8 * 4096);
    assert(heap_free_bytes() > 7 * 4096);
    assert(heap_used_bytes() == 0);
    assert(kfree(NULL));
    uint8_t *a = kmalloc(13), *b = kmalloc_aligned(35, 64);
    assert(a && b);
    assert((uintptr_t)a % 16 == 0 && (uintptr_t)b % 64 == 0);
    assert((uintptr_t)b >= (uintptr_t)a + 13);
    assert(!kmalloc(0));
    assert(!kmalloc_aligned(12, 3));
    assert(!kmalloc_aligned(12, 8192));
    assert(!kcalloc(SIZE_MAX, 2));
    uint32_t *zeros = kcalloc(10, sizeof(uint32_t));
    assert(zeros);
    for (unsigned i = 0; i < 10; ++i) assert(zeros[i] == 0);
    assert(!kfree(a + 1));
    assert(!kfree(fake_ram));
    assert(heap_used_bytes() == 13 + 35 + 40);
    assert(kfree(b));
    assert(!kfree(b));
    /* An aligned allocation reuses the freed address exactly. */
    uint8_t *reuse = kmalloc_aligned(32, 64);
    assert(reuse == b);
    assert(kfree(reuse));
    for (size_t i = 0; i < 13; ++i) a[i] = (uint8_t)(i + 20);
    uint8_t *grown = krealloc(a, 4000);
    assert(grown);
    for (size_t i = 0; i < 13; ++i) assert(grown[i] == i + 20);
    assert(!krealloc(grown + 1, 32));
    assert(krealloc(grown, SIZE_MAX) == NULL);
    for (size_t i = 0; i < 13; ++i) assert(grown[i] == i + 20);
    grown = krealloc(grown, 8);
    assert(grown);
    assert(krealloc(grown, 0) == NULL);
    assert(kfree(zeros));
    assert(heap_used_bytes() == 0);
    assert(heap_free_bytes() == heap_capacity_bytes() - 48); /* one header */
    /* Full coalescing: near-whole-heap allocation succeeds after fragmentation. */
    uint8_t *whole = kmalloc(heap_capacity_bytes() - 48 - 16);
    assert(whole);
    assert(!kmalloc(128));
    assert(kfree(whole));
    assert(heap_used_bytes() == 0);
    assert(heap_free_bytes() == heap_capacity_bytes() - 48);
    /* Lots of tiny blocks and frees in reverse order, no leaks. */
    void *objects[240];
    for (unsigned i = 0; i < 240; ++i) {
        objects[i] = kmalloc(15 + i % 53);
        assert(objects[i]);
    }
    for (unsigned i = 240; i > 0; --i) assert(kfree(objects[i - 1]));
    assert(heap_free_bytes() == heap_capacity_bytes() - 48);
    assert(fake_ram[0] == 0 && fake_ram[9 * 4096] == 0);
    puts("PASS: heap free/reuse, alignment, realloc, coalescing, guards, invalid frees, exhaustion");
    return 0;
}
#elif defined(OPENXP_TEST_HEAP_STRESS)
/* Deterministic randomized heap exercise (allocation, alignment, realloc,
 * freeing and collision checks). No external test framework required. */
#include "mm/heap.h"
#include <assert.h>
#include <stdint.h>
#include <stdalign.h>
#include <stdio.h>

#define SLOT_COUNT 96
#define STEPS 12000
alignas(4096) static uint8_t fake_ram[34 * 4096];
struct slot { uint8_t *data; size_t length, alignment; uint8_t pattern; };
static struct slot slots[SLOT_COUNT];
static uint32_t rng = 0x5a728b1f;
static uint32_t next_random(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static void verify(const struct slot *s) {
    if (!s->data) return;
    for (size_t j = 0; j < s->length; ++j) assert(s->data[j] == s->pattern);
}
static void fill(struct slot *s) {
    for (size_t j = 0; j < s->length; ++j) s->data[j] = s->pattern;
}
static void nonoverlap(void) {
    for (unsigned i = 0; i < SLOT_COUNT; ++i) {
        if (!slots[i].data) continue;
        uintptr_t left = (uintptr_t)slots[i].data;
        uintptr_t right = left + slots[i].length;
        for (unsigned j = i + 1; j < SLOT_COUNT; ++j) {
            if (!slots[j].data) continue;
            uintptr_t other = (uintptr_t)slots[j].data;
            assert(right <= other || other + slots[j].length <= left);
        }
    }
}
int main(void) {
    assert(heap_init(fake_ram + 4096, 32 * 4096));
    for (unsigned step = 0; step < STEPS; ++step) {
        struct slot *s = &slots[next_random() % SLOT_COUNT];
        uint32_t choice = next_random() % 4;
        if (s->data) {
            verify(s);
            if (choice <= 1) {
                assert(kfree(s->data));
                s->data = NULL;
                s->length = 0;
            } else {
                size_t target = 1 + (next_random() % 3000);
                uint8_t *new_data = krealloc(s->data, target);
                if (new_data) {
                    for (size_t j = 0; j < (target < s->length ? target : s->length); ++j)
                        assert(new_data[j] == s->pattern);
                    s->data = new_data;
                    s->length = target;
                    s->alignment = 16;
                    s->pattern = (uint8_t)(step % 251 + 1);
                    fill(s);
                } else {
                    verify(s); /* failed realloc preserved the original */
                }
            }
        } else {
            size_t alignment = (size_t)16 << (next_random() % 5);
            size_t length = 1 + next_random() % 2048;
            uint8_t *new_data = kmalloc_aligned(length, alignment);
            if (new_data) {
                assert(((uintptr_t)new_data & (alignment - 1)) == 0);
                s->data = new_data;
                s->length = length;
                s->alignment = alignment;
                s->pattern = (uint8_t)(step % 251 + 1);
                fill(s);
            }
        }
        if (step % 29 == 0) nonoverlap();
    }
    nonoverlap();
    for (unsigned i = 0; i < SLOT_COUNT; ++i) {
        verify(&slots[i]);
        if (slots[i].data) assert(kfree(slots[i].data));
    }
    assert(heap_used_bytes() == 0);
    assert(heap_free_bytes() == 32 * 4096 - 48);
    assert(fake_ram[0] == 0 && fake_ram[33 * 4096] == 0);
    puts("PASS: 12000 deterministic heap operations, no overlap, data preservation, full reclamation");
    return 0;
}
#elif defined(OPENXP_TEST_STRING)
/* Test OpenXP's freestanding memory primitives against expected byte arrays. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    unsigned char a[64];
    memset(a, 0x5a, sizeof(a));
    for (unsigned i = 0; i < sizeof(a); ++i) assert(a[i] == 0x5a);
    unsigned char b[64];
    memcpy(b, a, sizeof(b));
    for (unsigned i = 0; i < sizeof(b); ++i) assert(b[i] == 0x5a);
    for (unsigned i = 0; i < sizeof(a); ++i) a[i] = (unsigned char)i;
    memmove(a + 4, a, 40);
    for (unsigned i = 0; i < 40; ++i) assert(a[4 + i] == i);
    memmove(a, a + 4, 40);
    for (unsigned i = 0; i < 40; ++i) assert(a[i] == i);
    assert(memmove(a, a, sizeof(a)) == a);
    puts("PASS: freestanding memset, memcpy, memmove, overlapping copies");
    return 0;
}
#elif defined(OPENXP_TEST_SYNC)
/* POSIX-threaded host harness: simulates one IRQ-masked CPU's atomic
 * condition checks and sleeping. Tests the real kernel sync/IPC C sources. */
#include "sync/wait_queue.h"
#include "sync/event.h"
#include "sync/mutex.h"
#include "sync/ipc.h"
#include "arch/x86_64/irq_state.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdint.h>

#define TEST_THREADS 7
static pthread_mutex_t kernel_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sleeping[TEST_THREADS + 1];
static bool blocked[TEST_THREADS + 1];
static bool woken[TEST_THREADS + 1];
static _Thread_local uint64_t current_id = 1;
static _Thread_local bool if_enabled = true;
static void as_thread(uint64_t id) { assert(id && id <= TEST_THREADS); current_id = id; if_enabled = true; }
bool irq_are_enabled(void) { return if_enabled; }
uint64_t irq_save_disable(void) {
    uint64_t old = if_enabled ? X86_RFLAGS_IF : 0;
    if (if_enabled) { pthread_mutex_lock(&kernel_lock); if_enabled = false; }
    return old;
}
void irq_restore(uint64_t flags) {
    if (flags & X86_RFLAGS_IF) { assert(!if_enabled); if_enabled = true; pthread_mutex_unlock(&kernel_lock); }
    else assert(!if_enabled);
}
uint64_t scheduler_current_id(void) { return current_id; }
bool scheduler_park_current_locked(void) {
    assert(!if_enabled && current_id <= TEST_THREADS);
    blocked[current_id] = true;
    while (!woken[current_id]) pthread_cond_wait(&sleeping[current_id], &kernel_lock);
    woken[current_id] = false;
    blocked[current_id] = false;
    return true;
}
bool scheduler_wake(uint64_t id) {
    assert(!if_enabled);
    if (!id || id > TEST_THREADS || !blocked[id] || woken[id]) return false;
    woken[id] = true;
    pthread_cond_signal(&sleeping[id]);
    return true;
}
static void wait_for_count(wait_queue_t *q, unsigned expected) {
    for (unsigned tries = 0; tries < 1000000; ++tries) {
        uint64_t f = irq_save_disable();
        unsigned got = wait_queue_count(q);
        irq_restore(f);
        if (got == expected) return;
        sched_yield();
    }
    assert(!"waiter registration timed out");
}
static wait_queue_t global_wait;
static _Atomic unsigned completed;
static void *basic_waiter(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    uint64_t f = irq_save_disable();
    assert(wait_queue_sleep_locked(&global_wait));
    irq_restore(f);
    atomic_fetch_add(&completed, 1u);
    return NULL;
}
static event_t e;
static void *event_waiter(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    assert(event_wait(&e));
    atomic_fetch_add(&completed, 1u);
    return NULL;
}
static mutex_t m;
static unsigned protected_counter;
static void *mutex_waiter(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    assert(mutex_lock(&m));
    ++protected_counter;
    assert(mutex_unlock(&m));
    return NULL;
}
static void *mutex_stress_waiter(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    for (unsigned i = 0; i < 200; ++i) {
        assert(mutex_lock(&m));
        ++protected_counter;
        assert(mutex_unlock(&m));
        if (!(i % 3u)) sched_yield();
    }
    return NULL;
}
static ipc_queue_t q;
static _Atomic unsigned receive_value;
static void *consumer(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    unsigned value = 0;
    size_t got = 0;
    assert(ipc_receive(&q, &value, sizeof(value), &got) == IPC_OK);
    assert(got == sizeof(value));
    atomic_store(&receive_value, value);
    return NULL;
}
static void *producer(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    unsigned value = 5678;
    assert(ipc_send(&q, &value, sizeof(value)) == IPC_OK);
    return NULL;
}
static void *closed_consumer(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    unsigned v; size_t n;
    assert(ipc_receive(&q, &v, sizeof(v), &n) == IPC_CLOSED);
    return NULL;
}
static void *closed_producer(void *arg) {
    as_thread((uint64_t)(uintptr_t)arg);
    unsigned v = 500;
    assert(ipc_send(&q, &v, sizeof(v)) == IPC_CLOSED);
    return NULL;
}
int main(void) {
    as_thread(1);
    for (unsigned i = 1; i <= TEST_THREADS; ++i) assert(!pthread_cond_init(&sleeping[i], NULL));
    assert(!pthread_mutex_lock(&kernel_lock));
    assert(!pthread_mutex_unlock(&kernel_lock));
    pthread_t a, b;
    wait_queue_init(&global_wait);
    assert(!pthread_create(&a, NULL, basic_waiter, (void *)(uintptr_t)2));
    wait_for_count(&global_wait, 1);
    assert(!pthread_create(&b, NULL, basic_waiter, (void *)(uintptr_t)3));
    wait_for_count(&global_wait, 2);
    uint64_t f = irq_save_disable();
    assert(wait_queue_wake_one_locked(&global_wait) == 1);
    assert(wait_queue_count(&global_wait) == 1);
    assert(wait_queue_wake_all_locked(&global_wait) == 1);
    irq_restore(f);
    pthread_join(a, NULL); pthread_join(b, NULL);
    assert(atomic_load(&completed) == 2);

    event_init(&e, false);
    atomic_store(&completed, 0);
    assert(!pthread_create(&a, NULL, event_waiter, (void *)(uintptr_t)2));
    assert(!pthread_create(&b, NULL, event_waiter, (void *)(uintptr_t)3));
    wait_for_count(&e.waiters, 2);
    event_set(&e); event_reset(&e); /* wake remains valid even after reset */
    pthread_join(a, NULL); pthread_join(b, NULL);
    assert(atomic_load(&completed) == 2 && !event_is_set(&e));
    event_set(&e);
    assert(event_is_set(&e) && event_wait(&e));
    event_reset(&e);
    mutex_init(&m);
    assert(mutex_try_lock(&m) && !mutex_try_lock(&m));
    assert(!pthread_create(&a, NULL, mutex_waiter, (void *)(uintptr_t)2));
    assert(!pthread_create(&b, NULL, mutex_waiter, (void *)(uintptr_t)3));
    wait_for_count(&m.waiters, 2);
    assert(mutex_unlock(&m));
    pthread_join(a, NULL); pthread_join(b, NULL);
    assert(protected_counter == 2 && !mutex_unlock(&m));
    pthread_t stress[4];
    for (unsigned i = 0; i < 4; ++i)
        assert(!pthread_create(&stress[i], NULL, mutex_stress_waiter,
                               (void *)(uintptr_t)(i + 2u)));
    for (unsigned i = 0; i < 4; ++i) pthread_join(stress[i], NULL);
    assert(protected_counter == 802);


    ipc_init(&q);
    size_t got = 0;
    unsigned value = 0;
    assert(ipc_try_receive(&q, &value, sizeof(value), &got) == IPC_WOULD_BLOCK);
    assert(ipc_try_send(&q, &value, IPC_MESSAGE_BYTES + 1u) == IPC_BAD_ARGUMENT);
    assert(!pthread_create(&a, NULL, consumer, (void *)(uintptr_t)2));
    wait_for_count(&q.readers, 1);
    value = 1234;
    assert(ipc_try_send(&q, &value, sizeof(value)) == IPC_OK);
    pthread_join(a, NULL);
    assert(atomic_load(&receive_value) == 1234);

    for (unsigned i = 0; i < IPC_QUEUE_CAPACITY; ++i)
        assert(ipc_try_send(&q, &i, sizeof(i)) == IPC_OK);
    assert(ipc_try_send(&q, &value, sizeof(value)) == IPC_WOULD_BLOCK);
    assert(!pthread_create(&a, NULL, producer, (void *)(uintptr_t)2));
    wait_for_count(&q.writers, 1);
    for (unsigned i = 0; i < IPC_QUEUE_CAPACITY; ++i) {
        unsigned n = 999;
        assert(ipc_try_receive(&q, &n, 2, &got) == IPC_BUFFER_TOO_SMALL);
        assert(got == sizeof(n));
        assert(ipc_try_receive(&q, &n, sizeof(n), &got) == IPC_OK);
        assert(n == i);
    }
    pthread_join(a, NULL);
    assert(ipc_try_receive(&q, &value, sizeof(value), &got) == IPC_OK && value == 5678);
    assert(ipc_try_receive(&q, &value, sizeof(value), &got) == IPC_WOULD_BLOCK);

    ipc_init(&q);
    assert(!pthread_create(&a, NULL, closed_consumer, (void *)(uintptr_t)2));
    wait_for_count(&q.readers, 1);
    ipc_close(&q);
    pthread_join(a, NULL);
    assert(ipc_try_send(&q, &value, sizeof(value)) == IPC_CLOSED);
    assert(ipc_try_receive(&q, &value, sizeof(value), &got) == IPC_CLOSED);

    ipc_init(&q);
    for (unsigned i = 0; i < IPC_QUEUE_CAPACITY; ++i)
        assert(ipc_try_send(&q, &i, sizeof(i)) == IPC_OK);
    assert(!pthread_create(&a, NULL, closed_producer, (void *)(uintptr_t)2));
    wait_for_count(&q.writers, 1);
    ipc_close(&q);
    pthread_join(a, NULL);
    for (unsigned i = 0; i < IPC_QUEUE_CAPACITY; ++i) {
        assert(ipc_try_receive(&q, &value, sizeof(value), &got) == IPC_OK);
        assert(value == i);
    }
    assert(ipc_try_receive(&q, &value, sizeof(value), &got) == IPC_CLOSED);
    puts("PASS: wait FIFO, manual-reset event, mutex contention, IPC FIFO/backpressure/close");
    return 0;
}
#elif defined(OPENXP_TEST_SCHEDULER)
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
#elif defined(OPENXP_TEST_CONTEXT_SWITCH)
/* Runs the exact kernel assembly context switch in a native userspace process.
 * This is not a replacement for VM IRQ or fault tests but does execute the
 * real save/restore stack and return path. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

extern void arch_context_switch(uint64_t *old_sp, uint64_t new_sp);
static uint8_t alternate_stack[32768] __attribute__((aligned(16)));
static uint64_t main_sp, alternate_sp;
static volatile unsigned event;

__attribute__((noreturn)) static void alternate_entry(void) {
    assert(event == 0);
    event = 1;
    arch_context_switch(&alternate_sp, main_sp);
    assert(event == 2);
    event = 3;
    arch_context_switch(&alternate_sp, main_sp);
    /* There must be no third resume. */
    __builtin_trap();
}
int main(void) {
    uint64_t top = (uint64_t)(uintptr_t)(alternate_stack + sizeof(alternate_stack));
    assert((top & 15u) == 0);
    uint64_t *frame = (uint64_t *)(uintptr_t)(top - 64);
    for (unsigned i = 0; i < 6; ++i) frame[i] = 0;
    frame[6] = (uint64_t)(uintptr_t)alternate_entry;
    frame[7] = 0;  /* alternate_entry is noreturn */
    alternate_sp = (uint64_t)(uintptr_t)frame;
    arch_context_switch(&main_sp, alternate_sp);
    assert(event == 1);
    event = 2;
    arch_context_switch(&main_sp, alternate_sp);
    assert(event == 3);
    puts("PASS: real x86-64 context switch resumes two independent stacks");
    return 0;
}
#elif defined(OPENXP_TEST_PREEMPT)
#include "sched/scheduler.h"
#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/irq_state.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
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
    /* Regression for x86-64 IRETQ reading the unmapped upper stack guard:
     * RSP and SS must be the final two qwords, even for a CPL0 return. */
    assert(sizeof(*first) == 176);
    assert(offsetof(struct exception_frame, rsp) == 160);
    assert(offsetof(struct exception_frame, ss) == 168);
    assert(first->vector == 32 && first->error == 0);
    assert(first->cs == 0x08 && first->rflags == 0x202);
    assert(first->rip != 0);
    assert(first->ss == 0x10);
    assert(first->rsp == (uint64_t)(uintptr_t)first + sizeof(*first));
    assert((first->rsp & 15u) == 8);
    assert(*(const uint64_t *)(uintptr_t)first->rsp != 0);
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
#elif defined(OPENXP_TEST_SLEEP)
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
#elif defined(OPENXP_TEST_INPUT)
#include "input/input.h"
#include <assert.h>
#include <stdio.h>
static bool mouse(input_device_t *s,uint8_t a,uint8_t b,uint8_t c,input_event_t *e) {
    assert(!input_decode_mouse(s,a,e));assert(!input_decode_mouse(s,b,e));return input_decode_mouse(s,c,e);
}
int main(void){
    input_device_t d; input_event_t e;
    input_state_init(&d,640,480);assert(d.x==320 && d.y==240);
    assert(input_decode_keyboard(&d,0x1e,&e)&&e.key=='a');
    assert(!input_decode_keyboard(&d,0x2a,&e));
    assert(input_decode_keyboard(&d,0x1e,&e)&&e.key=='A');
    assert(!input_decode_keyboard(&d,0xaa,&e));
    assert(!input_decode_keyboard(&d,0x3a,&e));
    assert(input_decode_keyboard(&d,0x1e,&e)&&e.key=='A');
    assert(input_decode_keyboard(&d,0x1c,&e)&&e.key=='\n');
    assert(mouse(&d,0x09,5,2,&e));assert(e.kind==INPUT_BUTTON&&d.x==325&&d.y==238);
    assert(mouse(&d,0x08,0,0,&e));assert(e.kind==INPUT_BUTTON&&!e.left);
    assert(!mouse(&d,0x08,0,0,&e));
    assert(mouse(&d,0x0a,0,0,&e) && e.kind==INPUT_BUTTON && e.right && !e.left);
    assert(mouse(&d,0x08,0,0,&e) && e.kind==INPUT_BUTTON && !e.right);
    assert(mouse(&d,0x18,0xff,0,&e)&&d.x==324);
    /* E0-prefixed Windows keys and modifier-aware shortcuts. */
    assert(!input_decode_keyboard(&d,0xe0,&e));
    assert(input_decode_keyboard(&d,0x5b,&e));
    assert(e.code==KEY_WINDOWS && (e.modifiers&KEYMOD_WIN));
    assert(input_decode_keyboard(&d,0x12,&e) && (e.key=='e'||e.key=='E') &&
           (e.modifiers&KEYMOD_WIN));
    assert(!input_decode_keyboard(&d,0xe0,&e));
    assert(!input_decode_keyboard(&d,0xdb,&e));
    assert(!d.win);
    assert(!input_decode_keyboard(&d,0x38,&e));
    assert(input_decode_keyboard(&d,0x0f,&e) && e.code==KEY_TAB &&
           (e.modifiers&KEYMOD_ALT));
    assert(input_decode_keyboard(&d,0x3e,&e) && e.code==KEY_F4);
    assert(!input_decode_keyboard(&d,0xb8,&e));
    assert(!input_decode_keyboard(&d,0xe0,&e));
    assert(input_decode_keyboard(&d,0x48,&e) && e.code==KEY_UP);
    assert(!input_decode_keyboard(&d,0xe0,&e));
    assert(!input_decode_keyboard(&d,0xc8,&e));
    assert(!input_decode_keyboard(&d,0x2a,&e));
    assert(!input_decode_keyboard(&d,0x36,&e));
    assert(!input_decode_keyboard(&d,0xaa,&e));
    assert(d.shift); /* releasing one Shift must retain the other */
    assert(!input_decode_keyboard(&d,0xb6,&e));
    assert(!d.shift);
    input_raw_queue_t q;
    input_queue_reset(&q);
    for(unsigned i=0;i<INPUT_RAW_CAPACITY-1u;++i)
        assert(input_queue_push(&q,(uint8_t)i,(i&1u)!=0));
    assert(!input_queue_push(&q,0,false));
    assert(q.dropped==1);
    for(unsigned i=0;i<INPUT_RAW_CAPACITY-1u;++i) {
        input_raw_byte_t raw;
        assert(input_queue_pop(&q,&raw));
        assert(raw.value==(uint8_t)i && raw.auxiliary==(i&1u));
    }
    input_raw_byte_t raw;
    assert(!input_queue_pop(&q,&raw));
    assert(input_queue_push(&q,0xaa,true));
    assert(input_queue_pop(&q,&raw) && raw.value==0xaa && raw.auxiliary);
    puts("PASS: PS/2 packet decoding, modifiers, Windows key, special keys");
}
#else
#error "Compile this suite with an OPENXP_TEST_* selector"
#endif
