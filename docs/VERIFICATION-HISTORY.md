# Aurora Verification History

Original release verification reports. They are historical records, not proof of a current VM boot.

## AURORA-11-VERIFICATION

```text
OpenXP Aurora 11 — verification report
Source baseline: verified local Aurora 10 full source (not an unverified repository pull).

Implemented:
- Native GUI pointer-opt-in syscall 35; no caller-supplied process/slot ID.
- Bounded MOVE/DOWN/UP/LEAVE event ABI and per-window capture with PID isolation.
- Pointer motion coalescing and control-event priority in native queues.
- Shared Aurora widget registry for themed button, toolbar, tab, checkbox,
  scrollbar, and File Manager context-menu hover/pressed states.
- Release-based activation, cancel-on-outside/other control, maintained generic
  document-area click coordinates, focus navigation Tab/Shift+Tab and Enter.
- Legacy unregistered native applications still receive old CLICK semantics.
- New pure gesture FSM and kernel event tests integrated into unit/sanitizers.
- All existing 35 original PNG masters across Blue/Olive/Midnight included.

Verification in container:
PASS: make -j4 kernel (ELF64 kernel linked; native app bins and initrd built)
PASS: make test-unit (complete host-side suite)
PASS: make test-native-pointer-sanitize (ASan/UBSan, pure + kernel FIFO)
PASS: make test-native-widgets-sanitize (ASan/UBSan)
PASS: make test-desktop-sanitize (ASan/UBSan, pointer routing, captured UP)
PASS: Ring 3 PIC audit of windowd and 30-object ISA audit during kernel build
PASS: all three ZIP archives checked for integrity after creation

Unverified:
- Full test-sanitize was interrupted at the VMM sanitizer test by the
  execution time limit; preceding stages, including pointer and desktop,
  passed. This is NOT a claimed full sanitizer pass.
- No successful smoke-shell QEMU boot/interactive VirtualBox rendering in this
  session. Real mouse movement and artwork states still require VM inspection.
- Painting brush dragging, text selection dragging and continuously draggable
  scrollbar thumbs are not implemented by this release.

Usage:
1. Extract updated-files ZIP into Aurora 10 repository root, preserving paths,
   or use the full-source ZIP as the standalone base.
2. make -j4 kernel && make test-unit
3. make iso && make smoke-shell (with configured bootloader and QEMU)
4. Test Run, Settings, Calculator, File Manager menus, Notepad, WordPad, and
   theme changes: hover, hold, cancel outside, keyboard focus and resize.
```

## AURORA-12-VERIFICATION

```text
OpenXP Aurora 12 — verification report
Source baseline: mounted OpenXP Aurora 11 full source (not unverified remote code).

Implemented:
- Opt-in oxp_ui_wait_gesture() preserving captured native pointer DOWN/MOVE/UP.
- Motion interpolation for continuous mouse-held Paint brush strokes.
- WordPad drag-selection across wrapped rows, highlighted selection, replacement
  on typing/backspace, movable caret, draggable scrollbar thumb and track clicks.
- Notepad movable caret, drag-selection, selected-range editing, wrap-aware
  rendering, and vertical thumb dragging.
- File Manager thumb dragging that preserves item selection, with release
  activation and cancellation of long pointer drags.
- Pure bounded gesture/scroll helpers and input regression tests.
- Original Aurora Blue, Olive, Midnight PNG masters preserved (35 PNGs).

Verified in container:
PASS: make -j4 kernel (ELF kernel, native app bins and initrd)
PASS: make test-unit (complete host-side suite, including new regression tests)
PASS: make test-advanced-input-sanitize (ASan and UBSan)
PASS: make test-native-pointer-sanitize
PASS: make test-native-widgets-sanitize
PASS: make test-native-text-layout-sanitize
PASS: make test-native-scroll-sanitize
PASS: make test-desktop-sanitize
PASS: Ring 3 PIC and 30-object no-SSE/no-x87 ISA audit during build
PASS: 3 ZIP integrity checks and 35 editable PNG masters present

Not verified:
- Interactive QEMU/VirtualBox pointer and redraw behavior; qemu-system-x86_64
  and xorriso are not installed in this build container.
- Complete sanitizer suite was not run; focused relevant sanitizers pass.
- Notepad, WordPad and Paint save/load round trips inside a running VM.

Deployment:
1. Extract updated-files ZIP over Aurora 11 repository root, preserving paths.
2. make -j4 kernel && make test-unit
3. make iso && make smoke-shell (on an environment with Limine/QEMU/Xorriso).
4. Manually test mouse strokes, selection, thumb dragging, window resizing,
   keyboard Open/Save, three themes and pointer release outside the window.
See AURORA-RELEASES.md#aurora-12 for the implementation and known limitations.
```

## AURORA-13-VERIFICATION

