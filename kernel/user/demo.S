/* Position-independent, <= one page, embedded user program. Copied to a U/S
 * executable page. All relative jumps/LEAs remain valid after relocation. */
.section .rodata.user_demo,"a",@progbits
.align 16
.global openxp_user_demo_start
.global openxp_user_demo_end
openxp_user_demo_start:
    lea demo_message(%rip), %rdi
    movq $demo_message_end-demo_message, %rsi
    movq $4, %rax                /* checked sys_write(user_ptr, length) */
    int $0x80
    cmpq $demo_message_end-demo_message, %rax
    jne 3f
    xorq %rax,%rax               /* sys_version */
    int $0x80
    cmpq $0x10000,%rax
    jne 3f
    movq $5,%rax                 /* sys_getpid: must identify this process */
    int $0x80
    testq %rax,%rax
    jz 3f
    movq $999,%rax               /* ENOSYS */
    int $0x80
    cmpq $-38,%rax
    jne 3f
    xorq %rdi,%rdi
    jmp 4f
3:
    movq $1,%rdi
4:
    movq $2,%rax                 /* sys_exit */
    int $0x80
5:  jmp 5b
 demo_message:
    .ascii "OpenXP userspace: Hello from private ELF64!\n"
 demo_message_end:
openxp_user_demo_end:

.align 16
.global openxp_user_fault_start
.global openxp_user_fault_end
openxp_user_fault_start:
    ud2                          /* deliberate CPL3 invalid-opcode exception */
openxp_user_fault_end:
.section .rodata.user_demo,"a",@progbits
.align 16
.global openxp_user_protection_start
.global openxp_user_protection_end
openxp_user_protection_start:
    movabsq $0xffffffff80000000, %rax
    movb (%rax), %al           /* should #PF: kernel text is supervisor-only */
1:
    jmp 1b
openxp_user_protection_end:

/* This tiny application needs timer interrupts to make progress. Running
 * two instances gives each a distinct PID, stack and user address space. */
.section .rodata.user_demo,"a",@progbits
.align 16
.global openxp_user_preempt_start
.global openxp_user_preempt_end
openxp_user_preempt_start:
    movq $3, %rax              /* SYS_TICKS */
    int $0x80
    movq %rax, %r12
1:
    movq $3, %rax
    int $0x80
    subq %r12, %rax
    cmpq $20, %rax             /* survive many timer interrupts */
    jb 1b
    movq $5, %rax              /* SYS_GETPID */
    int $0x80
    testq %rax, %rax
    jz 2f
    xorq %rdi, %rdi
    jmp 3f
2:
    movq $1, %rdi
3:
    movq $2, %rax              /* SYS_EXIT */
    int $0x80
4:
    jmp 4b
openxp_user_preempt_end:

.section .note.GNU-stack,"",@progbits
