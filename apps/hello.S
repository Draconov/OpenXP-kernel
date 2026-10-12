/* Standalone, position-independent OpenXP app loaded from the IRFS image. */
.section .text
.global _start
_start:
    lea message(%rip), %rdi
    mov $message_end-message, %rsi
    mov $4, %rax
    int $0x80
    cmp $message_end-message, %rax
    jne failed
    mov $5, %rax
    int $0x80
    test %rax, %rax
    jz failed
    xor %rdi, %rdi
    jmp finish
failed:
    mov $1, %rdi
finish:
    mov $2, %rax
    int $0x80
1:  jmp 1b
message:
    .ascii "OpenXP initrd: Hello from a packaged ELF64 app!\n"
message_end:
.section .note.GNU-stack,"",@progbits
