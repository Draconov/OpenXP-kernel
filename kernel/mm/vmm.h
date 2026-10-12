#ifndef OPENXP_MM_VMM_H
#define OPENXP_MM_VMM_H
#include <stdbool.h>
#include <stdint.h>

#define VMM_PAGE_SIZE 4096ULL
#define VMM_PAGE_WRITE (1u << 0)
#define VMM_PAGE_EXEC (1u << 1)
/* This mapper owns one unused upper-half PML4 slot. Existing boot mappings
 * are copied into privately allocated page-table frames, including huge-page
 * entries. Existing HHDM *data* aliases and their permissions are retained;
 * this does NOT establish complete W^X or process isolation. */
bool vmm_init(void);
uint64_t vmm_region_base(void);
bool vmm_map_page(uint64_t va, uint64_t pa, unsigned flags);
bool vmm_unmap_page(uint64_t va); /* Removes VA only; caller owns data frame. */
bool vmm_query_page(uint64_t va, uint64_t *pa, unsigned *flags);
#ifdef OPENXP_VMM_HOST_TEST
/* Supplies a mock page-table root without executing privileged instructions. */
bool vmm_test_attach(uint64_t root_pa, uint16_t unused_pml4_slot);
bool vmm_test_clone_root(uint64_t old_root_pa, uint64_t *new_root_pa);
void vmm_test_destroy_clone(uint64_t root_pa);
#endif
/* A single 4-MiB virtual user window shared in virtual address but mapped
 * to distinct physical backing by independent process CR3 roots. This API
 * also retains older single-root operations for the earlier unit tests. */
bool vmm_user_init(void);
uint64_t vmm_user_base(void);
bool vmm_user_map_page(uint64_t va, uint64_t pa, unsigned flags);
bool vmm_user_unmap_page(uint64_t va);
bool vmm_user_query_page(uint64_t va, uint64_t *pa, unsigned *flags);

#endif /* OPENXP_MM_VMM_H */
