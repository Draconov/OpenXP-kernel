#ifndef OPENXP_MM_ADDRESS_SPACE_H
#define OPENXP_MM_ADDRESS_SPACE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Experimental single-core per-process CPL3 address space. User leaf frames are owned by
 * this object; kernel-half page-table branches are shared and never freed.
 * Interrupts must remain OFF for create/map/switch/destroy and all operations
 * on an address space currently in use. No SMP or user-task preemption yet. */
typedef struct openxp_address_space {
    uint64_t cr3;
} address_space_t;

bool aspace_create(address_space_t *space);
/* Cannot destroy the active root. Frees all owned user pages and tables. */
bool aspace_destroy(address_space_t *space);
bool aspace_map(address_space_t *space, uint64_t va, uint64_t pa, unsigned flags);
bool aspace_query(const address_space_t *space, uint64_t va, uint64_t *pa, unsigned *flags);
bool aspace_unmap(address_space_t *space, uint64_t va);
/* Validate the full range before accessing it. No actual CPL3 pointer is
 * dereferenced by the kernel; all data transfers use checked HHDM aliases. */
bool aspace_range_valid(const address_space_t *space, uint64_t addr, size_t size, bool writable);
bool aspace_copy_from_user(const address_space_t *space, void *dst, uint64_t user_src, size_t size);
bool aspace_copy_to_user(address_space_t *space, uint64_t user_dst, const void *src, size_t size);
/* Image loader may populate initially RX pages while still in kernel mode.
 * This is NOT available to an untrusted syscall. */
bool aspace_load_image(address_space_t *space, uint64_t user_dst, const void *src, size_t size);
/* Returns previous CR3 so kernel caller can restore it before destroy. */
uint64_t aspace_activate(const address_space_t *space);
bool aspace_restore(uint64_t previous_cr3);
uint64_t aspace_current_root(void);

#endif
