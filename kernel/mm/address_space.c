#include "mm/address_space.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include <stdint.h>
#include <stddef.h>

extern void *memcpy(void *, const void *, size_t);
#define P 1ULL
#define W 2ULL
#define U 4ULL
#define PS (1ULL << 7)
#define NX (1ULL << 63)
#define MASK 0x000ffffffffff000ULL
#define USER_SIZE (4ULL * 1024 * 1024)

/* The API is intentionally not reentrant. Any future preemptive user tasks
 * need a per-CPU address-space owner and proper page-table synchronization. */
static bool irq_masked(void) {
#ifdef OPENXP_VMM_HOST_TEST
    return true;
#else
    uint64_t rflags;
    __asm__ volatile ("pushfq; popq %0" : "=r"(rflags));
    return !(rflags & (1ULL << 9));
#endif
}
static uint64_t *table(uint64_t pa) { return (uint64_t *)pmm_direct_map(pa & MASK); }
static unsigned idx(uint64_t va, unsigned shift) { return (unsigned)((va >> shift) & 511u); }
static bool allowed(uint64_t va) {
    uint64_t base = vmm_user_base();
    return base && !(va & 4095) && va >= base && va - base < USER_SIZE;
}
static bool empty(const uint64_t *t) {
    for (unsigned i = 0; i < 512; ++i) if (t[i] & P) return false;
    return true;
}
#ifdef OPENXP_VMM_HOST_TEST
static uint64_t simulated_cr3;
uint64_t aspace_current_root(void) { return simulated_cr3; }
static void set_cr3(uint64_t cr3) { simulated_cr3 = cr3; }
#else
uint64_t aspace_current_root(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3,%0" : "=r"(cr3));
    return cr3 & MASK;
}
static void set_cr3(uint64_t cr3) {
    __asm__ volatile ("mov %0,%%cr3" : : "r"(cr3) : "memory");
}
#endif
uint64_t aspace_activate(const address_space_t *space) {
    if (!space || !space->cr3 || !irq_masked()) return 0;
    uint64_t previous = aspace_current_root();
    set_cr3(space->cr3);
    return previous;
}
bool aspace_restore(uint64_t previous_cr3) {
    if (!irq_masked() || !previous_cr3 || (previous_cr3 & 4095) || !table(previous_cr3)) return false;
    set_cr3(previous_cr3);
    return true;
}
bool aspace_create(address_space_t *space) {
    if (!irq_masked() || !space || space->cr3 || !vmm_user_base()) return false;
    uint64_t kernel_root = aspace_current_root();
#ifdef OPENXP_VMM_HOST_TEST
    /* Unit tests attach a mock root rather than executing privileged CR3. */
    if (!kernel_root) return false;
#endif
    uint64_t *kernel = table(kernel_root);
    if (!kernel) return false;
    uint64_t pa = pmm_alloc_zeroed_page();
    if (!pa) return false;
    uint64_t *newroot = table(pa);
    if (!newroot) { (void)pmm_free_page(pa); return false; }
    /* Do not share low-half bootloader/identity mappings with userspace. */
    for (unsigned i = 256; i < 512; ++i)
        newroot[i] = kernel[i] & ~U; /* shared kernel branch, supervisor only */
    space->cr3 = pa;
    return true;
}
static bool walk(const address_space_t *s, uint64_t va, uint64_t **leaf) {
    if (!irq_masked() || !s || !s->cr3 || !allowed(va)) return false;
    uint64_t *t = table(s->cr3);
    if (!t) return false;
    for (unsigned shift = 39; shift >= 21; shift -= 9) {
        uint64_t e = t[idx(va, shift)];
        if ((e & (P | U)) != (P | U) || (e & PS)) return false;
        t = table(e & MASK);
        if (!t) return false;
    }
    *leaf = &t[idx(va, 12)];
    return true;
}
bool aspace_query(const address_space_t *space, uint64_t va, uint64_t *pa, unsigned *flags) {
    uint64_t *leaf;
    if (!walk(space, va, &leaf) || (*leaf & (P | U)) != (P | U)) return false;
    if (pa) *pa = *leaf & MASK;
    if (flags) *flags = ((*leaf & W) ? VMM_PAGE_WRITE : 0) |
                         ((*leaf & NX) ? 0 : VMM_PAGE_EXEC);
    return true;
}
bool aspace_map(address_space_t *space, uint64_t va, uint64_t pa, unsigned flags) {
    if (!irq_masked() || !space || !space->cr3 || !allowed(va) || !pa || (pa & 4095) ||
        (pa & ~MASK) || (flags & ~(VMM_PAGE_WRITE | VMM_PAGE_EXEC)) ||
        ((flags & VMM_PAGE_WRITE) && (flags & VMM_PAGE_EXEC))) return false;
    uint64_t *tables[4] = { table(space->cr3), 0, 0, 0 };
    uint64_t phys[4] = { space->cr3, 0, 0, 0 };
    unsigned indices[3] = {idx(va,39),idx(va,30),idx(va,21)};
    bool created[3] = {false,false,false};
    if (!tables[0]) return false;
    for (unsigned level = 0; level < 3; ++level) {
        uint64_t e = tables[level][indices[level]];
        if (!(e & P)) {
            uint64_t fresh = pmm_alloc_zeroed_page();
            if (!fresh) goto rollback;
            if (!table(fresh)) { (void)pmm_free_page(fresh); goto rollback; }
            tables[level][indices[level]] = fresh | P | W | U;
            created[level] = true;
            e = tables[level][indices[level]];
        }
        if ((e & (P | U)) != (P | U) || (e & PS)) goto rollback;
        phys[level + 1] = e & MASK;
        tables[level + 1] = table(phys[level + 1]);
        if (!tables[level + 1]) goto rollback;
    }
    unsigned leaf = idx(va,12);
    if (tables[3][leaf] & P) goto rollback;
    tables[3][leaf] = pa | P | U | ((flags & VMM_PAGE_WRITE) ? W : 0) |
                    ((flags & VMM_PAGE_EXEC) ? 0 : NX);
    /* Mapping a currently active address space requires local TLB invalidation. */
#ifndef OPENXP_VMM_HOST_TEST
    if (aspace_current_root() == space->cr3)
        __asm__ volatile ("invlpg (%0)" : : "r"(va) : "memory");
#endif
    return true;
rollback:
    for (int i = 2; i >= 0; --i) if (created[i]) {
        tables[i][indices[i]] = 0;
        (void)pmm_free_page(phys[i+1]);
    }
    return false;
}
bool aspace_unmap(address_space_t *s, uint64_t va) {
    if (!irq_masked() || !s || !s->cr3 || !allowed(va)) return false;
    uint64_t *tables[4] = {table(s->cr3),0,0,0};
    uint64_t phys[4] = {s->cr3,0,0,0};
    unsigned indices[4] = {idx(va,39),idx(va,30),idx(va,21),idx(va,12)};
    if (!tables[0]) return false;
    for (unsigned i = 0; i < 3; ++i) {
        uint64_t e = tables[i][indices[i]];
        if ((e & (P | U)) != (P | U) || (e & PS)) return false;
        phys[i+1] = e & MASK;
        tables[i+1] = table(phys[i+1]);
        if (!tables[i+1]) return false;
    }
    uint64_t entry = tables[3][indices[3]];
    if ((entry & (P | U)) != (P | U)) return false;
    tables[3][indices[3]] = 0;
#ifndef OPENXP_VMM_HOST_TEST
    if (aspace_current_root() == s->cr3)
        __asm__ volatile ("invlpg (%0)" : : "r"(va) : "memory");
#endif
    if (!pmm_free_page(entry & MASK)) return false;
    for (int i = 3; i > 0; --i) {
        if (!empty(tables[i])) break;
        tables[i-1][indices[i-1]] = 0;
        if (!pmm_free_page(phys[i])) return false;
    }
    return true;
}
bool aspace_destroy(address_space_t *s) {
    if (!irq_masked() || !s || !s->cr3 || aspace_current_root() == s->cr3) return false;
    uint64_t *root = table(s->cr3);
    if (!root) return false;
    /* Only our one low user slot can hold owned mappings. */
    unsigned slot = idx(vmm_user_base(), 39);
    uint64_t e = root[slot];
    if (e & P) {
        if (!(e & U) || (e & PS)) return false;
        uint64_t *pdpt = table(e & MASK);
        if (!pdpt) return false;
        for (unsigned i=0;i<512;++i) if (pdpt[i] & P) {
            if (!(pdpt[i] & U) || (pdpt[i] & PS)) return false;
            uint64_t *pd = table(pdpt[i] & MASK);
            if (!pd) return false;
            for (unsigned j=0;j<512;++j) if (pd[j] & P) {
                if (!(pd[j] & U) || (pd[j] & PS)) return false;
                uint64_t *pt = table(pd[j] & MASK);
                if (!pt) return false;
                for (unsigned k=0;k<512;++k) if (pt[k] & P)
                    if (!pmm_free_page(pt[k] & MASK)) return false;
                if (!pmm_free_page(pd[j] & MASK)) return false;
            }
            if (!pmm_free_page(pdpt[i] & MASK)) return false;
        }
        if (!pmm_free_page(e & MASK)) return false;
    }
    if (!pmm_free_page(s->cr3)) return false;
    s->cr3 = 0;
    return true;
}
bool aspace_range_valid(const address_space_t *s, uint64_t addr, size_t length, bool writable) {
    if (!s || !s->cr3) return false;
    if (!length) return true;
    uint64_t base = vmm_user_base();
    if (!base || addr < base || length > USER_SIZE ||
        addr - base >= USER_SIZE || length > USER_SIZE - (addr - base)) return false;
    uint64_t end = addr + (uint64_t)length - 1;
    uint64_t first = addr & ~4095ULL;
    uint64_t last = end & ~4095ULL;
    for (uint64_t page = first;; page += 4096) {
        unsigned flags;
        if (!aspace_query(s, page, NULL, &flags) ||
            (writable && !(flags & VMM_PAGE_WRITE))) return false;
        if (page == last) break;
    }
    return true;
}
static bool transfer(address_space_t *s, uint64_t address, void *buf,
                     size_t length, bool to_user, bool loader) {
    if (!buf && length) return false;
    if (!aspace_range_valid(s, address, length, to_user && !loader)) return false;
    uint8_t *bytes = (uint8_t *)buf;
    while (length) {
        uint64_t page = address & ~4095ULL, pa;
        if (!aspace_query(s, page, &pa, NULL)) return false;
        unsigned off = (unsigned)(address & 4095ULL);
        size_t n = 4096 - off;
        if (n > length) n = length;
        uint8_t *mapped = (uint8_t *)pmm_direct_map(pa);
        if (!mapped) return false;
        if (to_user) memcpy(mapped + off, bytes, n);
        else memcpy(bytes, mapped + off, n);
        address += n;
        bytes += n;
        length -= n;
    }
    return true;
}
bool aspace_copy_from_user(const address_space_t *s, void *dst, uint64_t src, size_t size) {
    return transfer((address_space_t *)s, src, dst, size, false, false);
}
bool aspace_copy_to_user(address_space_t *s, uint64_t dst, const void *src, size_t size) {
    return transfer(s, dst, (void *)src, size, true, false);
}
bool aspace_load_image(address_space_t *s, uint64_t dst, const void *src, size_t size) {
    return transfer(s, dst, (void *)src, size, true, true);
}
