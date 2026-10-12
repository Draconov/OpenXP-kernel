#include "user/elf.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include <stdint.h>
#include <stddef.h>
extern void *memcpy(void *, const void *, size_t);

struct elf64_header {
    uint8_t ident[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct elf64_program {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
};
_Static_assert(sizeof(struct elf64_header) == 64, "ELF64 header size");
_Static_assert(sizeof(struct elf64_program) == 56, "ELF64 program header size");
#define ELF_PT_LOAD 1u
#define ELF_PF_X 1u
#define ELF_PF_W 2u
#define ELF_PF_R 4u
#define MAX_PROGRAM_HEADERS 16u
#define MAX_SEGMENT_BYTES (1024u * 1024u)
#define MAX_LOADED_PAGES 256u

static bool within(size_t total, uint64_t offset, uint64_t size) {
    return offset <= total && size <= (uint64_t)total - offset;
}
static bool power2(uint64_t x) { return x && !(x & (x-1)); }

bool elf64_load(address_space_t *s, const void *image, size_t length, uint64_t *entry) {
    if (!s || !s->cr3 || !image || !entry || length < sizeof(struct elf64_header)) return false;
    const uint8_t *bytes = image;
    struct elf64_header h;
    memcpy(&h, bytes, sizeof(h));
    if (h.ident[0] != 0x7f || h.ident[1] != 'E' || h.ident[2] != 'L' ||
        h.ident[3] != 'F' || h.ident[4] != 2 || h.ident[5] != 1 ||
        h.ident[6] != 1 || (h.type != 2 && h.type != 3) || h.machine != 62 || h.version != 1 ||
        h.ehsize != sizeof(h) || h.phentsize != sizeof(struct elf64_program) ||
        !h.phnum || h.phnum > MAX_PROGRAM_HEADERS ||
        !within(length, h.phoff, (uint64_t)h.phnum * sizeof(struct elf64_program))) return false;
    uint64_t base = vmm_user_base();
    if (!base) return false;
    /* ET_DYN is supported only as a static, relocation-free position-independent
     * image. Its addresses are offsets relative to the chosen user slot;
     * there is no interpreter, dynamic section, or relocation processing. */
    const bool relative = h.type == 3;
    if (relative && (h.entry >= 4ULL * 1024 * 1024 ||
                     base > UINT64_MAX - h.entry)) return false;
    const uint64_t real_entry = relative ? base + h.entry : h.entry;
    unsigned loaded_pages = 0;
    bool entry_executable = false;
    for (unsigned i = 0; i < h.phnum; ++i) {
        struct elf64_program p;
        memcpy(&p, bytes + (size_t)h.phoff + i * sizeof(p), sizeof(p));
        if (p.type != ELF_PT_LOAD) return false; /* reject PT_DYNAMIC, PT_INTERP, ... */
        if (relative) {
            if (p.vaddr >= 4ULL * 1024 * 1024 || base > UINT64_MAX - p.vaddr)
                return false;
            p.vaddr += base;
        }
        if (!p.memsz || p.memsz > MAX_SEGMENT_BYTES || p.filesz > p.memsz ||
            !within(length, p.offset, p.filesz) ||
            !(p.flags & ELF_PF_R) || (p.flags & ~(ELF_PF_R | ELF_PF_W | ELF_PF_X)) ||
            ((p.flags & ELF_PF_W) && (p.flags & ELF_PF_X)) ||
            (p.align != 0 && p.align != 1 && !power2(p.align)) ||
            (p.align > 1 && (p.vaddr % p.align) != (p.offset % p.align)) ||
            p.vaddr < base || p.vaddr - base >= 4ULL * 1024 * 1024 ||
            p.memsz > 4ULL * 1024 * 1024 - (p.vaddr - base)) return false;
        uint64_t first = p.vaddr & ~4095ULL;
        uint64_t last = (p.vaddr + p.memsz - 1) & ~4095ULL;
        uint64_t pages = (last - first)/4096 + 1;
        if (pages > MAX_LOADED_PAGES - loaded_pages) return false;
        loaded_pages += (unsigned)pages;
        unsigned permissions = (p.flags & ELF_PF_W ? VMM_PAGE_WRITE : 0) |
                               (p.flags & ELF_PF_X ? VMM_PAGE_EXEC : 0);
        /* Each segment owns unique pages. Reject overlaps rather than
         * implicitly widening permissions of an existing mapping. */
        for (uint64_t va=first; va<=last; va+=4096) {
            uint64_t pa = pmm_alloc_zeroed_page();
            if (!pa) return false;
            if (!aspace_map(s, va, pa, permissions)) {
                (void)pmm_free_page(pa);
                return false;
            }
        }
        if (p.filesz && !aspace_load_image(s, p.vaddr, bytes + p.offset, (size_t)p.filesz))
            return false;
        if ((p.flags & ELF_PF_X) && real_entry >= p.vaddr &&
            real_entry - p.vaddr < p.memsz) entry_executable = true;
    }
    if (!entry_executable) return false;
    *entry = real_entry;
    return true;
}
