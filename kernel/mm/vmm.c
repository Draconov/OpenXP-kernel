#include "mm/vmm.h"
#include "mm/pmm.h"
#include <stddef.h>
#include <stdint.h>

#define PRESENT (1ULL << 0)
#define WRITE (1ULL << 1)
#define HUGE (1ULL << 7)
#define NOEXEC (1ULL << 63)
#define ADDRESS_MASK 0x000ffffffffff000ULL
#define SLOT_SIZE (1ULL << 39)
static uint64_t root_physical, region_base;
static bool ready;

static uint64_t *table_ptr(uint64_t pa) {
    return (uint64_t *)pmm_direct_map(pa & ADDRESS_MASK);
}
static bool slot_address(uint16_t slot, uint64_t *result) {
    if (slot < 256 || slot > 510) return false;
    *result = 0xffff000000000000ULL | ((uint64_t)slot << 39);
    return true;
}
static bool region_contains(uint64_t va) {
    return ready && (va & (VMM_PAGE_SIZE - 1)) == 0 &&
           va >= region_base && va - region_base < SLOT_SIZE;
}
static uint16_t index_at(uint64_t va, unsigned shift) {
    return (uint16_t)((va >> shift) & 511u);
}
static bool empty_table(const uint64_t *table) {
    for (unsigned i = 0; i < 512; ++i) if (table[i] & PRESENT) return false;
    return true;
}

/* Deep-copy all four page-table levels, retaining the exact existing leaf
 * mappings (including huge pages). Never copy physical *data* frames. A failed
 * clone is rolled back completely. Old Limine tables remain allocated; future
 * reclaim requires proving their frames are outside any retained mappings. */
#define MAX_CLONE_TABLES 16384u
static void discard_clone(uint64_t pa, unsigned depth) {
    uint64_t *entries = table_ptr(pa);
    if (entries && depth > 1) {
        for (unsigned i = 0; i < 512; ++i) {
            uint64_t e = entries[i];
            if ((e & PRESENT) && !(e & HUGE))
                discard_clone(e & ADDRESS_MASK, depth - 1);
        }
    }
    (void)pmm_free_page(pa);
}
static bool clone_table(uint64_t old_pa, unsigned depth, unsigned *count, uint64_t *out_pa) {
    uint64_t *old = table_ptr(old_pa);
    if (!old || *count >= MAX_CLONE_TABLES) return false;
    uint64_t new_pa = pmm_alloc_zeroed_page();
    if (!new_pa) return false;
    ++*count;
    uint64_t *fresh = table_ptr(new_pa);
    if (!fresh) { (void)pmm_free_page(new_pa); return false; }
    for (unsigned i = 0; i < 512; ++i) {
        uint64_t e = old[i];
        if ((e & PRESENT) && depth > 1) {
            if (e & HUGE) {
                if (depth == 4) goto failed; /* huge pages invalid at PML4 */
            } else {
                uint64_t child = 0;
                if (!clone_table(e & ADDRESS_MASK, depth - 1, count, &child)) goto failed;
                e = (e & ~ADDRESS_MASK) | child;
            }
        }
        fresh[i] = e;
    }
    *out_pa = new_pa;
    return true;
failed:
    discard_clone(new_pa, depth);
    return false;
}
#ifdef OPENXP_VMM_HOST_TEST
bool vmm_test_clone_root(uint64_t old_root_pa, uint64_t *new_root_pa) {
    if (!old_root_pa || !new_root_pa || (old_root_pa & 4095)) return false;
    unsigned count = 0;
    return clone_table(old_root_pa, 4, &count, new_root_pa);
}
void vmm_test_destroy_clone(uint64_t root_pa) {
    if (root_pa) discard_clone(root_pa, 4);
}
#endif

static void flush_one(uint64_t va) {
#ifndef OPENXP_VMM_HOST_TEST
    __asm__ volatile ("invlpg (%0)" : : "r"(va) : "memory");
#else
    (void)va;
#endif
}

