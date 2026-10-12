# Roadmap

This document is the **single place for future work, proposed features and
readiness goals**. Features described here are plans, not completed
capabilities. Progress is measured through verified behavior.

OpenXP aims to become a small, reliable, open-source PC operating system with
an approachable XP-inspired desktop. Windows program compatibility is a
long-term exploration, **not** an existing feature.

## Priority 1 — Prove the boot path

**Goal:** Boot and interact with the current desktop in a disposable virtual machine.

- [ ] Build the complete ISO in GitHub Actions and confirm it boots in QEMU.
- [ ] Capture serial traces for normal boot and every diagnostic smoke test.
- [ ] Verify timer IRQ delivery, CR3/TSS switching and Ring 3 fault recovery.
- [ ] Check real keyboard/mouse interaction, window dragging and redraws.
- [ ] Test successful and failed application launches without corrupting state.
- [ ] Fix hangs, page faults, allocator errors and regressions found in VM testing.
- [ ] Make boot checks deterministic; keep artifacts and diagnostic logs accessible.

**Completion criteria:** A repeatable VM boot, interactive desktop, and passing
QEMU tests with no unexplained fault or silent timeout.

## Priority 2 — Make processes robust

**Goal:** Run independent applications without risking the kernel's state.

- [ ] Audit privilege separation and all inherited kernel page mappings.
- [ ] Remove unsafe dependencies between the GUI, command interpreter and Ring 0.
- [ ] Add event-driven process completion, waiting and cleanup.
- [ ] Implement process creation and termination through reviewed syscalls.
- [ ] Improve stack protections, exception containment and user-buffer validation.
- [ ] Preserve FPU/SIMD context during task switches.
- [ ] Add user-level handles, controlled memory sharing and safer IPC.
- [ ] Extend synchronization with timeouts, cancellation and priority handling.

**Completion criteria:** Multiple independently scheduled user tasks can start,
wait, exit or fail, and their resources are reliably reclaimed in QEMU tests.

## Priority 3 — Filesystem and command environment

**Goal:** Make the shell useful beyond bundled demonstration files.

- [ ] Introduce a small VFS with file handles, directories and common operations.
- [ ] Provide writable temporary storage and then a supported disk filesystem.
- [ ] Load executable images by path without embedding every file in the kernel.
- [ ] Move the command interpreter into userspace.
- [ ] Add process listing, file commands and meaningful error reporting.
- [ ] Design a stable syscall ABI and application development interface.

**Completion criteria:** A user process can read files and launch another
program through validated kernel interfaces.

## Priority 4 — Desktop and device support

**Goal:** Turn the present graphical demonstration into a useful desktop.

- [ ] Move the desktop and window manager into isolated user processes.
- [ ] Add event queues, surface ownership and window-to-application messaging.
- [ ] Implement keyboard layouts, reliable key repeat and broader pointer input.
- [ ] Add device discovery and USB keyboard/mouse support.
- [ ] Introduce persistent settings, a working file manager and native apps.
- [ ] Improve redraw performance, fonts, display scaling and screen modes.
- [ ] Investigate framebuffer acceleration before attempting GPU drivers.

**Completion criteria:** Several independent graphical applications remain
responsive and isolated while users interact through normal input devices.

## Priority 5 — Modern hardware and security

**Goal:** Support newer virtual and physical platforms only after core stability.

- [ ] Discover ACPI tables and introduce APIC/IOAPIC-based interrupt routing.
- [ ] Design per-CPU data, SMP scheduling and cross-CPU memory synchronization.
- [ ] Add UEFI testing, storage drivers, networking and audio incrementally.
- [ ] Design a trusted update path, permissions model and stronger memory defenses.
- [ ] Fuzz parsers and syscall interfaces; review security-critical changes.
- [ ] Establish reproducible builds, hardware compatibility reports and issue triage.

**Completion criteria:** Measured platform support and security properties,
backed by repeatable tests and external review rather than feature checkboxes.

## Longer-term research

- [ ] Explore Windows-compatible APIs and applications without using proprietary
      Microsoft code, assets or drivers.
- [ ] Create a supported driver model and hardware abstraction layer.
- [ ] Evaluate modern graphics acceleration and a composited desktop.
- [ ] Design distribution packaging, installers and signed releases.

## How to work on the roadmap

1. Choose one task with a reproducible test or observable result.
2. Open a GitHub issue describing scope, expected behavior and known risks.
3. Implement the smallest reviewable change; document any new interface.
4. Run host tests, sanitizers and the relevant QEMU integration test.
5. Mark an item complete only after the required evidence is available.

This page tracks **plans only**. The [README](../README.md) explains how to
build the current source; the [architecture guide](ARCHITECTURE.md) describes
implemented components and their limitations.