```text
OpenXP Aurora 13 — build and verification
Baseline: Aurora 12 full source mounted in the current task.

Implementation
- Volatile 960-byte system-wide clipboard (syscalls 36 and 37).
- Kernel copy-in/copy-out checks: invalid user pointers, insufficient destination
  capacity, and values outside the clipboard limit return an error.
- Native keyboard event translation: Ctrl+A/C/X/V/Z/Y, Ctrl+Shift+Z,
  Shift+Arrows, Home/End, Shift+Home/End, Ctrl+Home/End, Delete.
- Notepad/WordPad clipboard, bounded undo/redo and edit toolbars when wide.
- Paint stroke-based undo/redo, with branch truncation after undo.
- New editing and clipboard security regression tests.
- Increased VFS executable test buffer from 16KiB to 32KiB because new native
  Notepad binary exceeded the old test-only stack buffer.
- Aurora Blue/Olive/Midnight original PNG artwork retained unchanged.

Passed locally
- make -j6 kernel (all native apps, initrd, kernel)
- make -j4 test-unit (complete suite)
- make -j4 test-sanitize (complete ASan/UBSan suite)
- make test-aurora13-sanitize (final rerun)
- Ring 3 PIC audit, no SIMD/x87 instruction audit during build
- Packed theme and font resources during initrd creation

Not verified
- Interactive QEMU and VirtualBox sessions; qemu-system-x86_64 and xorriso
  were unavailable in this container. No VM graphical screenshots are claimed.
- User-facing performance and mouse/keyboard behavior inside a live VM.

Limits
- ASCII plain text; shared clipboard up to 960 bytes, volatile on reboot.
- Editor buffer 959 text bytes + NUL; up to 11 undoable history states.
- Paint up to seven undoable historical strokes; no image clipboard format.
- Ring 0 fallback editors not migrated.
- Clipboard stays in kernel pending a future Ring 3 clipboard server.

Deployment
1. Apply updated-files ZIP over Aurora 12, preserving directory structure.
2. Run make -j4 kernel && make -j4 test-unit && make -j4 test-sanitize.
3. On GitHub Actions runner run make iso && make smoke-shell.
4. In a VM verify cross-app copy/paste, selection, Undo/Redo and theme switching.

Documentation: AURORA-RELEASES.md#aurora-13
```

## AURORA-15-VERIFICATION

```text
OpenXP Aurora 15 — Verification

Source baseline: OpenXP Aurora 14 full source ZIP (locally available).

IMPLEMENTED
- Atomic allowlisted executable + document argument launch packet.
- Immutable per-PID startup document argument, with bounded copy-out syscall.
- File Manager -> Notepad/WordPad/Paint association handoff through new ABI.
- Existing shared picker enhanced for filename focus, Open typing, double click.
- File Manager drag of read-only file to Documents; collision-safe copy, no clipboard overwrite.
- Explicit opt-in disk flush request and status, boot-thread completion.
- Periodic disk commit timing based on elapsed ticks, with retry interval.
- Original Aurora Blue/Olive/Midnight PNG artwork retained.

PASSED
- make kernel (default build), PIC and Ring 3 ISA audits.
- make kernel-storage (storage-enabled build), PIC and Ring 3 ISA audits.
- Aurora 14/15 host workflow tests, normal and ASan/UBSan.
- Kernel app-launch tests, normal and ASan/UBSan.
- Kernel syscall tests, normal and ASan/UBSan.
- Process manager isolated startup argument test, normal and ASan/UBSan.
- Existing dual-slot persistence recovery tests, normal and ASan/UBSan.

NOT VERIFIED
- Complete unit + sanitizer suite in this pass: full host tests began and
  succeeded through the C suite; full Python discovery was interrupted by the
  execution limit. Do not claim a completed full suite.
- ISO/QEMU/VirtualBox interactive runtime (emulator and ISO dependencies absent).
- Actual disk writes, power-loss behavior and drag/drop interaction in a VM.

KNOWN LIMITS
- Single optional document path, not arbitrary POSIX argv.
- Drop destinations are limited to the Documents sidebar target inside Explorer.
- HOME remains RAM-only in default build. Experimental IDE storage is opt-in.
- VFS remains bounded and userland legacy mailboxes remain as fallback.
```

## AURORA-16-VERIFICATION

