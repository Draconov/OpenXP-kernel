#include "mm/pmm.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_REGIONS 128
struct region {
    uint64_t first, pages, bitmap_start;
};
static struct region regions[MAX_REGIONS];
static unsigned region_count;
static uint64_t total_pages, free_pages, next_hint, direct_map_offset;
static uint64_t metadata_physical, metadata_pages;
static uint8_t *bitmap;
static bool initialized;

static bool bit_get(uint64_t i) { return (bitmap[i >> 3] & (uint8_t)(1u << (i & 7))) != 0; }
static void bit_set(uint64_t i) { bitmap[i >> 3] |= (uint8_t)(1u << (i & 7)); }
static void bit_clear(uint64_t i) { bitmap[i >> 3] &= (uint8_t)~(1u << (i & 7)); }

bool pmm_init(const struct limine_memmap_response *map, uint64_t hhdm_offset) {
    if (initialized || !map || !map->entries) return false;
    region_count = 0;
    total_pages = free_pages = 0;
    /* Only whole, usable 4 KiB frames are considered. Reject unsafe maps. */
    for (uint64_t i = 0; i < map->entry_count; ++i) {
        const struct limine_memmap_entry *entry = map->entries[i];
        if (!entry || entry->type != LIMINE_MEMMAP_USABLE || !entry->length) continue;
        if (entry->base > UINT64_MAX - entry->length || entry->base > UINT64_MAX - 4095) return false;
        uint64_t start = (entry->base + 4095) & ~(PMM_PAGE_SIZE - 1);
        uint64_t end = (entry->base + entry->length) & ~(PMM_PAGE_SIZE - 1);
        if (start == 0) start = PMM_PAGE_SIZE; /* frame zero is an allocation-failure sentinel */
        if (end <= start) continue;
        if (region_count == MAX_REGIONS) return false;
        uint64_t pages = (end - start) / PMM_PAGE_SIZE;
        if (total_pages > UINT64_MAX - pages) return false;
        /* No overlapping usable regions, even if the memory map is unsorted. */
        for (unsigned j = 0; j < region_count; ++j) {
            uint64_t r_end = regions[j].first + regions[j].pages * PMM_PAGE_SIZE;
            if (start < r_end && end > regions[j].first) return false;
        }
        regions[region_count++] = (struct region){ start, pages, total_pages };
        total_pages += pages;
    }
    if (!total_pages || total_pages > UINT64_MAX - 7) return false;
    uint64_t bitmap_bytes = (total_pages + 7) / 8;
    if (bitmap_bytes > UINT64_MAX - 4095) return false;
    metadata_pages = (bitmap_bytes + 4095) / PMM_PAGE_SIZE;
    if (!metadata_pages) return false;
    unsigned storage = region_count;
    for (unsigned j = 0; j < region_count; ++j) {
        if (regions[j].pages >= metadata_pages) { storage = j; break; }
    }
    if (storage == region_count) return false;
    metadata_physical = regions[storage].first;
    if (hhdm_offset > UINT64_MAX - metadata_physical) return false;
    direct_map_offset = hhdm_offset;
    bitmap = (uint8_t *)(uintptr_t)(hhdm_offset + metadata_physical);
    for (uint64_t i = 0; i < bitmap_bytes; ++i) bitmap[i] = 0;
    for (uint64_t i = 0; i < metadata_pages; ++i)
        bit_set(regions[storage].bitmap_start + i);
    free_pages = total_pages - metadata_pages;
    next_hint = 0;
    initialized = true;
    return true;
}

uint64_t pmm_alloc_page(void) {
    if (!initialized || !free_pages) return 0;
    uint64_t index = next_hint;
    for (uint64_t scanned = 0; scanned < total_pages; ++scanned) {
        if (index >= total_pages) index = 0;
        if (!bit_get(index)) {
            for (unsigned r = 0; r < region_count; ++r) {
                uint64_t begin = regions[r].bitmap_start;
                if (index >= begin && index - begin < regions[r].pages) {
                    bit_set(index);
                    --free_pages;
                    next_hint = index + 1;
                    return regions[r].first + (index - begin) * PMM_PAGE_SIZE;
                }
            }
            return 0; /* inconsistent internal state */
        }
        ++index;
    }
    return 0;
}

void *pmm_direct_map(uint64_t physical_address) {
    if (!initialized || direct_map_offset > UINT64_MAX - physical_address) return NULL;
    return (void *)(uintptr_t)(direct_map_offset + physical_address);
}
uint64_t pmm_alloc_zeroed_page(void) {
    uint64_t pa = pmm_alloc_page();
    if (!pa) return 0;
    uint8_t *ptr = (uint8_t *)pmm_direct_map(pa);
    if (!ptr) { pmm_free_page(pa); return 0; }
    for (unsigned i = 0; i < PMM_PAGE_SIZE; ++i) ptr[i] = 0;
    return pa;
}
bool pmm_free_page(uint64_t physical_address) {
    if (!initialized || !physical_address || (physical_address & (PMM_PAGE_SIZE - 1))) return false;
    if (physical_address >= metadata_physical &&
        (physical_address - metadata_physical) / PMM_PAGE_SIZE < metadata_pages) return false;
    for (unsigned r = 0; r < region_count; ++r) {
        const uint64_t begin = regions[r].first;
        if (physical_address < begin) continue;
        uint64_t page_index = (physical_address - begin) / PMM_PAGE_SIZE;
        if (page_index < regions[r].pages) {
            uint64_t bit = regions[r].bitmap_start + page_index;
            if (!bit_get(bit)) return false; /* reject double-free and never-allocated page */
            bit_clear(bit);
            ++free_pages;
            if (bit < next_hint) next_hint = bit;
            return true;
        }
    }
    return false;
}
uint64_t pmm_total_pages(void) { return initialized ? total_pages : 0; }
uint64_t pmm_free_pages(void) { return initialized ? free_pages : 0; }
