ENTRY(_start)
SECTIONS {
    . = 0x100000;
    .text : { *(.text.start) *(.text .text.*) *(.rodata .rodata.*) *(.data .data.*) }
    . = ALIGN(4096);
    .bss (NOLOAD) : { *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON) }
    /DISCARD/ : { *(.note.*) *(.comment) *(.eh_frame*) *(.llvm*) }
}
