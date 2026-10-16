#ifndef OPENXP_USER_PROCESS_LAYOUT_H
#define OPENXP_USER_PROCESS_LAYOUT_H

/* Each Ring 3 process owns a private 4 MiB virtual address range. Native ELF
 * images begin at offset 0x100000 and windowd reserves up to a 2 MiB RW segment for larger native surfaces.
 * Keep the guarded user stack near the end of the range: placing it just
 * above the native code (the former 0x110000 position) overlaps windowd's
 * data pages and prevents that service from launching. */
#define PROCESS_USER_SLOT_BYTES (4ULL * 1024ULL * 1024ULL)
#define PROCESS_USER_STACK_GUARD_OFFSET 0x3f0000ULL
#define PROCESS_USER_STACK_PAGES 4u
#define PROCESS_USER_PAGE_BYTES 4096ULL

_Static_assert(PROCESS_USER_STACK_GUARD_OFFSET % PROCESS_USER_PAGE_BYTES == 0,
               "user stack guard must be page aligned");
_Static_assert(PROCESS_USER_STACK_GUARD_OFFSET +
               (PROCESS_USER_STACK_PAGES + 2u) * PROCESS_USER_PAGE_BYTES <=
               PROCESS_USER_SLOT_BYTES, "user stack must fit in process slot");
#endif
