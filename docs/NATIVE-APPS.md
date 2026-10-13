# Native OpenXP applications

The native application pilot separates freestanding x86-64 programs from the
kernel. The installer, dynamic libraries and Windows PE `.exe` loader have **not**
been implemented. All current native files use a custom, constrained ELF64
profile inside the read-only `OXRD` boot archive.

## Current programs

| Source | Image path | Function |
| --- | --- | --- |
| `apps/native/calculator.c` | `bin/calculator` | Integer keyboard calculator |
| `apps/native/clock.c` | `bin/clock` | PIT uptime display |
| `apps/native/notepad.c` | `bin/notepad` | Editable note with temporary RAM saving |
| `apps/native/probe.c` | `bin/probe` | Synchronous GUI and RAM-file ABI smoke probe |
| `apps/native/waitprobe.c` | `bin/waitprobe` | Preemptible Ring 3 GUI wait and timeout smoke probe |
| `apps/native/windowd.c` | `bin/windowd` | Trusted Ring 3 shell/window-policy service with IPC and bounded drawing commands |
| `apps/native/winclient.c` | `bin/winclient` | Separate Ring 3 client that connects to the window service |
| `apps/native/canvas.c` | `bin/canvas` | Demonstrates independent process-owned pixel drawing and click events |

See [Process IPC and window service](IPC.md) for the handle model,
service authorization, syscall arguments and optional QEMU smoke test.

The earlier `bin/hello` and `bin/fault` are still independent assembly-only
ELF64 examples. All other familiar desktop tools currently remain embedded
in the kernel's `kernel/gui/desktop.c` implementation.

## Build and boot

```bash
make kernel            # Builds native apps and packages them into openxp.elf
make test-unit         # Host-side regression tests
make test-sanitize     # Memory/UB checks on supported host components
make smoke-native      # Native Ring 3 ABI boot test (requires QEMU/Limine)
make iso               # Bootable disc image (requires Xorriso/Limine)
```

Use **Run** or **Terminal** to launch `BIN/CALCULATOR`, `BIN/CLOCK`,
`BIN/NOTEPAD`, or `BIN/CANVAS`. They can now be launched concurrently, limited by the
experimental process/scheduler and three-window capacities (host-tested;
interactive VM verification is outstanding). My Computer also lists the files under BIN. The programs are
prebuilt into the read-only ISO, not installed on a persistent disk.

## User ABI

A program enters at `_start` with a mapped, writable private user stack and
invokes `int 0x80`: syscall number in `RAX`, arguments in `RDI` and `RSI`,
result in `RAX`. The kernel validates user address ranges and copies bounded
buffers through the process address-space mapping.

| Number | Operation | Result |
| --- | --- | --- |
| 6 | `GUI_OPEN(user_title, title_len)` | Open one of three PID-owned windows; 0 on success |
| 7 | `GUI_TEXT(user_text, text_len)` | Update native text content; 0 on success |
| 8 | `GUI_EVENT()` | Nonblocking event: ASCII, 8 Backspace, 10 Enter, 27 Escape, `0x10000` close, `0x10001` Ctrl+S, `0x10002` Ctrl+O, or bit 31 mouse click |
| 9 | `RAM_READ(user_buf, max_size)` | Byte count or negative error; fixed temporary note path |
| 10 | `RAM_WRITE(user_buf, size)` | Byte count or negative error; fixed temporary note path |
| 11 | `GUI_DRAW(user_commands, count)` | Atomically validate and draw 1–32 fixed-size commands to the calling PID's private surface; 0 or negative error |
| 12 | `GUI_WAIT(timeout_ticks)` | Atomically dequeue or block until a GUI event; `0` waits indefinitely, positive values set a timer deadline; `0` result means timeout (or unavailable scheduler) |

Native app source includes `apps/native/oxp.h`. A tiny `_start` assembly stub
calls `oxp_main()` with the expected System V AMD64 C stack alignment. The
freestanding runtime supplies the required memory primitives without a libc.
The linker creates a relocation-free native binary, and `tools/mkinitrd.py`
wraps it as `ET_DYN` ELF64 with a bounded executable segment plus separate
zero-initialized writable pages for native `.bss`. This keeps W^X rather
than putting application state in writable executable memory. Initialized
mutable data, dynamic relocations, shared libraries and ordinary Linux/Windows
programs remain unsupported. **Actual VM execution of the new layout still
requires confirmation.**

## Security and compatibility boundaries

- Up to **three** simultaneous PID-owned native windows, each with an
  independent **320×180 RGB565** pixel surface and/or text content.
  The owner is checked for updates and event reads; exit/fault releases its
  window. The fourth concurrent `GUI_OPEN` fails cleanly.
- Input follows window focus, and client mouse click events encode window-local
  x in bits 10–19 and y in bits 0–9, with bit 31 set. Coordinates include
  the window decorations. Resizing is not implemented.
- Drawing commands support **clear**, **filled rectangle**, and **5×7 glyph**
(scale 1–2) operations, with colors `0x00RRGGBB`. The kernel copies and
validates each bounded syscall batch before updating an owner-only surface;
applications never receive a direct framebuffer pointer. `apps/native/oxp.h`
provides `oxp_draw`, `oxp_fill`, and `oxp_label` helpers. This is a small
**native graphics ABI prototype**, not a stable SDK or separate Ring 3 compositor.

- The separate process runs with its own CR3, Ring 3 mappings, and guarded
  user and kernel stacks. Hardware-enforced behavior requires VM testing.
- Window-manager drawing and existing built-in apps still run in Ring 0.
- Native applications now use `oxp_wait()` for scheduler-backed event waits.
  The queue check and park are atomic under disabled local interrupts, and
  GUI delivery wakes the waiting thread. Calculator, Notepad and Canvas wait
  until an event arrives; Clock uses a bounded timeout to repaint the time.
  GUI_CLOSE is a sticky signal even when the input FIFO is full. The older
  `GUI_EVENT` nonblocking syscall remains available.
- The wait path needs real QEMU/VirtualBox verification (`make smoke-native-wait`).
  The separate wait boot test launches a preemptible Ring 3 waitprobe, which exercises a
  timed GUI_WAIT while timer interrupts are enabled. The test does not prove
  a fully protected Ring 3 window server or a complete desktop migration.
- Older native builds used busy-polling loops, so native programs could consume
  CPU even when idle; the scheduler remains experimental.
- The note filesystem is RAM-only, max 2048 bytes through this user API,
  and is lost at reboot. No per-user permissions model is claimed.
- The loader accepts restricted static ELF64, not Microsoft PE `.exe`.
- No guarantees of robustness, sandbox security or VirtualBox behavior until
  the integration tests have passed on the actual ISO.

The remaining migration work is tracked in [Roadmap](ROADMAP.md).
