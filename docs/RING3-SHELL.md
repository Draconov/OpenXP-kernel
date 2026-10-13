# Ring 3 desktop shell and compositor policy

`bin/windowd` now owns taskbar/Start-menu *layout*, selection and global hotkey policy.
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
- `SHELL_WINDOW` (24): query a bounded window metadata record

When the service exits or fails to refresh for three seconds, its kernel-owned
overlay and queue are discarded and normal legacy drawing resumes. A smoke
check (`make smoke-shell`) expects `OPENXP_RING3_SHELL_READY` over COM1. Automatic shell launch is disabled for the
existing capability-IPC diagnostic self-test so that its independent service
can keep testing the original protocol.

## Known limitations

This transfers **desktop interaction and composition policy**, not GPU ownership
or all desktop drawing. The privileged kernel still handles framebuffer I/O,
legacy app windows, mouse dragging and ordinary window drawing, and retains the
initial wallpaper. Built-in apps remain Ring 0. The service queries legacy
window metadata via narrow syscalls; it cannot access arbitrary kernel memory.
Window-manager input and composition policy must receive actual QEMU and
VirtualBox verification before this becomes the default release path.

The fixed 1024×768 coordinates currently match the Limine framebuffer request.
Resolution negotiation, surface damage tracking, keyboard repeat, compositor
recovery tests, and migrating legacy apps remain future milestones.
