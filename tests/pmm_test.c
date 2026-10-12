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
