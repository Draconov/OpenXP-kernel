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
- [ ] Implement a general user process lifecycle and spawn/wait API.
      **Partial:** Ring 3 now submits allowlisted executable paths to a
      one-entry launch queue; only the boot thread creates new processes.
      No arbitrary binary execution, process handles or parent/child waits.
- [ ] Preserve FPU/SIMD state across context switches.
- [x] Add **bounded process-owned IPC endpoint handles**, per-PID access checks,
      generation-based stale-handle rejection, and copied message buffers
      (host tests and sanitizers).
- [ ] Extend these into general-purpose per-process handle tables, robust
      capability delegation, and audited permission/identity policies.
      **Partial:** the initial transport has one kernel-authorized singleton
      service, fixed global endpoints, and no delegation or SMP support.
- [ ] Add priority scheduling, timeouts and deadlock diagnostics.
- [ ] Audit mapping inheritance, kernel entry stacks, exception context
      restoration and every syscall argument/permission boundary.

## Native graphical applications

- [x] Add five separate native ELF64 apps: Calendar, Mines, Snake,
      System Monitor and About, with independent process memory and
      bounded per-PID drawing surfaces. Host builds/tests pass; VM pending.
- [x] Deliver native directional key and right-click events to the focused
      PID-owned window; host tests verify per-owner routing.
- [x] Add read-only RTC-date and free-memory system calls for native apps,
      with host regression checks.
- [ ] Complete native feature parity for existing programs; Ring 0 versions
      remain as fallbacks (the native Clock lacks stopwatch/timer functions).
- [x] Compile separate Ring 3 source applications for Paint, WordPad,
      Terminal, File Manager, Run and Settings with bounded graphics and VFS
      calls; retain the legacy defaults. Host builds/tests pass; VM pending.
- [x] Prefer native versions of Paint, WordPad, Terminal, File Manager, Run
      and Settings from Start, with Ring 0 fallbacks on launch failure.
- [x] Add native Settings IPC theme requests, three Ring 3 desktop palettes,
      fallback-theme synchronization, RAM preference save/reload (host-tested).
- [x] Expand native Paint to eight colors with backwards-compatible OXP1
      import, WordPad cursor editing, native Terminal's basic HOME operations,
      and File Manager copy/rename/delete (host builds and focused tests).
- [ ] Reach complete feature parity and remove redundant Ring 0 application
      controllers: native Paint remains smaller and lacks mouse drag strokes,
      WordPad lacks rich formatting, and native file browser is still flat.
      UI functionality is not yet verified in a VM.
- [ ] Verify all newly migrated programs in QEMU/VirtualBox; archive tests
      and host unit tests cannot prove interactive guest behavior.

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
- [x] Add Ring 3 **composition-order display commands**: windowd supplies a
      validated back-to-front list of window IDs, in addition to wallpaper,
      desktop icons, menus and taskbar. The privileged renderer uses this list
      for legacy and PID-owned native surfaces; an absent/hung windowd restores
      kernel composition ordering. Compilation/host-tested; VM pending.
- [x] Add bounded RGB565 userspace-rasterized wallpaper and shell overlay
      layers: `windowd` generates pixels and submits complete scanlines with
      capability-gated syscalls. The kernel atomically publishes layer banks,
      performs final blits and retains legacy fallback. Host tests passed;
      new `make smoke-raster` QEMU integration has not been run locally.
- [ ] Verify pixel-rasterized shell input responsiveness, Start/taskbar
      drawing, repeated resize/move/focus, and process crash recovery in QEMU
      and VirtualBox; the new boot marker confirms upload, not visible output.
- [ ] Remove the final window-rasterization dependency on Ring 0. The compositor
      still paints legacy app contents in kernel mode, and physically blits
      native per-process surfaces. Ring 3 currently owns *policy*, not pixel
      mapping. Require successful VM interaction and feature-parity testing
      before deleting the old controllers.
- [ ] Move the entire graphical compositor and legacy application controllers to
      Ring 3. **Partial:** a persistent `bin/windowd` owns shell policy,
      wallpaper and desktop-icon layout, Start/taskbar controls, title-bar
      drag requests, global hotkeys and focus decisions. It submits distinct
      background and foreground display lists to a bounds-checking kernel
      rasterizer. The older application windows and their drawing/control
      code still run in Ring 0; Ring 3 does not yet own final composition of
      all per-app surfaces. A watchdog restores the older desktop if the
      server fails. These changes pass host tests but await VM interaction tests.
- [x] Prefer native Calculator, Notepad, Calendar, Mines and Snake from
      Start with legacy-window fallback. Clock, System Monitor and About
      have native ELF pilot binaries but retain richer legacy Start entries.
      The initial three-slot window limit remains a practical constraint.
- [x] Reject off-screen Ring 3 window movement, route captured pointer
      down/move/up events to windowd and avoid repainting identical shell
      frames on heartbeat updates (host regression tests passed).