#ifdef OPENXP_VMM_HOST_TEST
bool vmm_test_attach(uint64_t root_pa, uint16_t unused_pml4_slot) {
    uint64_t base;
    if (ready || !root_pa || (root_pa & 4095) ||
        !slot_address(unused_pml4_slot, &base)) return false;
    uint64_t *root = table_ptr(root_pa);
    if (!root || (root[unused_pml4_slot] & PRESENT)) return false;
    root_physical = root_pa;
    region_base = base;
    ready = true;
    return true;
}
#else
static bool cpu_has_nx(void) {
    uint32_t eax = 0x80000000u, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    if (eax < 0x80000001u) return false;
    eax = 0x80000001u;
    __asm__ volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    return (edx & (1u << 20)) != 0;
}
bool vmm_init(void) {
    if (ready || !cpu_has_nx()) return false;
    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    if (cr4 & (1ULL << 12)) return false; /* this walker only supports 4 levels */
    uint64_t old_cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(old_cr3));
    const uint64_t old_root_pa = old_cr3 & ADDRESS_MASK;
    uint64_t *old_root = table_ptr(old_root_pa);
    if (!old_root) return false;
    uint16_t slot = 0;
    /* Choose an entirely unused upper-half PML4 entry. Never overwrite
     * existing HHDM, framebuffer, kernel or bootloader mappings. */
    for (unsigned i = 288; i <= 508; ++i) {
        if (!(old_root[i] & PRESENT)) { slot = (uint16_t)i; break; }
    }
    uint64_t base;
    if (!slot || !slot_address(slot, &base)) return false;
    unsigned cloned_tables = 0;
    uint64_t new_root_pa = 0;
    if (!clone_table(old_root_pa, 4, &cloned_tables, &new_root_pa)) return false;

    /* NX bit must be enabled before page tables containing NX entries exist. */
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xc0000080u));
    uint64_t efer = ((uint64_t)hi << 32) | lo;
    if (!(efer & (1u << 11))) {
        efer |= (1u << 11);
        lo = (uint32_t)efer; hi = (uint32_t)(efer >> 32);
        __asm__ volatile ("wrmsr" : : "c"(0xc0000080u), "a"(lo), "d"(hi));
    }
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1ULL << 16); /* supervisor write protection */
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
    __asm__ volatile ("cli; mov %0, %%cr3" : : "r"(new_root_pa) : "memory");
    root_physical = new_root_pa;
    region_base = base;
    ready = true;
    return true;
}
#endif

uint64_t vmm_region_base(void) { return ready ? region_base : 0; }

bool vmm_map_page(uint64_t va, uint64_t pa, unsigned flags) {
    if (!region_contains(va) || !pa || (pa & 4095) ||
        (pa & ~ADDRESS_MASK) || (flags & ~(VMM_PAGE_WRITE | VMM_PAGE_EXEC))) return false;
    uint64_t *tables[4] = { table_ptr(root_physical), NULL, NULL, NULL };
    uint64_t child_pa[4] = { 0, 0, 0, 0 };
    unsigned parent_indices[3] = { index_at(va, 39), index_at(va, 30), index_at(va, 21) };
    bool created[3] = { false, false, false };
    if (!tables[0]) return false;
    for (unsigned level = 0; level < 3; ++level) {
        uint64_t entry = tables[level][parent_indices[level]];
        if (!(entry & PRESENT)) {
            uint64_t new_pa = pmm_alloc_zeroed_page();
            if (!new_pa) goto rollback;
            uint64_t *new_table = table_ptr(new_pa);
            if (!new_table) { (void)pmm_free_page(new_pa); goto rollback; }
            tables[level][parent_indices[level]] = new_pa | PRESENT | WRITE;
            entry = tables[level][parent_indices[level]];
            created[level] = true;
        }
        if (entry & HUGE) goto rollback;
        child_pa[level + 1] = entry & ADDRESS_MASK;
        tables[level + 1] = table_ptr(child_pa[level + 1]);
        if (!tables[level + 1]) goto rollback;
    }
    unsigned i = index_at(va, 12);
    if (tables[3][i] & PRESENT) goto rollback;
    tables[3][i] = pa | PRESENT | ((flags & VMM_PAGE_WRITE) ? WRITE : 0) |
                   ((flags & VMM_PAGE_EXEC) ? 0 : NOEXEC);
    flush_one(va);
    return true;
rollback:
    for (int level = 2; level >= 0; --level) {
        if (created[level]) {
            tables[level][parent_indices[level]] = 0;
            (void)pmm_free_page(child_pa[level + 1]);
        }
    }
    return false;
}