```text
OpenXP Aurora 16 — Verification report
16 October 2026

Base: OpenXP-Aurora-15-full-source.zip provided in this conversation.

Implemented changes:
- Writable nested HOME / Documents folders via .oxpdir RAMFS metadata entries.
- APP_FS_MKDIR (9), APP_FS_RMDIR (10), APP_FS_STAT (11) new FS opcodes.
- VFS listing/alias handling, file-size lookup, parent validation, safe empty-dir removal.
- File Manager new-folder UI, Properties panel, current-folder copy/paste and
  intra-Explorer drag-to-folder copying.
- Document drag message from File Manager to trusted windowd via a 64-byte IPC
  payload, filtered by file type and editor title, launching a new editor process.
- Shell observes normal client mouse-button releases without consuming them.
- Desktop shortcut visibility can be toggled via Settings and restored from
  HOME/desktop.cfg if RAMFS survives/reloads.
- Terminal MKDIR/RMDIR/STAT; corrected trailing slash directory listing.
- Tests for VFS nested dirs, bounds and document routing; updated shell bridge test.

Executed checks:
PASS: make kernel (Clang/LLD freestanding compile, link and initrd packing)
PASS: Ring 3 PIC audit of 9 window-manager object files
PASS: Ring 3 ISA audit of 30 modules
PASS: make test-unit (full host-side suite, after all major changes)
PASS: targeted VFS regression test with ASan/UBSan
PASS: Aurora 16 workflow tests with ASan/UBSan
PASS: shell-bridge pointer routing regression test with ASan/UBSan
PASS: final kernel build and Aurora 16 targeted tests after final helper change
INCOMPLETE: make test-sanitize: it passed the first several sanitized tests,
    then exceeded the available execution time during native pointer tests.
NOT TESTED: QEMU/VirtualBox actual guest boot and cross-window drag interaction;
    QEMU/xorriso executables are absent from this session.

Limits and caution:
- The entire writable RAMFS holds at most 12 entries, including directory
  markers and preferences. Each file is bounded by existing RAMFS limits.
- Directory rename, recursive delete, arbitrary persistent disks, and general
  cross-application drag/drop are not implemented.
- Document drop works only for validated short paths and launches a new editor;
  it does NOT modify an existing editor process or document.
- The Ring 0 legacy desktop is not theme/visibility-integrated with this feature.
- Standard ISO uses volatile RAMFS. Persistence still requires a separately
  configured storage-enabled build with a disposable dedicated formatted IDE disk.
- Real guest functionality is not release-verified. Run GitHub Actions and VM
  interaction tests before treating it as stable.
```

## AURORA-17-VERIFICATION

```text
OpenXP Aurora 17 — Verification Report
======================================
Source baseline: OpenXP Aurora 16 full-source ZIP.

IMPLEMENTED
- RAMFS slot capacity: 12 -> 48 (16,384 bytes maximum per record;
  total maximum contents 786,432 bytes / 768 KiB, less folder markers).
- Transactional directory rename/move with preflight checks for invalid
  destinations, path length, collisions, self-nesting; renames metadata for
  every descendant atomically in the current single-CPU RAMFS.
- File Manager folder Rename, folder Cut/Paste move, clear status feedback.
- New opt-in OXPDISK2 version 2 data-image format from tools/mkstorage.py.
  Two 1,700-sector slots fit all 48 maximum-size files in a 2 MiB disk.
- Existing OXPDISK1 version 1 images still mount with the original
  400-sector journal geometry; they are never reformatted or migrated.
  If data exceeds a v1 slot, commits fail before disk modification.
- Desktop icon visibility, selected appearance, wallpaper and documents
  are included in snapshots with the storage-enabled kernel/dedicated disk.
- Settings distinguishes RAM-only saves from a pending disk checkpoint.

TEST RESULTS
PASS: make -j4 kernel (normal freestanding x86-64 build).
PASS: make -j4 kernel-storage (opt-in IDE-enabled build).
PASS: make test-unit (complete suite; final rerun).
PASS: make test-aurora17 (RAMFS, VFS, V1/V2 persistence, mkstorage tool).
PASS: make test-aurora17-sanitize (ASan/UBSan targeted).
PASS: make test-sanitize (complete earlier run on Aurora 17 source).
NOTE: last full sanitizer rerun after final UI changes was interrupted by
      the execution limit; targeted sanitizer tests and syscall sanitizer
      were rerun and passed after final changes.
PASS: ring3 ISA and windowd PIC build audits.
PASS: full-size 48 x 16KiB journal save/restore with simulated power cut.
PASS: appearance preference restoration and legacy journal compatibility.
NOT VERIFIED: interactive QEMU / VirtualBox boot, mouse interactions, and
              persistent storage across actual VM reboot.

SAFETY / LIMITS
- Default ISO is RAM-only; experimental persistence requires a correctly
  formatted dedicated secondary IDE disk. Never attach a valuable disk.
- Existing OXPDISK1 disks cannot hold the maximum enlarged RAMFS; the old
  layout is intentionally preserved, with no automatic format migration.
- Path limit 47 ASCII bytes; no recursive copy/remove, disk filesystem,
  FAT/NTFS, long names or guaranteed crash safety on real hardware.
- Synchronous full snapshots may cause UI pauses with very large data sets.
- Bundled artwork is UNCHANGED from Aurora 16 (35 editable PNG assets).

FILES
- Updated-files ZIP includes all changed/new source and documentation
  plus every original editable Aurora PNG.
- Full-source ZIP includes every original source asset, test and document.
- Artwork ZIP includes the original editable image assets; historical
  preview contact sheets were later removed to reduce file count.
```
