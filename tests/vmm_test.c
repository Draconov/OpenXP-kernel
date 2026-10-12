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