bool vmm_query_page(uint64_t va, uint64_t *pa, unsigned *flags) {
    if (!region_contains(va)) return false;
    uint64_t *table = table_ptr(root_physical);
    if (!table) return false;
    const unsigned shifts[] = { 39, 30, 21 };
    for (unsigned level = 0; level < 3; ++level) {
        uint64_t entry = table[index_at(va, shifts[level])];
        if (!(entry & PRESENT) || (entry & HUGE)) return false;
        table = table_ptr(entry & ADDRESS_MASK);
        if (!table) return false;
    }
    uint64_t entry = table[index_at(va, 12)];
    if (!(entry & PRESENT)) return false;
    if (pa) *pa = entry & ADDRESS_MASK;
    if (flags) *flags = ((entry & WRITE) ? VMM_PAGE_WRITE : 0) |
                        ((entry & NOEXEC) ? 0 : VMM_PAGE_EXEC);
    return true;
}

bool vmm_unmap_page(uint64_t va) {
    if (!region_contains(va)) return false;
    uint64_t *tables[4];
    unsigned indices[4] = { index_at(va, 39), index_at(va, 30),
                            index_at(va, 21), index_at(va, 12) };
    uint64_t addresses[4] = { root_physical, 0, 0, 0 };
    tables[0] = table_ptr(root_physical);
    if (!tables[0]) return false;
    for (unsigned i = 0; i < 3; ++i) {
        uint64_t entry = tables[i][indices[i]];
        if (!(entry & PRESENT) || (entry & HUGE)) return false;
        addresses[i + 1] = entry & ADDRESS_MASK;
        tables[i + 1] = table_ptr(addresses[i + 1]);
        if (!tables[i + 1]) return false;
    }
    if (!(tables[3][indices[3]] & PRESENT)) return false;
    tables[3][indices[3]] = 0;
    flush_one(va);
    /* Reclaim now-empty child tables. Data frames remain caller-owned. */
    for (int i = 3; i >= 1; --i) {
        if (!empty_table(tables[i])) break;
        tables[i - 1][indices[i - 1]] = 0;
        if (!pmm_free_page(addresses[i])) return false;
    }
    return true;
}

/* An unused low-half PML4 entry is reserved for user pages. Avoid index 0:
 * boot firmware often places identity mappings there. Keep all user mappings
 * in a small 4-MiB portion of the chosen 512-GiB slot. */
#define USER (1ULL << 2)
#define USER_LIMIT (4ULL * 1024 * 1024)
static uint64_t user_base;
static bool user_ready;
static bool user_contains(uint64_t va) {
    return user_ready && !(va & (VMM_PAGE_SIZE - 1)) &&
           va >= user_base && va - user_base < USER_LIMIT;
}
bool vmm_user_init(void) {
    if (!ready || user_ready) return false;
    uint64_t *root = table_ptr(root_physical);
    if (!root) return false;
    for (unsigned i = 1; i < 256; ++i) {
        if (!(root[i] & PRESENT)) {
            user_base = (uint64_t)i << 39;
            user_ready = true;
            return true;
        }
    }
    return false;
}
uint64_t vmm_user_base(void) { return user_ready ? user_base : 0; }

