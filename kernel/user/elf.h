#ifndef OPENXP_USER_ELF_H
#define OPENXP_USER_ELF_H
#include "mm/address_space.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Validated, bounded ELF64 ET_EXEC / AMD64 loader. PT_LOAD only; no relocations,
 * dynamic linker, TLS, interpreter, argv or shared objects. Caller destroys
 * entire address space on failure (partial pages may have been mapped). */
bool elf64_load(address_space_t *space, const void *image, size_t length, uint64_t *entry);
#endif
