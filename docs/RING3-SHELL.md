# Ring 3 desktop shell and compositor policy

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

## Known limitations

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

## Application migration boundary

Calculator and Notepad are launched as native Ring 3 ELF applications from the
new Start menu when a process slot is available; a failed launch returns to the
legacy window. The native variants have smaller toolkits and are not yet
feature-identical to the original kernel applications. WordPad, Paint, Mines,
Snake, Calendar, settings, Terminal, and File Manager still run inside the
kernel. Migrating these faithfully requires per-app ports, more than three
native GUI windows, an event-driven GUI toolkit, and VM testing. Track these
items in [Roadmap](ROADMAP.md) rather than treating the shell migration as complete.

## Diagnosing a Ring 3 startup crash

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

**Important:** a successful host build or unit test is not evidence that the
live window manager boots. Re-run `make smoke-shell` under QEMU and verify
`OPENXP_RING3_SHELL_READY` before treating any startup defect as closed.
