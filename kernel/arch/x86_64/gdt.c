#include "arch/x86_64/gdt.h"
#include "serial.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include <stddef.h>
#include <stdint.h>

struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t reserved1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t reserved2;
    uint16_t reserved3, iomap_base;
} __attribute__((packed));
struct descriptor_pointer { uint16_t limit; uint64_t base; } __attribute__((packed));
_Static_assert(sizeof(struct tss64) == 104, "x86-64 TSS must be 104 bytes");
_Static_assert(offsetof(struct tss64, ist1) == 36, "IST offset must match CPU layout");
_Static_assert(sizeof(struct descriptor_pointer) == 10, "GDTR must be 10 bytes");

/* GDT: null, ring-0 code/data, ring-3 data/code (reserved), 16-byte TSS. */
static uint64_t gdt[7] __attribute__((aligned(16))) = {
    0,
    0x00af9a000000ffffULL,
    0x00cf92000000ffffULL,
    0x00cff2000000ffffULL,
    0x00affa000000ffffULL,
    0, 0
};
static struct tss64 tss __attribute__((aligned(16)));
/* Dedicated exception stacks, plus a future ring-3 -> ring-0 transition stack.
 * These are statically allocated inside the kernel's mapped .bss segment. */
#define STACK_BYTES (16 * 1024)
static uint8_t rsp0_stack[STACK_BYTES] __attribute__((aligned(16)));
static uint8_t df_stack[STACK_BYTES] __attribute__((aligned(16)));
static uint8_t nmi_stack[STACK_BYTES] __attribute__((aligned(16)));
static uint8_t mc_stack[STACK_BYTES] __attribute__((aligned(16)));

uint16_t gdt_task_register(void) {
    uint16_t selector;
    __asm__ volatile ("str %0" : "=r"(selector));
    return selector;
}

void gdt_init(void) {
    __asm__ volatile ("cli" ::: "memory");
    tss.rsp0 = (uint64_t)(uintptr_t)(rsp0_stack + sizeof(rsp0_stack));
    tss.ist1 = (uint64_t)(uintptr_t)(df_stack + sizeof(df_stack));
    tss.ist2 = (uint64_t)(uintptr_t)(nmi_stack + sizeof(nmi_stack));
    tss.ist3 = (uint64_t)(uintptr_t)(mc_stack + sizeof(mc_stack));
    tss.iomap_base = sizeof(tss); /* no I/O permission bitmap */

    uint64_t base = (uint64_t)(uintptr_t)&tss;
    uint64_t limit = sizeof(tss) - 1;
    gdt[5] = (limit & 0xffffu) | ((base & 0xffffffu) << 16) |
             (0x89ULL << 40) | (((limit >> 16) & 0xfu) << 48) |
             (((base >> 24) & 0xffu) << 56);
    gdt[6] = base >> 32;
    const struct descriptor_pointer gdtr = {
        .limit = (uint16_t)(sizeof(gdt) - 1), .base = (uint64_t)(uintptr_t)gdt
    };
    __asm__ volatile ("lgdt %0" : : "m"(gdtr) : "memory");
    /* Reload CS through a 64-bit far return. Reload all data selectors as well. */
    __asm__ volatile (
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n"
        "1:\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%ss\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw $0x28, %%ax\n\t"
        "ltr %%ax\n\t"
        : : : "rax", "memory"
    );
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    if (cs != GDT_KERNEL_CS || gdt_task_register() != GDT_TSS_SELECTOR) {
        serial_puts("ERROR: GDT/TSS selector validation failed\n");
        kernel_halt();
    }
    serial_puts("GDT and TSS installed (DF/NMI/MC dedicated IST stacks)\nOPENXP_GDT_OK\n");
}

/* Reserve non-overlapping 4-page stacks with a lower and an upper unmapped
 * guard for TSS RSP0, #DF, NMI, and #MC. The current boot stack is untouched.
 * Called after VMM initialization, with interrupts disabled. The statically
 * allocated early IST stacks remain mapped but become unused. */
#define STACK_PAGES 4u
#define STACK_SLOTS 4u
#define STACK_STRIDE (STACK_PAGES + 2u)
#define STACK_OFFSET (1024u * 1024u)

bool gdt_install_guarded_stacks(void) {
    uint64_t base = vmm_region_base();
    if (!base) return false;
    uint64_t pages[STACK_SLOTS][STACK_PAGES] = {{0}};
    for (unsigned slot = 0; slot < STACK_SLOTS; ++slot) {
        uint64_t lower = base + STACK_OFFSET + (uint64_t)slot * STACK_STRIDE * VMM_PAGE_SIZE;
        if (vmm_query_page(lower, NULL, NULL) ||
            vmm_query_page(lower + (STACK_PAGES + 1u) * VMM_PAGE_SIZE, NULL, NULL)) goto rollback;
        for (unsigned page = 0; page < STACK_PAGES; ++page) {
            uint64_t va = lower + (page + 1u) * VMM_PAGE_SIZE;
            uint64_t pa = pmm_alloc_zeroed_page();
            if (!pa) goto rollback;
            if (!vmm_map_page(va, pa, VMM_PAGE_WRITE)) {
                (void)pmm_free_page(pa);
                goto rollback;
            }
            pages[slot][page] = pa;
        }
    }
    uint64_t tops[STACK_SLOTS];
    for (unsigned slot = 0; slot < STACK_SLOTS; ++slot)
        tops[slot] = base + STACK_OFFSET + (uint64_t)(slot * STACK_STRIDE + STACK_PAGES + 1u) * VMM_PAGE_SIZE;
    tss.rsp0 = tops[0];
    tss.ist1 = tops[1];
    tss.ist2 = tops[2];
    tss.ist3 = tops[3];
    serial_puts("TSS switched to four guarded stacks (RSP0/DF/NMI/MC)\nOPENXP_STACKS_OK\n");
    return true;
rollback:
    for (unsigned slot = 0; slot < STACK_SLOTS; ++slot)
        for (unsigned page = 0; page < STACK_PAGES; ++page)
            if (pages[slot][page]) {
                uint64_t va = base + STACK_OFFSET +
                    (uint64_t)(slot * STACK_STRIDE + page + 1u) * VMM_PAGE_SIZE;
                (void)vmm_unmap_page(va);
                (void)pmm_free_page(pages[slot][page]);
            }
    return false;
}

/* A distinct TSS RSP0 is required for each user process so an interrupt
 * cannot overwrite another process's kernel entry frame. The caller owns
 * the mapping and ensures stack memory remains alive through the return. */
uint64_t gdt_kernel_stack_top(void) { return tss.rsp0; }
bool gdt_set_kernel_stack(uint64_t top) {
    if (!top || (top & 15u)) return false;
    tss.rsp0 = top;
    return true;
}
