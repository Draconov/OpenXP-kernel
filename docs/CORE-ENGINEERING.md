# Core Engineering

Consolidated from the Aurora 17 source tree. All original technical instructions are retained below, grouped by topic.

## Sections

- [ARCHITECTURE](#architecture)
- [USERSPACE](#userspace)
- [IPC](#ipc)
- [RING3 SHELL](#ring3-shell)
- [COMPOSITOR](#compositor)

---

## ARCHITECTURE

## Architecture

OpenXP is a small, **single-core x86-64 kernel** targeting PC-style virtual
hardware. It uses Limine for bootstrapping and owns its memory manager,
interrupt handlers, scheduler and software desktop.

> [!IMPORTANT]
> Design notes describe the source tree, not a fully QEMU-verified operating
> system. In particular, interrupt-driven userspace scheduling and privilege
> transitions still need live integration testing.

### System overview

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
application pilot is described in [Native applications](DESKTOP-AND-APPS.md#native-apps).

### Boot sequence

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

### Memory and privilege levels

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

### Interrupts and scheduling

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

### Files and applications

The build embeds a validated **OXRD** read-only RAM archive in the kernel image.
`kernel/fs/initrd.[ch]` parses it; `tools/mkinitrd.py` creates it.
It contains example ELF64 applications and a text document. A bounded RAM filesystem (`kernel/fs/ramfs.[ch]`) holds up to twelve temporary
files with a maximum of 16 KiB each. The graphical Notepad, WordPad and Paint
programs save fixed HOME filenames there; the My Computer window merges their
names with the OXRD boot archive, and Terminal can read temporary text files.
A narrowly scoped user API can read/write only `home/native-note.txt`;
there are no general-purpose user filesystem handles yet.
There is **no persistent disk storage**: all HOME files disappear on reboot.

See [Userspace](CORE-ENGINEERING.md#userspace) for executable formats, user memory boundaries,
system calls and process lifecycle. See [Desktop](DESKTOP-AND-APPS.md#desktop) for the input and
window manager architecture.

### Testing and diagnostics

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

### New native rendering and optional hardware groundwork

Native ELF64 programs can submit bounded pixel drawing batches through syscall
11. Each native process has its own kernel-managed RGB565 surface, with a
maximum allocation of 640×360 and resize-dependent logical dimensions.
The current Ring 0 desktop composes it into a clipped framebuffer window.
The surface is not a shared writable user mapping and cannot be modified by a
different PID. This is an intermediate architecture, **not a Ring 3 window
server**. See [Native apps](DESKTOP-AND-APPS.md#native-apps).

OpenXP can enumerate legacy PCI configuration devices and classify potential
storage, graphics, audio, network and USB controllers, but the associated
hardware drivers remain unimplemented. An optional secondary IDE ATA PIO
backend and host-tested snapshot journal exist for a **disposable test disk**
only. The feature is disabled by default; see [Storage](FILES-AND-STORAGE.md#storage).

### Native-event wait and virtual files

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

---

## USERSPACE

## Userspace and application loading

OpenXP contains an **experimental Ring 3 runtime** for small, statically linked
x86-64 ELF programs. It demonstrates a privilege boundary and separate process
address spaces, but it is not a mature process system or a security boundary
that has been verified under QEMU.

The desktop, terminal and legacy built-in applications still run in **Ring 0**.
Small native applications use a separate Ring 3 GUI syscall interface.

### Process model

A process owns:

- A unique process ID and an entry in a small, fixed-capacity process table.
- A private PML4 root, user code/data pages and guarded user stack.
- A separate guarded kernel-entry stack used through TSS RSP0.
- Exit or fault status, followed by explicit resource reclamation.

The implementation provides a sequential runner with interrupts disabled and
an **experimental** timer-preemptible runner for small numbers of processes.
The desktop now allocates up to three Ring 3 launch workers, each owning its
own text window. Parallel GUI execution still needs live VM verification.
The latter switches CR3 and TSS RSP0 with the saved interrupt context. It must
not be assumed safe until live CPU-level behavior is validated.

A process can be created from a supported ELF image, run, inspected for
completion and reaped. `process_wait_ticks()` polls completion using short PIT
sleeps; it is a kernel API, not a userspace `waitpid` syscall. The process
table is limited to four slots.

### Executable format

The loader (`kernel/user/elf.c`) accepts a **restricted** subset of little-endian
AMD64 ELF64:

- Static `ET_EXEC` images and relocation-free, position-independent `ET_DYN`.
- Up to 16 loadable segments, at most 2 MiB per segment, and 768 loaded pages total.
- Checked offsets, alignment, executable entry points and page bounds.
- Readable segments, zero-filled memory and rejection of writable-executable
  mappings (W^X).

Dynamic linking, ELF interpreters, relocations, TLS and standard general-purpose
PIE executables are unsupported. A failed load can leave partial mappings;
the caller must destroy that address space rather than reusing it.

**Native flat-image constraint:** `apps/native/app.ld` combines linked `.data`
with the read-only `.text` payload; `tools/mkinitrd.py` maps only the following
page-aligned `.bss` region writable. Do not use initialized mutable globals in
`windowd` (including `static int value = -1`): declare them zero-initialized
and set their runtime defaults at the start of `oxp_main`. The `windowd` link
map validator inspects **input** `.data` sections nested inside RX `.text`,
not merely the output `.data` section, and aborts the build if it finds them.
This is a limitation of the interim native packer; a future multi-segment
loader should represent initialized writable data in a real RW PT_LOAD.

The build packs `bin/hello`, `bin/fault`, native programs `bin/calculator`,
`bin/clock`, `bin/notepad`, a native ABI probe, and a text file into the embedded
**OXRD** archive. That archive has bounded paths, sizes and CRC32 checks, and
is read-only; it is not a persistent disk filesystem.

### Memory access and isolation

Each process gets private low-half user mappings while inheriting supervisor-only
kernel-half mappings. User pages must carry the user bit at each page-table
level; stacks are writable, non-executable and bordered by unmapped guards.
The normal process stack starts after a guard page at user offset `0x3f0000`
inside the private 4 MiB range. This leaves native ELF code and its larger
zero-filled RW/BSS mapping (2 MiB for `windowd`) separate from the stack.
The layout is defined in `kernel/user/process_layout.h`; the initrd image test
verifies that bundled ELF segments never overlap the reserved stack pages.

`aspace_copy_from_user` and `aspace_copy_to_user` validate the full user range,
then access data through checked physical aliases instead of dereferencing
untrusted user pointers directly. This design is **not concurrency-safe**
against a second thread modifying the same address space during a copy.

### System-call interface

Calls use **`int 0x80`** with the call number in `RAX`. Relevant arguments are
passed through general-purpose registers (shown below).

| Number | Name | Purpose |
| --- | --- | --- |
| `0` | `SYS_VERSION` | Return the kernel ABI identifier |
| `1` | `SYS_PUTC` | Write one ASCII character to serial |
| `2` | `SYS_EXIT` | Terminate the current process |
| `3` | `SYS_TICKS` | Read timer ticks |
| `4` | `SYS_WRITE` | Write up to 1,024 validated user bytes to serial (`RDI` pointer, `RSI` length) |
| `5` | `SYS_GETPID` | Return the current process ID |
| `6` | `SYS_GUI_OPEN` | Open the native text window (`RDI` user title, `RSI` byte count); single-owner |
| `7` | `SYS_GUI_TEXT` | Set native window text (`RDI` pointer, `RSI` byte count; max 383) |
| `8` | `SYS_GUI_EVENT` | Poll one queued keyboard/close event, or zero if none |
| `9` | `SYS_RAM_READ` | Read `home/native-note.txt` into user-writable memory, max 2048 bytes |
| `10` | `SYS_RAM_WRITE` | Replace `home/native-note.txt` from user memory, max 2048 bytes |

Invalid calls or invalid pointers return errors where applicable. This small
ABI is internal to the prototype; it is **not** the Windows API.

### Integration tests

```bash
make smoke-process          # Private mappings, ELF startup and cleanup
make smoke-user             # Ring 3 entry and syscall path
make smoke-user-preempt     # Two timer-preempted user processes
make smoke-user-fault       # Deliberate user fault
make smoke-user-protection  # Rejected supervisor-memory access
make smoke-initrd           # Packaged application and fault-recovery test
make smoke-native           # Native GUI and RAM-file ABI probe
```

These require Limine, Xorriso and QEMU. Until these paths successfully execute
in a VM, the security and fault-isolation claims remain **unverified**. See the
[Roadmap](ROADMAP.md) for verification and development priorities.

### Native application SDK

See [Native applications](DESKTOP-AND-APPS.md#native-apps) for sample programs, the minimal
freestanding user runtime, the build/linker script, and explicit current limits.
The native ABI is unstable and the renderer/window manager remain privileged.

---

## IPC

## Process handles and native window-service IPC

OpenXP has a **small experimental capability IPC mechanism**, separate from its
older kernel-thread `sync/ipc.c` queues. This is the first transport intended
for communication between *independent Ring 3 processes*. The service now
also owns Ring 3 taskbar/Start-menu policy and drawing requests, while final
pixel composition and legacy windows remain in Ring 0.

### Try the window-management protocol pilot

The normal graphical boot automatically starts `bin/windowd` (Ring 3).
Open Terminal and run its separate demonstration client:

```text
RUN BIN/WINCLIENT
```

The client connects via capability IPC, creates a logical window, and then
moves, focuses, queries and closes it. The server validates channel ownership,
and a restricted `WM_DISPLAY` syscall forwards accepted changes to the real
native-window broker. The legacy Ring 0 compositor renders the resulting
positions on its next desktop-loop iteration.
The service remains alive after each demonstration client exits. It runs
headlessly so client windows retain all native GUI slots. The shell service
also manages global shortcuts, Start menu and taskbar rendering policy.
The kernel restores its own desktop shell on service failure.

The optional QEMU integration target auto-launches `windowd` and two
concurrent independent clients:

```bash
make smoke-window-ipc
```

Expected serial markers: `OPENXP_WM_BRIDGE_OK`,
`OPENXP_WM_MULTICLIENT_OK`, and `OPENXP_CAPIPC_OK`.
The first requires the live GUI broker to receive create/move, focus and close
requests; this is still not pixel-level screenshot verification.
Host tests do not substitute for a live QEMU or VirtualBox run.

#### Window protocol

`apps/native/wm_protocol.h` defines the versioned 60-byte `wm_packet_t` wire
format, carried inside the existing bounded 64-byte IPC transport. Commands
are `WM_CREATE`, `WM_MOVE`, `WM_FOCUS`, `WM_QUERY`, `WM_CLOSE` and `WM_PING`.
Replies carry the reply flag, result code and current logical window geometry.
The Ring 3 service (`apps/native/wm_protocol.c`) owns a fixed table of up to
six logical windows, currently limited to one per client channel because the
native broker supports one window per PID. IDs contain a slot and generation number so deleted
window IDs cannot be reused as stale capabilities.

Window operations verify that the *accepted channel* owns the referenced
window. They validate geometry and desktop bounds, scrub control characters
from titles and clean up owned windows on disconnection. A different channel
cannot modify or query another client's window. This is a **userspace window-policy state machine** with a narrowly scoped
kernel compositor bridge; actual composition and hardware input remain in Ring 0.

Run the host regressions independently with:

```bash
make test-window-protocol
make test-window-protocol-sanitize
```

### Kernel design

- `kernel/user/capipc.c` owns every endpoint, message queue and handle table.
  User programs never receive mapped kernel buffers or pointers.
- The trusted kernel launcher grants a **single server role** only to
  `bin/windowd` from the *read-only boot archive*. Other Ring 3 callers cannot
  register the service. This is a boot-image trust assumption, **not** package
  signing or an application security policy.
- A client connects to the singleton service, receiving a **process-owned
  opaque handle**. The server accepts and receives a different handle.
- Handles contain a slot number and a generation counter. All operations check
  the caller's PID; close and slot reuse invalidate stale handles.
- Each direction is a separate queue with at most four messages of **1–64
  bytes**. The server and each client can send and receive. Clients are limited
  to two concurrent channels; the global fixed table supports twelve endpoints.
- Requests return immediately if a queue is full or empty. `IPC_WAIT` uses
  the existing scheduler's atomic park/wake path and optional timer timeout.
  Service-only `IPC_WAIT(UINT64_MAX, timeout)` atomically waits for **any**
  pending accept, accepted-channel message or client disconnect. The server
  therefore needs no continuous polling and services one request per client
  per pass. A bounded wait also checks for the service-window close event.
- Process exit or fault closes that process's handles, wakes peers and releases
  service registration. A peer can drain already queued messages after closure.
- This is a single-CPU design guarded by local interrupt masking. It is **not**
  an SMP-safe implementation or a general-purpose secure microkernel.

### Ring 3 system calls

`apps/native/oxp.h` provides the demonstration wrappers:

| Call | Purpose |
| --- | --- |
| `IPC_LISTEN` (13) | Claim the authorized singleton service |
| `IPC_ACCEPT` (14) | Accept the oldest pending connection |
| `IPC_CONNECT` (15) | Connect to the listening service |
| `IPC_SEND` (16) | Copy 1–64 bytes into the peer's queue |
| `IPC_RECV` (17) | Copy the next message into a validated user buffer |
| `IPC_CLOSE` (18) | Release the caller's channel handle |
| `IPC_WAIT` (19) | Block until a message/connection/close or timeout; the listening service can pass `UINT64_MAX` to wait for any client |

`SEND` and `RECV` take `(handle, user_buffer, length)` in `RDI`, `RSI`, `RDX`.
Other calls use the existing two-argument syscall convention. Successful
connect/accept return an opaque positive handle; send/receive return a byte
count. Other successful calls return zero. Negative values indicate errors
(e.g. `-11` unavailable, `-32` disconnected, `-1` permission denied).

### Security boundaries and limitations

IPC rejects unowned handles, validates lengths and mapped user pages before
copying, and clears resources on exit. There is **no privilege escalation from
an IPC connection**: owning a channel does not grant framebuffer access or a
window-manager capability. The demo's displayed windows use the *existing* PID-checked GUI
syscalls, which still draw through the kernel compositor.

The service now starts automatically and has a narrow, process-authenticated
input/focus and shell-rendering interface. Full userspace window composition,
process launch permissions, service restart supervision, security auditing,
real QEMU/VirtualBox testing and SMP-safe locking are outstanding.

### Live window compositor bridge

`windowd` can now submit a `WM_DISPLAY` syscall (number 20) with an accepted
IPC handle and a copied `openxp_wm_display_cmd_t`. The kernel resolves that
handle to the **connected client PID**; user-provided PIDs are never trusted.
A disconnected, stale, unaccepted, or unauthorized channel is rejected.

`SET_RECT` requests a bounded window position and size; `RAISE` requests focus;
`REQUEST_CLOSE` delivers the existing sticky close event to that client's GUI
queue. The boot thread reconciles native-window metadata to the kernel's
existing compositor on its next desktop loop. Window contents remain
process-owned RGB565 surfaces, never direct framebuffer mappings.

**Limitations:** The final framebuffer compositor is still Ring 0, and the
protocol assumes a 1024×768 display. The Ring 3 service now owns taskbar/Start
rendering commands and mouse/keyboard focus requests, but dragging remains
kernel-hosted and only one native window exists per client. There is no general
Ring 3 compositor or automatic service restart. The old desktop is the fallback.

### Appearance requests

`WM_THEME` (operation 7) is a bounded request sent by native Settings to the
trusted `windowd` service. The `x` field contains a preset ID (0–2). The
service validates the packet magic/version and preset, then issues the
privileged `OXP_SHELL_THEME` action through the existing service-gated syscall.
The kernel updates the fallback desktop's appearance as well. Other Ring 3
applications cannot issue `OXP_SHELL_THEME` directly.

The request does not delegate framebuffer access. At present any application
that is permitted to connect to `windowd` can request an appearance change;
there is no multiuser preference ownership model yet. `home/appearance.cfg`
is stored in RAMFS; this is not persistent across reboots by default.

---

## RING3 SHELL

## Ring 3 desktop shell and compositor policy

`bin/windowd` owns desktop wallpaper/icon composition, taskbar/Start-menu layout, global hotkeys, focus and title-bar drag policy.
It runs as a preemptible Ring 3 process and is automatically started on the
normal graphical boot. The desktop retains a kernel-hosted fallback.

The immutable boot archive image is given one privileged service capability
when spawned. Syscalls 21–24 reject all callers without this capability.
Input commands are copied into a fixed-size kernel queue (64 events); when the
queue is full, the oldest event is dropped. The service submits an **entire
validated display list**, replacing its previous one only when all commands
pass validation. The kernel performs final framebuffer drawing and protects
physical framebuffer mappings from Ring 3 access.

- `SHELL_EVENT` (21): dequeue a mouse/keyboard shell event
- `SHELL_FRAME` (22): atomically submit up to 96 bounded rectangle/text commands
- `SHELL_ACTION` (23): request focus, launch a legacy app, taskbar activation,
  menu policy state, or stop
- `SHELL_WINDOW` (24): query bounded window metadata including position and z-order
- `SHELL_ACTION` also accepts `MOVE` with packed x/y coordinates and
  `LAUNCH_NATIVE` for a fixed allowlist (bounds and launch paths enforced in Ring 0)

The rendering commands use background (`BG_RECT`, `BG_TEXT`) and foreground
(`RECT`, `TEXT`) layers. Ring 3 lays out and commits both at once; the
kernel rasterizer composites background, then application contents, then
the taskbar/menu. When `windowd` stops or times out, the original kernel
wallpaper, icons, Start menu, and taskbar are restored. Window dragging is
chosen in userspace and executed using a bounds-checked MOVE request.

When the service exits or fails to refresh for three seconds, its kernel-owned
overlay and queue are discarded and normal legacy drawing resumes. A smoke
check (`make smoke-shell`) expects `OPENXP_RING3_SHELL_READY` over COM1. Automatic shell launch is disabled for the
existing capability-IPC diagnostic self-test so that its independent service
can keep testing the original protocol.

### Known limitations

This transfers **desktop interaction and composition policy**, not GPU ownership
or all desktop drawing. The privileged kernel still handles framebuffer I/O,
legacy application windows and ordinary window content rendering. The Ring 3
shell now supplies wallpaper and icon drawing commands; it is not a fully
userspace compositor because kernel-owned legacy windows remain. Built-in apps remain Ring 0. The service queries legacy
window metadata via narrow syscalls; it cannot access arbitrary kernel memory.
Window-manager input and composition policy must receive actual QEMU and
VirtualBox verification before this becomes the default release path.

The fixed 1024×768 coordinates currently match the Limine framebuffer request.
Resolution negotiation, surface damage tracking, keyboard repeat, compositor
recovery tests, and migrating legacy apps remain future milestones.

### Application migration boundary

Calculator and Notepad are launched as native Ring 3 ELF applications from the
new Start menu when a process slot is available; a failed launch returns to the
legacy window. The native variants have smaller toolkits and are not yet
feature-identical to the original kernel applications. WordPad, Paint, Mines,
Snake, Calendar, settings, Terminal, and File Manager still run inside the
kernel. Migrating these faithfully requires per-app ports, more than three
native GUI windows, an event-driven GUI toolkit, and VM testing. Track these
items in [Roadmap](ROADMAP.md) rather than treating the shell migration as complete.

### Diagnosing a Ring 3 startup crash

If the `smoke-shell` test reports `USER EXCEPTION pid=1 vector=6`, the CPU
raised an invalid-opcode exception in the native window manager. For `#UD`,
CR2 is **not** the offending address. OpenXP now reports the faulting `rip`,
`rsp`, the linker-relative `image_offset`, and (if safely readable) the first
eight opcode bytes. `scripts/smoke.sh` also annotates such crashes using the
matching `build/apps/windowd.map` symbol map. Failure markers are still
required: a diagnostic does not make a failing smoke test pass.

The current flat native binary format has executable, read-only linked code
and `.data`, plus a separate zero-filled read/write segment for `.bss`.
Until initialized RW data is supported, mutable globals in `windowd` must be
zero-initialized in `.bss` and assigned their desired values in `oxp_main`.
`tools/check_windowd_bss.py` now rejects initialized `.data` in that service.

### Ring 3 CPU instruction policy (October 2026)

The 2026-10-14 `smoke-shell` failure was an invalid-opcode (`#UD`) at
`windowd!oxp_raster_core+0x47` (`66 0f 6e c0`, `movd xmm0,eax`). The
`apps/native/shell_raster.c` build rule previously omitted the no-SSE
compiler flags used by most other Ring 3 programs; its optimized row-fill
loop consequently contained XMM instructions. OpenXP does **not yet** enable
and save/restore userspace SSE/FPU state across processes, so these instructions
cannot safely execute.

`Makefile` now gives **every Ring 3 C object** (including the runtime and
rasterizer) one integer-register-only CPU policy. Before packing initrd,
`tools/check_ring3_isa.py` disassembles every app object and refuses any
SIMD/x87 instructions. `make kernel` and `make test-unit` exercise this guard;
`tests/test_ring3_isa.py` verifies that it catches the fault opcode.

This is an interim compatibility choice rather than full SSE support. When
userspace floating-point/SIMD becomes necessary, add correct CR0/CR4 setup,
feature detection and per-thread XMM/XSAVE save/restore **first**, then update
the common Ring 3 instruction policy and its validation together.

**Important:** a successful host build or unit test is not evidence that the
live window manager boots. Re-run `make smoke-shell` under QEMU and verify
`OPENXP_RING3_SHELL_READY` before treating any startup defect as closed.

---

## COMPOSITOR

## Compositor architecture

OpenXP separates window-management **policy** from privileged framebuffer
access. The Ring 3 `windowd` process owns desktop composition decisions,
wallpaper/icon/taskbar/start-menu layout, focus requests, and window order.
The low-level kernel still validates ownership and performs physical scanout.

### Ring 3 pixel-rasterized shell layers

`windowd` now software-rasterizes its **background** and **overlay** drawing
commands into RGB565 scanlines in its own process, using
`apps/native/shell_raster.c`. These pixels cross the Ring 3 boundary through
capability-checked system calls, not a mapped hardware framebuffer:

```text
windowd Ring 3
  | 5x7 glyphs + rectangles -> RGB565 scanlines
  | SHELL_RASTER_ROW (one 1024-pixel row per call)
  | SHELL_RASTER_COMMIT (one or both complete layers)
  v
kernel: check service capability + page permissions
  | copy row into inactive bounded layer bank
  | reject incomplete layer commit
  | atomically switch banks for committed layers
  v
framebuffer: background -> app windows -> transparent shell overlay -> cursor
```

The background is opaque; the overlay uses RGB565 `0x0001` as its
transparent color key. This reserves that one RGB565 value, so true alpha
blending is **not** yet supported. Each layer has two 1024×768 buffers in the
kernel, allowing a complete frame to be staged without replacing the currently
published layer. `windowd` uploads only changed layers and keeps the previous
validated command-based path as a compatibility fallback.

The executable has no direct framebuffer access. All row pointers are range-
checked and copied from userspace; only the active window-service capability
can upload or commit. A full set of scanlines is mandatory before publication.
The service remains responsible for selecting validated back-to-front window
indices. The kernel still composites **native application surfaces and legacy
Ring 0 windows** between the two Ring 3 shell layers.

### Fallback and limitations

If `windowd` exits, hangs, or fails its watchdog, the kernel returns to its
legacy wallpaper/menu/taskbar/window-composition path. The scanline buffers are
inaccessible to normal Ring 3 applications. Legacy apps are not removed in
this update; they lack demonstrated feature parity and successful interactive
VM regression results. The normal image remains an experimental OS, not a
secure, production-grade desktop.

Physical scanout, video device access, low-level input routing, and portions
of window composition remain in Ring 0. This update is **actual Ring 3 pixel
rasterization of shell layers**, **not** full Ring 3 composition of independent
application surfaces or elimination of all kernel UI code. The 1024×768 fixed
mode, one color key and large double buffers will need a scalable redesign
for variable resolutions and graphical acceleration.

### Verify

```bash
make test-shell-raster
make test-shell-bridge
make test-compositor-integration
make test-unit
make test-sanitize
make smoke-raster  # requires bootable ISO, Limine and QEMU
```

`OPENXP_RING3_RASTER_READY` is emitted after both pixel layers are accepted
and committed by the kernel. This marker **does not prove that visible pixels
are correct**. Check taskbar, Start, dragging, overlapping windows, mouse
latency and watchdog/fallback manually in VirtualBox before retiring any
legacy controllers.

### Ring 3 native-window scene

`windowd` now collects **read-only, validated snapshots** of each native window
and its RGB565 application surface through the service-authorized
`NATIVE_INFO` / `NATIVE_ROW` syscalls. It builds window chrome, the content,
stacking and a transparent RGB565 composition layer **in userspace**, then
publishes that layer atomically using the existing scanline protocol.

The kernel holds both the last published layer and a staging layer. A partial
scene is never presented. The kernel copies the published pixels to the
framebuffer, with clipping. The old native-window renderer remains the
fallback if the window service exits, fails to publish, or is disabled.

Only app-surface content and window metadata cross this read-only interface:
no physical framebuffer addresses, writable shared kernel pointers, or
arbitrary process memory are exposed. The authoritative surface owner remains
tracked by the kernel, while native graphical composition executes in Ring 3.

**Limitations:** The service currently uses an RGB565 color-key raster and
up to three native windows. For compatibility, legacy Ring 0 windows are still
rendered beneath the native layer rather than interleaved with it in one
single globally ordered scene. GPU acceleration, Ring 3 rendering of legacy
programs, verified tear-free app snapshotting under concurrent updates, and
complete isolation are not achieved. Retire legacy controllers only after
VirtualBox/QEMU parity and fallback tests pass.

#### Checks

- `make test-native-scene` checks pixels, overlap, clipping and minimization.
- `make test-shell-bridge` checks atomic native layer publication and fallback.
- `make smoke-native-compose` is a QEMU-only test that launches the Canvas ELF
  program and requires `OPENXP_RING3_NATIVE_PIXELS_READY`.

### Frame presentation and flicker mitigation

The physical framebuffer is not a safe scratch canvas: building a complete
scene there exposes half-painted windows while it is being composed. The
kernel now prepares a coherent frame in a bounded 1024×768 RAM backbuffer,
checks whether the window service changed the scene during composition, and
only then copies the finished frame to the guest display. A concurrent Ring 3
commit causes a bounded retry instead of publishing a mixed-generation frame.

Redundant kernel repaint requests are suppressed while `windowd` owns the
native pixel layer; publication of a new completed layer triggers a repaint.
Window moves, taskbar updates and active-window repaints use clipped software
recomposition to avoid redrawing unrelated areas. Full-frame rendering embeds
the software cursor in the completed frame, preventing a long cursor disappearance
while the display copy is in progress. The PS/2 cursor-only path stays direct
for responsiveness. The temporary backbuffer is kernel-owned, not mapped into
Ring 3; the final video memory access remains privileged.

**Limitation:** This reduces partial-frame flicker but is not synchronized to
VirtualBox's virtual display refresh (true vsync) and does not provide GPU
acceleration. Nonstandard framebuffer dimensions larger than 1024×768 still
use the previous clipped direct path. Guest validation remains necessary.

Regression tests: `make test-present test-present-sanitize`, then verify native
window focus, dragging, text edits, the taskbar clock and cursor motion with
the updated ISO in VirtualBox.

### Framebuffer presentation and tearing

OpenXP first paints the complete scene into an off-screen 32-bit RAM buffer.
The final presentation step now compares the finished frame with the actual
framebuffer and **writes only changed pixels**. This avoids rescanning static
window contents on the display device on every desktop tick or shell update.
Mouse input is also coalesced to a single cursor-underlay update per event burst.

This reduces redundant VirtualBox scanout writes; **it is not hardware page
flipping or vertical-blank synchronization**. Scrolling, dragging, or changing
large portions of the screen can still exhibit some visible tearing. Continue
testing in a VM, including recovery after windowd is terminated.

---


## Aurora 17 runtime-maintenance notes

The Ring 3 `windowd` service must treat an ordinary idle IPC timeout as an
expected no-message condition, not as a fatal disconnection. Theme-change
requests and saved appearance preferences use asynchronous notification and
reload paths. The native Terminal/compositor work includes partial scanline
updates and shorter interrupt-disabled drawing batches, with full redraws for
window geometry, focus, and theme transitions.

These fixes are source-level work: successful compilation and host tests do
not prove smooth Terminal interaction or working live theme switching in
VirtualBox. Test both with the exact newly built ISO.
