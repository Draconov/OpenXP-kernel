/* Enter CPL3 via IRETQ. The saved kernel continuation belongs to the
 * process, NEVER a global variable: timer IRQs may switch between tasks.
 * System V args: user RIP, user RSP, pointer to continuation, enable IF. */
.section .text
.global arch_user_enter
.type arch_user_enter,@function
arch_user_enter:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, (%rdx)
    /* User stack/code live in low canonical memory, both U/S enabled. */
    pushq $0x1b                 /* ring-3 data selector */
    pushq %rsi                  /* ring-3 stack top */
    pushfq
    popq %rax
    andq $~(1<<9), %rax
    testl %ecx, %ecx
    jz 1f
    orq $(1<<9), %rax         /* preemptible Ring 3 */
1:
    orq $2, %rax
    pushq %rax
    pushq $0x23                 /* ring-3 code selector */
    pushq %rdi                  /* user entry */
    iretq
.size arch_user_enter,.-arch_user_enter

.global arch_user_return
.type arch_user_return,@function
arch_user_return:
    cli
    /* Resolve the continuation for the currently scheduled process.
     * IF remains disabled throughout this kernel return path. */
    andq $-16, %rsp
    call process_current_resume_sp
    testq %rax, %rax
    jz .Lbad_resume
    movq %rax, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    ret
.Lbad_resume:
    cli
    hlt
    jmp .Lbad_resume
.size arch_user_return,.-arch_user_return

/* Userspace interrupt gate vector 0x80. Full register restoration on normal
 * syscalls; on EXIT we deliberately drop the user hardware interrupt frame. */
.global irq_syscall_entry
.type irq_syscall_entry,@function
irq_syscall_entry:
    pushq $0
    pushq $128
    cld
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, %r12
    movq %rsp, %rdi
    andq $-16, %rsp
    call user_syscall_dispatch
    movq %rdx, 112(%r12)       /* overwrite saved RAX with syscall result */
    testq %rax, %rax
    jnz arch_user_return
    movq %r12, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    iretq
.size irq_syscall_entry,.-irq_syscall_entry
.section .note.GNU-stack,"",@progbits
