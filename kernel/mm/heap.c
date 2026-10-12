#include "mm/heap.h"
#include <stdint.h>
#include <stddef.h>

#define HEAP_MAGIC 0x4f58504845415031ULL /* OXPHEAP1 */
#define HEAP_ALIGN 16u
struct block {
    uint64_t magic;
    size_t capacity, requested;
    struct block *prev, *next;
    uint64_t allocated;
};
_Static_assert(sizeof(struct block) % HEAP_ALIGN == 0, "block headers must be 16-byte aligned");
static uintptr_t heap_base;
static size_t heap_capacity, live_bytes;
static struct block *first;
static bool initialized;

static bool round_up(size_t value, size_t alignment, size_t *out) {
    if (value > SIZE_MAX - (alignment - 1)) return false;
    *out = (value + alignment - 1) & ~(alignment - 1);
    return true;
}
static uint8_t *payload(struct block *b) { return (uint8_t *)b + sizeof(*b); }
static void make_block(struct block *b, size_t cap, struct block *prev, struct block *next) {
    b->magic = HEAP_MAGIC;
    b->capacity = cap;
    b->requested = 0;
    b->prev = prev;
    b->next = next;
    b->allocated = 0;
}
/* Split after `before` payload bytes, if enough space for another block.
 * Returns the tail, or NULL without changing anything. */
static struct block *split(struct block *b, size_t before) {
    if (before > b->capacity || b->capacity - before < sizeof(*b) + HEAP_ALIGN) return NULL;
    struct block *tail = (struct block *)(payload(b) + before);
    make_block(tail, b->capacity - before - sizeof(*b), b, b->next);
    if (b->next) b->next->prev = tail;
    b->next = tail;
    b->capacity = before;
    return tail;
}
static void merge_next(struct block *b) {
    struct block *next = b->next;
    if (!next || next->allocated || next->magic != HEAP_MAGIC) return;
    b->capacity += sizeof(*b) + next->capacity;
    b->next = next->next;
    if (b->next) b->next->prev = b;
    next->magic = 0;
}
static void coalesce(struct block *b) {
    while (b->next && !b->next->allocated) merge_next(b);
    if (b->prev && !b->prev->allocated) {
        b = b->prev;
        merge_next(b);
        while (b->next && !b->next->allocated) merge_next(b);
    }
}
static struct block *lookup(void *ptr) {
    /* Never dereference an untrusted pointer. Compare against known blocks. */
    for (struct block *b = first; b; b = b->next)
        if (b->magic == HEAP_MAGIC && payload(b) == (uint8_t *)ptr) return b;
    return NULL;
}
bool heap_init(void *mapped_base, size_t bytes) {
    uintptr_t ptr = (uintptr_t)mapped_base;
    if (initialized || !ptr || (ptr & 4095) || (bytes & 4095) ||
        bytes < sizeof(struct block) + HEAP_ALIGN ||
        ptr < 4096 || ptr > UINTPTR_MAX - 4096 || bytes > UINTPTR_MAX - 4096 - ptr) return false;
    heap_base = ptr;
    heap_capacity = bytes;
    live_bytes = 0;
    first = (struct block *)ptr;
    make_block(first, bytes - sizeof(*first), NULL, NULL);
    initialized = true;
    return true;
}
void *kmalloc_aligned(size_t bytes, size_t alignment) {
    if (!initialized || !bytes || alignment < sizeof(void *) ||
        (alignment & (alignment - 1)) || alignment > 4096) return NULL;
    if (alignment < HEAP_ALIGN) alignment = HEAP_ALIGN;
    size_t rounded;
    if (!round_up(bytes, HEAP_ALIGN, &rounded)) return NULL;
    for (struct block *b = first; b; b = b->next) {
        if (b->allocated || b->magic != HEAP_MAGIC) continue;
        uintptr_t start = (uintptr_t)payload(b), aligned;
        if (start > UINTPTR_MAX - (alignment - 1)) continue;
        aligned = (start + alignment - 1) & ~(uintptr_t)(alignment - 1);
        size_t prefix = aligned - start;
        /* A leading fragment must fit an independent block header. */
        if (prefix && prefix < sizeof(*b) + HEAP_ALIGN) {
            if (aligned > UINTPTR_MAX - alignment) continue;
            aligned += alignment;
            prefix = aligned - start;
        }
        if (prefix > b->capacity || rounded > b->capacity - prefix) continue;
        if (prefix) {
            struct block *tail = split(b, prefix - sizeof(*b));
            if (!tail) continue;
            b = tail;
        }
        (void)split(b, rounded);
        b->requested = bytes;
        b->allocated = 1;
        live_bytes += bytes; /* bounded by heap capacity */
        return payload(b);
    }
    return NULL;
}
void *kmalloc(size_t bytes) { return kmalloc_aligned(bytes, HEAP_ALIGN); }
void *kcalloc(size_t count, size_t bytes) {
    if (!count || !bytes || count > SIZE_MAX / bytes) return NULL;
    size_t total = count * bytes;
    uint8_t *result = kmalloc(total);
    if (!result) return NULL;
    for (size_t i = 0; i < total; ++i) result[i] = 0;
    return result;
}
bool kfree(void *ptr) {
    if (!ptr) return true;
    if (!initialized) return false;
    struct block *b = lookup(ptr);
    if (!b || !b->allocated) return false;
    live_bytes -= b->requested;
    b->requested = 0;
    b->allocated = 0;
    coalesce(b);
    return true;
}
void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (!new_size) { (void)kfree(ptr); return NULL; }
    if (!initialized) return NULL;
    struct block *b = lookup(ptr);
    if (!b || !b->allocated) return NULL;
    size_t rounded;
    if (!round_up(new_size, HEAP_ALIGN, &rounded)) return NULL;
    size_t old_size = b->requested;
    if (rounded > b->capacity && b->next && !b->next->allocated &&
        b->next->magic == HEAP_MAGIC &&
        rounded - b->capacity <= sizeof(*b) + b->next->capacity) merge_next(b);
    if (rounded <= b->capacity) {
        live_bytes = live_bytes - old_size + new_size;
        b->requested = new_size;
        struct block *tail = split(b, rounded);
        if (tail) coalesce(tail);
        return ptr;
    }
    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;
    uint8_t *dest = (uint8_t *)new_ptr, *source = (uint8_t *)ptr;
    for (size_t i = 0; i < old_size; ++i) dest[i] = source[i];
    (void)kfree(ptr);
    return new_ptr;
}
size_t heap_used_bytes(void) { return initialized ? live_bytes : 0; }
size_t heap_capacity_bytes(void) { return initialized ? heap_capacity : 0; }
size_t heap_free_bytes(void) {
    size_t sum = 0;
    for (struct block *b = first; b; b = b->next) if (!b->allocated) sum += b->capacity;
    return sum;
}
uint64_t heap_guard_low(void) { return initialized ? (uint64_t)heap_base - 4096 : 0; }
uint64_t heap_guard_high(void) { return initialized ? (uint64_t)(heap_base + heap_capacity) : 0; }