- [x] Add a capability-gated Ring 3 shell command/event ABI with bounded
      drawing lists, keyboard/mouse focus dispatch, service watchdog and
      legacy fallback. Compilation and host tests pass; VM behavior unverified.
- [ ] Retire legacy Ring 0 built-ins without regressions. **Partial:** Start
      now prefers their Ring 3 versions and the old windows are fallbacks;
      their legacy paint/input/render implementations still compile into
      the kernel and must remain until guest tests confirm replacements.
- [ ] Add a stable SDK, dynamic libraries/resources and installable apps.
- [x] Package native applications with a distinct read+execute image and
      zero-initialized read+write segment; host-tested, VM execution unverified.
- [ ] Support full native ELF segments, initialized mutable data, relocations
      and separate on-disk executable loading rather than embedded files.

## Filesystems, storage and terminal

- [x] Define an XP-style virtual drive tree including Windows, System32,
      Program Files, System Apps, Documents and Settings, and user folders;
      validate direct-child directory enumeration in host tests.
- [x] Present allowlisted OpenXP ELF64 apps as read-only `.exe` aliases in the
      filesystem, without duplicating executable bytes or accepting Windows PE.
- [x] Bundle original 24-bit BMP wallpapers from `assets/wallpapers/` into
      `Windows/Web/Wallpaper` at build time.
- [ ] Decode and select BMP wallpaper images in the Ring 3 compositor and
      expose that choice through Appearance Settings.
- [ ] Replace placeholder directories with real writable, persistent folders;
      implement user profiles, a file installer, extensions and associations.


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
- [x] Add native `APP_FS_COPY` for copying immutable boot files to HOME,
      plus native Terminal COPY/REN/DEL/WRITE/TOUCH and File Manager copy,
      rename and delete. All write destinations remain RAM-only (host tests).
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
- [x] Add a bounded RAM backbuffer for coherent desktop presentation,
      revision checks against concurrent Ring 3 commits, and clipped window,
      taskbar and active-window repainting. Host and sanitizer tests pass;
      flicker reduction still requires VM confirmation.
- [ ] Complete repaint clipping, resize behavior, scaling and tearing control.
      **Partial:** buffered composition is implemented for framebuffer modes
      up to 1024×768, but hardware vsync, larger modes and GPU scanout remain open.
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
2. Validate the new native Calendar, Mines, Snake, Monitor and About in
   QEMU and VirtualBox, including key direction and right-click routing,
   window close/reopen and resource reclamation.
3. Validate the new atomic GUI_WAIT in QEMU (`make smoke-native-wait`),
   then add process handles, event queue permissions, and user-mode desktop
   and window-server processes. The compositor still lives in Ring 0.
4. VM-test native Paint, WordPad, Terminal, File Manager, Run and Settings
   now selected by Start, including failures and Ring 0 fallback. Finish
   feature parity, then remove legacy rendering/controllers one by one.
5. Verify the opt-in secondary IDE disk against a disposable VM image, then
   extend the VFS path abstraction with file handles, directory structures,
   disk I/O errors, and crash/power-loss tests.
6. Implement real xHCI/USB HID, NIC/network, audio, accelerated graphics,
   ACPI/APIC and SMP subsystems incrementally; detection is not a driver.

A checkmark means the specified scope exists and has the indicated evidence;
it is **not** a claim that the entire operating system is finished.

## Window-service compositor bridge

- [x] Connect service `CREATE`/`MOVE`/`FOCUS`/`CLOSE` to live native-window
  geometry, focus and close events through a privileged, peer-bound syscall.
- [x] Require an accepted IPC channel owned by the authorized window server;
  prevent clients from controlling unrelated windows through forged PIDs.
- [x] Transfer taskbar/Start-menu layout and focus/shortcut policy into a
      privileged-service-authenticated Ring 3 process (host-tested source).
- [x] Move desktop background/icon drawing and title-bar drag policy into
      Ring 3; preserve kernel-only bounds checks and automatic fallback.
- [ ] Move **all** window composition, legacy application painting and the
      final desktop renderer out of Ring 0. Kernel framebuffer blitting is
      still required; no direct hardware framebuffer mapping is given to userspace.
- [ ] Run `make smoke-window-ipc` in QEMU and verify geometry changes visually
  in VirtualBox. Host test success alone does not prove guest behavior.

## Persistent Ring 3 window-service IPC

- [x] Keep `windowd` alive across multiple client sessions; accept up to six
  channels, constrained by the current fixed twelve-endpoint IPC table.
- [x] Add a server-only `IPC_WAIT` selector for connections, data on any
  accepted channel, and peer disconnects (host tests and sanitizer pass).
- [x] Implement fair bounded request dispatch and automatic logical-window
  cleanup when clients disconnect; preserve stale-window-ID protection.
- [x] Add a diagnostic build that launches two concurrent WM clients and
  checks that `windowd` remains alive after both complete. **VM test pending.**
