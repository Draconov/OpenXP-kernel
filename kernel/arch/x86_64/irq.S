/* IRQ0: save GPRs on CPL0 or CPL3 timer interruptions. CPL3 iret frames
 * additionally contain user RSP/SS, restored automatically by iretq.
 * Every integer GPR is saved in the same order as exception_frame.
 * All registers, including volatile ones, are restored before iretq.
 * The returned RSP may point into a different, guarded kernel stack.
 * The CPU always pushes RSP and SS in x86-64 mode, including CPL0->CPL0.
 * IRETQ consumes these fields; fabricated first-run frames must have them.
 * IRQ0 is a CPL0-only, no-error-code interrupt gate; never enable IF here. */
.section .text
.global irq_timer_entry
.type irq_timer_entry, @function
irq_timer_entry:
    pushq $0
    pushq $32
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
    movq %rsp, %rdi
    andq $-16, %rsp
    call timer_irq_dispatch
    movq %rax, %rsp
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
    addq $16, %rsp /* vector + error */
    iretq
.size irq_timer_entry, .-irq_timer_entry
.section .note.GNU-stack,"",@progbits
