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