- [ ] Verify persistent multi-client execution in QEMU/VirtualBox and check
  mouse interaction, focus switching and compositor geometry visually.
- [x] Route shell mouse clicks, global keyboard shortcuts and mouse focus
      selection to `windowd`, with a three-second heartbeat fallback.
- [ ] Move all window composition, legacy desktop applications and draggable
      window policy to Ring 3. The kernel compositor remains active.
- [x] Automatically launch `windowd` during normal boot; keep diagnostic mode
      independent and retain the kernel desktop after service failure.
- [ ] Add automatic restart supervision, dedicated service-only process slots,
      reliable input delivery under burst load, and shell-level crash telemetry.

## Native file and launch API pilot

- [x] Expand OXRD boot archive record capacity to 32 with source/host tests.
- [x] Add named-file list/read/write/delete/rename/kind system call. All
      user pointers are checked, structs are copied, and operations are bounded
      to 1 KiB per call; VFS prevents writes to `bin/` and `docs/`.
- [x] Add allowlisted Ring 3 app-launch request (deferred to boot thread),
      with queue, permission and overload tests.
- [ ] Add per-app file permissions, scalable file handles, robust errors,
      true directories, asynchronous process handles, and persistent storage.
- [ ] Verify the new syscall execution and native app interaction in QEMU
      and VirtualBox before claiming working interactive migration.

## Window composition ordering — latest pass

- [x] Submit back-to-front window IDs from Ring 3 `windowd` as a bounded
      composition display list (including the zero-window case).
- [x] Reject duplicate, out-of-range and improperly ordered drawing commands
      without replacing the last accepted scene; compare unchanged frames to
      avoid redundant full-screen repainting.
- [x] Make title-bar pointer selection agree with the submitted window order.
- [x] Preserve the legacy compositor on service exit, heartbeat timeout or
      when no valid Ring 3 plan exists.
- [x] Pixel-level host integration verifies changing composition order changes
      real framebuffer output and the fallback continues rendering.
- [ ] Pass `make smoke-compositor` in QEMU and inspect the desktop, overlapping
      windows and mouse focus in VirtualBox. Host tests cannot prove this.
- [ ] Move window pixel composition itself out of Ring 0 and finish native
      feature parity for all application controllers; only then retire the
      kernel-hosted implementations, individually after VM regression tests.

See [the compositor boundary](COMPOSITOR.md) for the protocol and release gate.

### Native window rasterization checkpoint

- [x] User-space native-window chrome and RGB565 content composition (host-tested).
- [x] Service-only read-only native surface snapshots, bounded scanline transport.
- [x] Atomic publication of a separately composed native-window raster layer.
- [x] Kernel-native fallback when the Ring 3 service fails.
- [ ] Verify native composition on QEMU and VirtualBox, including overlapped windows.
- [ ] Integrate legacy and native windows into one globally ordered userspace scene.
- [ ] Move legacy program rendering and controllers to Ring 3; remove Ring 0 copies only after parity and recovery checks.

## Explorer, file types and desktop pictures

- [x] Create browseable XP-inspired C: system folders and verified ELF64 `.exe` aliases (host tests).
- [x] Add Ring 3 Explorer navigation, address/path display, a shortcuts sidebar, color-coded file icons, and a right-click action menu (source build; VM test pending).
- [x] Add extension-based opening for text/configuration files and `.oxp` drawings, with kernel-enforced allowlisted executable launches (host-tested classification; VM test pending).
- [x] Add validated, bounded OXRD range reads for BMP images, a Ring 3 decoder with RGB565 scaling, and icon-safe wallpaper composition (host tests and sanitizer tests).
- [x] Connect bundled wallpaper selection in native Display Properties and Explorer to the Ring 3 `windowd` compositor; persist selection in RAM across service restarts (source implementation; VM test pending).
- [ ] Verify image selection, different image sizes, all associated Open actions, Explorer context operations, and compositor stability in VirtualBox and QEMU.
- [ ] Add file thumbnails, user-managed icon themes, editable associations, persistent preferences, a real disk-backed filesystem and a richer resizable Explorer layout.

## Window maximize and restore

- [x] Add a maximize/restore button between Minimize and Close on both
      native Ring 3 and legacy window title bars. Save normal window geometry;
      reserve the taskbar; restore normal bounds without losing position.
- [x] Support Win+Up and Win+Down in the userspace shell and fallback desktop.
      Prevent maximized windows from entering title-bar drag mode.
- [x] Cover maximize/restore and title-bar chrome in focused host tests.
- [ ] Confirm native and legacy maximize, restore, minimize, taskbar behavior,
      and pixel refresh on the latest ISO in VirtualBox.
- [x] Add owner-scoped logical canvas-size query and resize notifications;
      reflow built-in native controls/editors and match hit tests to rendering
      (host-tested). Full VM visual verification and higher-resolution native
      surfaces remain for a later pass.
