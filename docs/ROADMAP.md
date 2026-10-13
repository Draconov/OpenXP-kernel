# OpenXP development roadmap

The roadmap tracks **what exists, what is verified, and what remains**. It is the
single place for future plans. OpenXP is a research OS, **not a secure or
Windows-compatible daily-use system**.

## How progress is recorded

- **[x] Implemented:** the feature exists in source and passed its specified
  test. VM-dependent features are only checked once they have VM evidence.
- **[ ] Partial:** there is supporting code, but the full objective remains
  unimplemented, untested, or both.
- **[ ] Not started:** no supported implementation exists yet.

**Verification boundary:** the graphical ISO has booted in a user's VirtualBox
and normal QEMU desktop checks passed in a previous CI run, but this source
update's multi-process interactive behavior has *not* been verified in a VM.
Host compilation and unit tests cannot prove ring transitions, interrupt
preemption, protection or hardware input are correct.

## Platform and boot

- [x] Implement an original freestanding x86-64 ELF kernel.
- [x] Integrate Limine boot information, framebuffer and ISO build scripts.
- [x] Boot an XP-inspired desktop in VirtualBox (user-reported).
- [x] Create normal and deep/diagnostic QEMU smoke-test targets.
- [x] Set up GitHub Actions to build the ISO and upload build artifacts.
- [ ] Confirm that **all** current diagnostic CI tests pass together without
      unhandled faults, hangs or spurious timeouts. **Partial:** a normal boot
      previously passed; a deep scheduler fault was later diagnosed and patched.
- [ ] Retest GUI interaction, application launching and triple native windows
      in both VirtualBox and QEMU with the new ISO.
- [ ] Preserve serial traces and failure artifacts from every CI test;
      differentiate unverified, failed and passed integration checks.
- [ ] Verify BIOS and UEFI paths separately, including boot regression tests.

## Kernel foundations

- [x] Establish IDT, GDT, TSS and exception diagnostics.
- [x] Implement physical page allocation, four-level virtual mappings and a
      guarded kernel heap with reusable allocations (host tests).
- [x] Add kernel cooperative scheduling, PIC/PIT timer scheduling, sleeping
      kernel workers and basic mutexes/events/IPC (host tests).
- [x] Add provisional Ring 3 execution, system calls, guarded user/kernel
      stacks, private CR3 address spaces and a restricted ELF64 loader.
- [x] Track process IDs, completion/fault states and reclaim resources.
- [x] Add bounded user buffer-copy checks to existing syscalls (host tests).
- [x] Add a diagnostic `PS` command that queries real process slots.
- [ ] Fully verify privilege transitions, CR3/TSS preemption, exception
      recovery, stack guards and process cleanup under QEMU.
- [ ] Build robust asynchronous process wait/exit notifications, cancellation
      and parent/child relationships. **Partial:** timed polling-based waiting
      and process completion records already exist.
- [ ] Implement safe user-triggered spawn/exit/wait syscalls. **Partial:**
      launching currently requires the privileged desktop controller.
- [ ] Preserve FPU/SIMD state across context switches.
- [ ] Add per-process handle tables and safe capability-based IPC.
- [ ] Add priority scheduling, timeouts and deadlock diagnostics.
- [ ] Audit mapping inheritance, kernel entry stacks, exception context
      restoration and every syscall argument/permission boundary.

## Native graphical applications

- [x] Compile Calculator, Clock and Notepad as separate freestanding ELF64
      binaries and bundle them in the read-only boot archive.
- [x] Provide PID-checked GUI_OPEN, GUI_TEXT and GUI_EVENT calls.
- [x] Launch native programs on scheduler workers without blocking the desktop
      controller (source and host tests).
- [x] **This update:** support three separate PID-owned native text windows,
      individual event queues, title/taskbar routing, window close and
      lifecycle cleanup (source + host tests).
- [x] **This update:** route client-area mouse clicks as packed native events
      and keyboard events only to the focused native owner (host tests).
- [ ] Verify these changes end-to-end in QEMU/VirtualBox (especially three
      concurrent Ring 3 tasks and close-while-active behavior).
- [x] Implement **scheduler-backed native GUI event waits** with atomic
      check/park/wake, optional timer deadlines, sticky close, and conversion
      of Calculator/Clock/Notepad/Canvas away from busy-polling. Host-tested.
- [ ] Verify timed GUI waits from actual preemptible Ring 3 in QEMU/VirtualBox.
      A dedicated `make smoke-native-wait` integration target is provided.
- [x] Implement bounded **per-PID RGB565 drawing surfaces** and atomically
      validated batched draw commands (clear, rectangles, bitmap glyphs).
      Native Calculator, Clock, Notepad and Canvas now draw pixels themselves;
      these claims are compilation/host-tested, not VM-tested.
- [ ] Add standard buttons/widgets, per-application resizing, menus and
      scrollbars. **Partial:** Calculator draws its own buttons and handles
      client mouse events, but no shared widget toolkit exists.
- [ ] Move the graphical window server, shell and desktop controller to
      separate Ring 3 processes. **Not started:** all still run in Ring 0.
- [ ] Migrate *all* legacy built-ins (Paint, WordPad, Calendar, Mines, Snake,
      Settings, file manager, Terminal, etc.) without losing features.
