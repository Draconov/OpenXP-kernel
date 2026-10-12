#ifndef OPENXP_X86_64_GDT_H
#define OPENXP_X86_64_GDT_H
#include <stdint.h>
#define GDT_KERNEL_CS 0x08u
#define GDT_KERNEL_DS 0x10u
#define GDT_TSS_SELECTOR 0x28u
#define GDT_USER_DS 0x1bu
#define GDT_USER_CS 0x23u
/* Loads a private 64-bit GDT and TSS; sets IST1/2/3 and RSP0.
 * Interrupts must stay disabled until the IDT has been installed. */
void gdt_init(void);
/* Relocate four TSS stacks into kernel-owned guarded virtual mappings. */
#include <stdbool.h>
bool gdt_install_guarded_stacks(void);
uint16_t gdt_task_register(void);
/* UP only: switch the kernel-entry stack before entering CPL3, IF=0. */
uint64_t gdt_kernel_stack_top(void);
bool gdt_set_kernel_stack(uint64_t stack_top);
#endif
