/* x86-64 exception entries. CPU-supplied error codes are not duplicated.
 * Common frame: 15 saved GPRs, vector, error, RIP, CS, RFLAGS, RSP, SS.
 * We intentionally never return from a fatal exception in this milestone. */
.section .text
.macro ISR num has_error=0
.global isr_\num
.type isr_\num, @function
isr_\num:
.if !\has_error
    pushq $0
.endif
    pushq $\num
    jmp isr_common
.endm

ISR 0
ISR 1
ISR 2
ISR 3
ISR 4
ISR 5
ISR 6
ISR 7
ISR 8, 1
ISR 9
ISR 10, 1
ISR 11, 1
ISR 12, 1
ISR 13, 1
ISR 14, 1
ISR 15
ISR 16
ISR 17, 1
ISR 18
ISR 19
ISR 20
ISR 21, 1
ISR 22
ISR 23
ISR 24
ISR 25
ISR 26
ISR 27
ISR 28
ISR 29, 1
ISR 30, 1
ISR 31

.global isr_unexpected
.type isr_unexpected, @function
isr_unexpected:
    pushq $0
    pushq $255
    jmp isr_common

.type isr_common, @function
isr_common:
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
    call exception_dispatch
1:
    cli
    hlt
    jmp 1b
.section .note.GNU-stack,"",@progbits
