#ifndef OPENXP_MM_PMM_H
#define OPENXP_MM_PMM_H
#include <stdbool.h>
#include <stdint.h>
#include "limine_min.h"

#define PMM_PAGE_SIZE 4096ULL
/* Single CPU only. Uses Limine's HHDM; cannot survive replacing page tables
 * unless the HHDM mapping is preserved. All addresses in alloc/free are PHYSICAL. */
bool pmm_init(const struct limine_memmap_response *map, uint64_t hhdm_offset);
uint64_t pmm_alloc_page(void); /* Returns 0 on failure; page 0 never allocated. */
uint64_t pmm_alloc_zeroed_page(void);
bool pmm_free_page(uint64_t physical_address);
void *pmm_direct_map(uint64_t physical_address);
uint64_t pmm_total_pages(void);
uint64_t pmm_free_pages(void);
#endif
