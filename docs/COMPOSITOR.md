# Compositor architecture

OpenXP separates window-management **policy** from privileged framebuffer
access. The Ring 3 `windowd` process owns desktop composition decisions,
wallpaper/icon/taskbar/start-menu layout, focus requests, and window order.
The low-level kernel still validates ownership and performs physical scanout.

## Ring 3 pixel-rasterized shell layers

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

## Fallback and limitations

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

## Verify

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

## Ring 3 native-window scene

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

### Checks

- `make test-native-scene` checks pixels, overlap, clipping and minimization.
- `make test-shell-bridge` checks atomic native layer publication and fallback.
- `make smoke-native-compose` is a QEMU-only test that launches the Canvas ELF
  program and requires `OPENXP_RING3_NATIVE_PIXELS_READY`.

## Frame presentation and flicker mitigation

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

## Framebuffer presentation and tearing

OpenXP first paints the complete scene into an off-screen 32-bit RAM buffer.
The final presentation step now compares the finished frame with the actual
framebuffer and **writes only changed pixels**. This avoids rescanning static
window contents on the display device on every desktop tick or shell update.
Mouse input is also coalesced to a single cursor-underlay update per event burst.

This reduces redundant VirtualBox scanout writes; **it is not hardware page
flipping or vertical-blank synchronization**. Scrolling, dragging, or changing
large portions of the screen can still exhibit some visible tearing. Continue
testing in a VM, including recovery after windowd is terminated.