bool vmm_user_map_page(uint64_t va, uint64_t pa, unsigned flags) {
    if (!user_contains(va) || !pa || (pa & 4095) || (pa & ~ADDRESS_MASK) ||
        (flags & ~(VMM_PAGE_WRITE | VMM_PAGE_EXEC)) ||
        ((flags & VMM_PAGE_WRITE) && (flags & VMM_PAGE_EXEC))) return false;
    uint64_t *tables[4] = { table_ptr(root_physical), NULL, NULL, NULL };
    uint64_t child_pa[4] = {0};
    const unsigned indices[3] = { index_at(va, 39), index_at(va, 30), index_at(va, 21) };
    bool created[3] = {false};
    if (!tables[0]) return false;
    for (unsigned level = 0; level < 3; ++level) {
        uint64_t e = tables[level][indices[level]];
        if (!(e & PRESENT)) {
            uint64_t fresh = pmm_alloc_zeroed_page();
            if (!fresh) goto rollback;
            if (!table_ptr(fresh)) { (void)pmm_free_page(fresh); goto rollback; }
            tables[level][indices[level]] = fresh | PRESENT | WRITE | USER;
            e = tables[level][indices[level]];
            created[level] = true;
        }
        /* Never add USER to existing supervisor page-table branches. */
        if (!(e & USER) || (e & HUGE)) goto rollback;
        child_pa[level + 1] = e & ADDRESS_MASK;
        tables[level + 1] = table_ptr(child_pa[level + 1]);
        if (!tables[level + 1]) goto rollback;
    }
    unsigned idx = index_at(va, 12);
    if (tables[3][idx] & PRESENT) goto rollback;
    tables[3][idx] = pa | PRESENT | USER |
                     ((flags & VMM_PAGE_WRITE) ? WRITE : 0) |
                     ((flags & VMM_PAGE_EXEC) ? 0 : NOEXEC);
    flush_one(va);
    return true;
rollback:
    for (int level = 2; level >= 0; --level) {
        if (created[level]) {
            tables[level][indices[level]] = 0;
            (void)pmm_free_page(child_pa[level + 1]);
        }
    }
    return false;
}

bool vmm_user_query_page(uint64_t va, uint64_t *pa, unsigned *flags) {
    if (!user_contains(va)) return false;
    uint64_t *table = table_ptr(root_physical);
    if (!table) return false;
    const unsigned shifts[] = {39,30,21};
    for (unsigned level = 0; level < 3; ++level) {
        uint64_t e = table[index_at(va, shifts[level])];
        if ((e & (PRESENT | USER)) != (PRESENT | USER) || (e & HUGE)) return false;
        table = table_ptr(e & ADDRESS_MASK);
        if (!table) return false;
    }
    uint64_t e = table[index_at(va, 12)];
    if ((e & (PRESENT | USER)) != (PRESENT | USER)) return false;
    if (pa) *pa = e & ADDRESS_MASK;
    if (flags) *flags = ((e & WRITE) ? VMM_PAGE_WRITE : 0) |
                        ((e & NOEXEC) ? 0 : VMM_PAGE_EXEC);
    return true;
}

bool vmm_user_unmap_page(uint64_t va) {
    if (!user_contains(va)) return false;
    uint64_t *tables[4] = {0};
    const unsigned indices[4] = {index_at(va, 39), index_at(va, 30),
                                  index_at(va, 21), index_at(va, 12)};
    uint64_t physical[4] = {root_physical,0,0,0};
    tables[0] = table_ptr(root_physical);
    if (!tables[0]) return false;
    for (unsigned i = 0; i < 3; ++i) {
        uint64_t e = tables[i][indices[i]];
        if ((e & (PRESENT | USER)) != (PRESENT | USER) || (e & HUGE)) return false;
        physical[i+1] = e & ADDRESS_MASK;
        tables[i+1] = table_ptr(physical[i+1]);
        if (!tables[i+1]) return false;
    }
    if ((tables[3][indices[3]] & (PRESENT | USER)) != (PRESENT | USER)) return false;
    tables[3][indices[3]] = 0;
    flush_one(va);
    for (int i = 3; i > 0; --i) {
        if (!empty_table(tables[i])) break;
        tables[i-1][indices[i-1]] = 0;
        if (!pmm_free_page(physical[i])) return false;
    }
    return true;
}
