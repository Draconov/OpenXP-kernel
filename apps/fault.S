/* Deliberately fault inside CPL3; the kernel must remain alive. */
.section .text
.global _start
_start:
    ud2
1:  jmp 1b
.section .note.GNU-stack,"",@progbits
