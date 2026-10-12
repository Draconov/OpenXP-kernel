#ifndef OPENXP_MM_HEAP_H
#define OPENXP_MM_HEAP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Single-core, fixed-size allocator. Objects can be freed/reused; no page
 * growth, preemption, locking, or concurrent access. Heap bounds are guarded
 * by unmapped pages installed by the bootstrap, not by this allocator. */
bool heap_init(void *mapped_base, size_t bytes);
void *kmalloc(size_t bytes);
void *kmalloc_aligned(size_t bytes, size_t alignment);
void *kcalloc(size_t count, size_t bytes);
void *krealloc(void *ptr, size_t new_size);
bool kfree(void *ptr); /* false for unknown pointers or double-free; NULL is OK */
size_t heap_used_bytes(void); /* live requested payload bytes */
size_t heap_capacity_bytes(void); /* total mapped region, including metadata */
size_t heap_free_bytes(void); /* available free payload, excluding headers */
uint64_t heap_guard_low(void);
uint64_t heap_guard_high(void);
#endif