- [ ] Add a stable SDK, dynamic libraries/resources and installable apps.
- [ ] Support full native ELF segments, relocations and separate on-disk
      executable loading rather than embedded files.

## Filesystems, storage and terminal

- [x] Mount a validated, read-only `OXRD` boot archive.
- [x] Implement a bounded writable RAM filesystem and Save/Open for Notepad,
      WordPad and Paint. **RAM contents disappear on reboot.**
- [x] Provide basic file-manager folder navigation, previews and launching
      bundled ELF samples.
- [x] Implement command history, file browsing/reading, arithmetic and Run.
- [x] **This update:** replace placeholder `PS` reporting with live process
      slot states and clarify that `RUN` is asynchronous.
- [x] Add a **minimal VFS namespace** joining read-only boot archive entries
      with writable `HOME` files; unify shell read/list, temporary file
      operations and native note storage behind it (host tests).
- [ ] Extend the VFS into genuine directories, file descriptors/handles,
      permissions, multiple mounts and safe open/read/write/stat APIs.
      **Partial:** the new VFS is a simple path API, not a disk filesystem.
- [x] **This update:** implement `COPY`, `REN` and `DEL` for temporary
      `HOME` files in the kernel-hosted terminal, with no-overwrite and path
      validation tests. These operations never touch the boot archive/disk.
- [ ] Add real directory creation, Save As, GUI copy/rename/delete, and
      persistent file operations. **Partial:** RAM file operations exist.
- [x] Implement a small **two-slot CRC32 disk snapshot serializer** and
      host-side interrupted-write recovery tests, with a fresh-disk creator.
- [ ] Verify and harden an **opt-in secondary-IDE ATA PIO driver** in a VM,
      using a dedicated formatted virtual disk only. Default ISO has disk
      writes disabled. This is not yet generally supported persistent storage.
- [ ] Implement a genuine multi-file VFS, directories, permissions, block
      cache and robust filesystem. The snapshot backend is only experimental.
- [ ] Move the terminal interpreter to Ring 3; use syscalls for file/process
      access instead of calling privileged service callbacks.
- [ ] Separate system/application binaries, user data, settings and cache.

## Desktop, peripherals and performance

- [x] Implement a framebuffer XP-inspired shell, Start menu, taskbar,
      draggable/minimizable windows, shortcuts, PS/2 input and mouse cursor.
- [x] Add dirty-rectangle redraw paths and IRQ-backed PS/2 input with
      polling fallback. **Partial:** full compositor/frame pacing is absent.
- [x] Add kernel-hosted utilities and games (Notepad, Calculator, Paint,
      WordPad, Calendar, Snake, Mines and others).
- [ ] Complete repaint clipping, resize behavior, scaling and tearing control.
- [ ] Expand keyboard layouts and localization, key repeat and high-resolution
      pointer handling.
- [x] Add read-only legacy PCI configuration-space discovery, bridge
      traversal and USB/network/audio/graphics class reporting (host tests).
- [ ] Add USB host-controller and HID drivers; PCI discovery is not USB support.
- [ ] Add working sound drivers, NIC drivers, network protocols and services.
      **Partial:** PCI device-class discovery only; no usable audio/network.
- [ ] Develop a graphics driver model and compositing / accelerated rendering.
- [ ] Support persistent appearance/preferences and configurable desktop.

## Hardware, security and distribution

- [ ] Parse ACPI and use APIC/IOAPIC interrupt routing; design SMP and
      per-CPU scheduling with correct cross-CPU memory synchronization.
- [ ] Expand UEFI, NVMe, storage, network and GPU hardware compatibility.
- [ ] Add authentication, permissions, service boundaries, least privilege and
      a trustworthy update/rollback mechanism.
- [ ] Fuzz syscall, filesystem and executable parsers, with independent
      reviews of kernel memory safety and privilege boundaries.
- [ ] Build reproducible/signed releases, installers, support matrices and a
      vulnerability reporting process.
- [ ] Research legally clean Win32/PE compatibility. **Not implemented:**
      native OpenXP ELF binaries are not Windows `.exe` programs.

## Current next-up work

1. **Validate multi-process GUI in the VM**: concurrently launch
   `RUN BIN/CALCULATOR`, `RUN BIN/CLOCK` and `RUN BIN/NOTEPAD`, type into
   separate windows, close and relaunch each, check `PS` and QEMU logs.
2. Validate Ring 3 native Calculator, Clock, Notepad and Canvas drawing,
   focus, typing and close/relaunch in QEMU and VirtualBox.
3. Validate the new atomic GUI_WAIT in QEMU (`make smoke-native-wait`),
   then add process handles, event queue permissions, and user-mode desktop
   and window-server processes. The compositor still lives in Ring 0.
4. Migrate legacy applications individually and remove Ring 0 UI code only
   after behavioral parity and isolation tests.
5. Verify the opt-in secondary IDE disk against a disposable VM image, then
   extend the VFS path abstraction with file handles, directory structures,
   disk I/O errors, and crash/power-loss tests.
6. Implement real xHCI/USB HID, NIC/network, audio, accelerated graphics,
   ACPI/APIC and SMP subsystems incrementally; detection is not a driver.

A checkmark means the specified scope exists and has the indicated evidence;
it is **not** a claim that the entire operating system is finished.
