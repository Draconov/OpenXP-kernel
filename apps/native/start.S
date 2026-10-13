/* ELF process entry: CPU gives a 16-byte-aligned user stack. Calling a C
 * function creates the System V AMD64 ABI's expected 8-byte return slot. */
.section .text.start,"ax"
.global _start
.type _start,@function
_start:
    call oxp_main
1:  pause
    jmp 1b
.size _start, .-_start
.section .note.GNU-stack,"",@progbits
