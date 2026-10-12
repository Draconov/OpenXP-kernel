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
