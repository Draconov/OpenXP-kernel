# Aurora 3 — bitmap-backed controls

The original `widgets-atlas.png` (192×128) and `icons-32.png` (192×64)
are now **runtime resources**, not unused design references. They are packed
using standard-library-only `tools/pack_ui_atlases.py` into separate `OXUA`
RGB565+A8 files stored in the initrd under `windows/resources/`.

## Implemented

- Run, Settings, and WordPad load a bounded read-only widget atlas. File Manager
  loads both widget and icon atlases; all retain procedural drawing as fallback.
- Buttons (normal/primary/pressed), text fields (normal/focused), tabs
  (selected/unselected), and scrollbars are driven by sprites. Text remains
  real text drawn over the graphics.
- File Manager list icons use original folder, computer, document, painting,
  and settings artwork, reduced to 13×13 with nearest-neighbor sampling.
- Source PNGs are never automatically regenerated during a build.
- Each bitmap drawing request transfers at most 32×32 RGB565+A8 pixels; the
  kernel validates the entire request, bounds, and window owner. Alpha is
  composited into the caller's own native window backing store only.
- Nine-slice controls retain undistorted corner geometry when resizing.
- Incomplete, unavailable or malformed atlas data trigger the previous native
  flat rendering. No trust is placed in graphics metadata from the resource.

## Atlas coordinates (pixels)

`widgets-atlas.png`: buttons are 63×28 at (0,0), (64,0), (128,0);
textboxes are 63×27 at (0,32) and (64,32); tabs at (0,64) and
(64,64), 63×27; scrollbar track strip (55,97,8,27), thumb
(8,99,22,23). Each image maps to one themed control state.

`icons-32.png`: row-major six 32×32 icons in the first row:
folder, computer, notepad, paint, settings, terminal. These master
files can be replaced in-place at any time and repacked by `make kernel`.

## API and limits

Syscall 34 (`OPENXP_SYS_GUI_BITMAP`) takes a copied 3,088-byte
`native_bitmap_req_t`, whose 32×32 maximum prevents arbitrary-size transfers.
Pixels are RGB565 little-endian with 8-bit alpha, clipped to the logical
app canvas. 16-byte OXUA header: `OXUA`, u16 version=1, u16 width,
u16 height, u16 reserved=0, u32 pixel data length. Atlas source dimensions
are fixed, so malicious metadata cannot cause an unbounded allocation.

This first implementation copies 32×32 tiles per draw and nearest-neighbor
scales them in Ring 3. Transparent pixels preserve the underlying app
surface. The design prioritizes correctness and isolation over performance.
A later renderer can batch multiple tiles to reduce syscall overhead.

## Validation

`make test-ui-atlas test-native-widgets`, `make test-unit`, and
`make test-sanitize` cover art packing and validation, clipping, ownership,
widget geometry, and the existing compositor tests. Actual GUI visuals and
latency still need QEMU/VirtualBox testing.
