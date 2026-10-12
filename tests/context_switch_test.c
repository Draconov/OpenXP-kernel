/* Runs the exact kernel assembly context switch in a native userspace process.
 * This is not a replacement for VM IRQ or fault tests but does execute the
 * real save/restore stack and return path. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

extern void arch_context_switch(uint64_t *old_sp, uint64_t new_sp);
static uint8_t alternate_stack[32768] __attribute__((aligned(16)));
static uint64_t main_sp, alternate_sp;
static volatile unsigned event;

__attribute__((noreturn)) static void alternate_entry(void) {
    assert(event == 0);
    event = 1;
    arch_context_switch(&alternate_sp, main_sp);
    assert(event == 2);
    event = 3;
    arch_context_switch(&alternate_sp, main_sp);
    /* There must be no third resume. */
    __builtin_trap();
}
int main(void) {
    uint64_t top = (uint64_t)(uintptr_t)(alternate_stack + sizeof(alternate_stack));
    assert((top & 15u) == 0);
    uint64_t *frame = (uint64_t *)(uintptr_t)(top - 64);
    for (unsigned i = 0; i < 6; ++i) frame[i] = 0;
    frame[6] = (uint64_t)(uintptr_t)alternate_entry;
    frame[7] = 0;  /* alternate_entry is noreturn */
    alternate_sp = (uint64_t)(uintptr_t)frame;
    arch_context_switch(&main_sp, alternate_sp);
    assert(event == 1);
    event = 2;
    arch_context_switch(&main_sp, alternate_sp);
    assert(event == 3);
    puts("PASS: real x86-64 context switch resumes two independent stacks");
    return 0;
}
