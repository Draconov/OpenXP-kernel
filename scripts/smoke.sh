#!/usr/bin/env bash
set -euo pipefail
ISO="${1:-build/openxp.iso}"
EXPECTED="${2:-OPENXP_BOOT_OK}"
command -v qemu-system-x86_64 >/dev/null || { echo 'ERROR: install qemu-system-x86_64' >&2; exit 1; }
[[ -f "$ISO" ]] || { echo "ERROR: no ISO at $ISO" >&2; exit 1; }
LOG="$(mktemp)"; trap 'rm -f "$LOG"' EXIT
# The kernel intentionally halts with QEMU running; timeout exit 124 is normal.
code=0
timeout 15s qemu-system-x86_64 -m 512M -cdrom "$ISO" -display none -serial "file:$LOG" -no-reboot -monitor none || code=$?
cat "$LOG"
if [[ "$code" != 0 && "$code" != 124 ]]; then exit "$code"; fi
if [[ "$EXPECTED" == OPENXP_BOOT_OK || "$EXPECTED" == OPENXP_NATIVE_ABI_OK || "$EXPECTED" == OPENXP_NATIVE_WAIT_OK || "$EXPECTED" == OPENXP_CAPIPC_OK ]]; then
    REQUIRED=(OPENXP_GDT_OK OPENXP_IDT_OK OPENXP_PMM_OK OPENXP_VMM_OK OPENXP_HEAP_OK OPENXP_STACKS_OK OPENXP_BOOT_OK OPENXP_INPUT_OK OPENXP_GUI_OK OPENXP_SHELL_OK OPENXP_WM_OK "$EXPECTED")
else
    REQUIRED=(OPENXP_GDT_OK OPENXP_IDT_OK OPENXP_PMM_OK OPENXP_VMM_OK OPENXP_HEAP_OK OPENXP_STACKS_OK OPENXP_SCHED_OK OPENXP_REAP_OK OPENXP_IRQ_OK OPENXP_TIMER_OK OPENXP_CLOCK_OK OPENXP_SLEEP_OK OPENXP_BLOCK_OK OPENXP_BOOT_OK "$EXPECTED")
fi
for marker in "${REQUIRED[@]}"; do
    grep -q "$marker" "$LOG" || { echo "ERROR: missing boot marker $marker" >&2; exit 1; }
done
if [[ "$EXPECTED" == OPENXP_PROCESS_REAP_OK ]]; then
    for marker in OPENXP_PROCESS_PID_OK OPENXP_PROCESS_STACK_OK OPENXP_ELF64_OK OPENXP_PRIVATE_CR3_OK OPENXP_USERSPACE_OK; do
        grep -q "$marker" "$LOG" || { echo "ERROR: missing process marker $marker" >&2; exit 1; }
    done
    [[ "$(grep -c "OpenXP userspace: Hello from private ELF64!" "$LOG")" -eq 2 ]] || { echo "ERROR: expected two clean ELF launches" >&2; exit 1; }
fi
if [[ "$EXPECTED" == OPENXP_INITRD_OK ]]; then
    grep -q OPENXP_INITRD_ELF_OK "$LOG" || { echo 'ERROR: missing ELF loader marker' >&2; exit 1; }
    grep -q OPENXP_FAULT_ISOLATION_OK "$LOG" || { echo 'ERROR: missing fault recovery marker' >&2; exit 1; }
    [[ "$(grep -c 'OpenXP initrd: Hello from a packaged ELF64 app!' "$LOG")" -eq 2 ]] || { echo 'ERROR: expected two file-backed app launches' >&2; exit 1; }
fi
if [[ "$EXPECTED" == OPENXP_USER_PREEMPT_OK ]]; then
    grep -q OPENXP_PROCESS_WAIT_OK "$LOG" || { echo "ERROR: missing process wait marker" >&2; exit 1; }
    grep -q OPENXP_USER_CR3_SWITCH_OK "$LOG" || { echo 'ERROR: missing user CR3 preemption marker' >&2; exit 1; }
fi
if [[ "$EXPECTED" == OPENXP_EXCEPTION_OK || "$EXPECTED" == OPENXP_GUARD_OK ]]; then
    grep -q OPENXP_CRASH_DUMP_OK "$LOG" || { echo 'ERROR: missing fault register dump' >&2; exit 1; }
fi
if grep -q 'ERROR:' "$LOG"; then echo 'ERROR: kernel reported a failure' >&2; exit 1; fi
echo "PASS: OpenXP boot check ($EXPECTED)"
