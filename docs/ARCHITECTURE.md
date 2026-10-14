# Architecture

OpenXP is a small, **single-core x86-64 kernel** targeting PC-style virtual
hardware. It uses Limine for bootstrapping and owns its memory manager,
interrupt handlers, scheduler and software desktop.

> [!IMPORTANT]
> Design notes describe the source tree, not a fully QEMU-verified operating
> system. In particular, interrupt-driven userspace scheduling and privilege
> transitions still need live integration testing.

## System overview

```text
Limine (BIOS / UEFI)
  └─ OpenXP kernel (Ring 0)
      ├─ Memory: physical pages, virtual mappings, heap
      ├─ CPU: GDT, TSS, IDT, exception and IRQ handlers
      ├─ Tasks: scheduler, PIT clock, wait queues, mutexes, IPC
      ├─ Files: read-only OXRD archive + writable temporary RAM files
      ├─ Desktop: framebuffer, PS/2 input, shell, windows
      └─ Experimental Ring 3 runtime
          └─ Private address spaces, ELF64 loader, int 0x80 syscalls
```

The kernel still owns the **final framebuffer drawing** and legacy program
windows. A trusted `bin/windowd` Ring 3 process now supplies the taskbar and
Start-menu drawing commands, global shell shortcuts and focus policy. The
kernel validates each command and restores its original shell when the service
stops. Other native programs have bounded drawing surfaces and an event ABI;
most historical built-ins remain kernel-hosted. The native
application pilot is described in [Native applications](NATIVE-APPS.md).

## Boot sequence

The normal boot path is designed to:

1. Receive a framebuffer, memory map and higher-half direct map from Limine.
2. Initialize COM1, the GDT/TSS, interrupt descriptor table and crash reporting.
3. Set up physical-page allocation, kernel-owned page tables, NX protection,
   guarded memory regions and a fixed 128 KiB heap.
4. Initialize the scheduler and the legacy PIC/PIT timer (nominally 100 Hz).
5. Validate and mount the embedded read-only RAM filesystem.
6. Initialize PS/2 input and the software desktop, then enter its event loop.

The event loop polls keyboard, mouse and serial input, redraws the framebuffer
when needed, and uses `hlt` between wakeups. Its serial boot marker is
`OPENXP_BOOT_OK`; smoke tests inspect markers, **not** visible guest input.

`make smoke-deep` builds a diagnostic boot path that additionally exercises
kernel threads, timers, IPC, process loading, faults and user-mode execution.
These tests are separate from the ordinary interactive desktop path.

## Memory and privilege levels

| Layer | Current design |
| --- | --- |
| Physical memory | Bitmap allocator for usable 4 KiB frames |
| Virtual memory | Four-level x86-64 page tables, private user PML4 roots |
| Kernel heap | Fixed 128 KiB heap with allocate, free and reuse |
| Stack safety | Unmapped guard pages on selected kernel and user stacks |
| Page permissions | NX and user/supervisor bits; loader rejects writable-executable segments |
| Processes | Small fixed-capacity process table with private user mappings |

Kernel-half mappings are inherited by process address spaces as supervisor
mappings. Their security has **not** been fully audited. The kernel's initial
mappings also retain bootloader-created mappings. Memory-management changes
and process teardown require carefully controlled interrupts on one CPU;
there is no SMP or TLB shootdown support.

## Interrupts and scheduling

- **Exceptions:** IDT entries and COM1 diagnostic dumps, including page-fault
  information. Dedicated protected stacks exist for critical exceptions.
- **Timer:** legacy PIC and PIT IRQ0 with periodic tick accounting.
- **Kernel threads:** cooperative context switching and an experimental
  timer-preemptive path. These use different register-frame contracts.
- **Sleep and wait:** tick-based sleeping, blocking, wakeups and FIFO wait queues.
- **Synchronization:** manual-reset events, nonrecursive mutexes and bounded
  kernel IPC queues (8 messages of up to 64 bytes each).
- **User scheduling:** experimental process-aware CR3/TSS switching; not yet
  established as correct through live QEMU execution.

Interrupt masking here is a **single-CPU** critical-section mechanism, not a
replacement for SMP locks. Floating-point/SIMD task-state saving is absent.
All bundled Ring 3 C objects are therefore compiled for general-purpose
registers only, with a disassembly-based build guard before initrd packaging.

## Files and applications

The build embeds a validated **OXRD** read-only RAM archive in the kernel image.
`kernel/fs/initrd.[ch]` parses it; `tools/mkinitrd.py` creates it.
It contains example ELF64 applications and a text document. A bounded RAM filesystem (`kernel/fs/ramfs.[ch]`) holds up to twelve temporary
files with a maximum of 16 KiB each. The graphical Notepad, WordPad and Paint
programs save fixed HOME filenames there; the My Computer window merges their
names with the OXRD boot archive, and Terminal can read temporary text files.
A narrowly scoped user API can read/write only `home/native-note.txt`;
there are no general-purpose user filesystem handles yet.
There is **no persistent disk storage**: all HOME files disappear on reboot.

See [Userspace](USERSPACE.md) for executable formats, user memory boundaries,
system calls and process lifecycle. See [Desktop](DESKTOP.md) for the input and
window manager architecture.

## Testing and diagnostics

```bash
make test-unit       # Host-side C and Python tests
make test-sanitize   # Host-side ASan/UBSan checks
make kernel          # Freestanding ELF64 compilation
make smoke           # Normal QEMU boot/serial markers
make smoke-deep      # Deep diagnostic QEMU boot
```

Other dedicated `smoke-*` targets exercise faults, timer IRQs, synchronization,
application loading and userspace protection. A passing host test does not
establish that an interrupt, privilege switch or GUI actually works in QEMU.

All work that is **planned rather than implemented** is tracked in the
[Roadmap](ROADMAP.md).

## New native rendering and optional hardware groundwork

Native ELF64 programs can submit bounded pixel drawing batches through syscall
11. Each process has its own kernel-managed 320×180 RGB565 backing surface.
The current Ring 0 desktop composes it into a clipped framebuffer window.
The surface is not a shared writable user mapping and cannot be modified by a
different PID. This is an intermediate architecture, **not a Ring 3 window
server**. See [Native apps](NATIVE-APPS.md).

OpenXP can enumerate legacy PCI configuration devices and classify potential
storage, graphics, audio, network and USB controllers, but the associated
hardware drivers remain unimplemented. An optional secondary IDE ATA PIO
backend and host-tested snapshot journal exist for a **disposable test disk**
only. The feature is disabled by default; see [Storage](STORAGE.md).

## Native-event wait and virtual files

`GUI_WAIT` atomically checks a PID-owned GUI queue and parks the calling
single-CPU scheduler thread if no event is present. GUI delivery wakes it;
positive timer deadlines permit periodic redraws without a busy loop.
The process remains in Ring 3 between syscalls, but the GUI broker and
window composition are still privileged kernel services. VM validation of
this new syscall path is pending.

The small `kernel/fs/vfs.c` layer unifies enumeration and path-based reads
from OXRD with writable operations restricted to validated RAMFS `home/`
paths. It is **not** an on-disk filesystem, nor a complete VFS with file
handles or per-user permissions. The experimental IDE snapshot backend is
kept opt-in, and default ISO images never write to an attached disk.
