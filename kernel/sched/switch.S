/* x86-64 SysV cooperative kernel thread switch.
 * Arguments: old_sp_ptr in RDI, new_sp in RSI.
 * Every switch happens with IF=0 on the same CPU and in one CR3.
 * Callee-saved integer registers preserved. Initial context is hand-built
 * to match this push/pop/ret sequence and the SysV stack-entry alignment.
 */
.section .text
.global arch_context_switch
.type arch_context_switch, @function
arch_context_switch:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, (%rdi)
    movq %rsi, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    ret
.size arch_context_switch, .-arch_context_switch
.section .note.GNU-stack,"",@progbits
