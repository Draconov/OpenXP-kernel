# Theming

Consolidated from the Aurora 17 source tree. All original technical instructions are retained below, grouped by topic.

## Sections

- [VISUAL SYSTEM](#visual-system)
- [SKIN ENGINE](#skin-engine)
- [AURORA WIDGETS](#aurora-widgets)
- [AURORA BITMAP CONTROLS](#aurora-bitmap-controls)
- [AURORA DESKTOP](#aurora-desktop)
- [AURORA THEMES](#aurora-themes)

---

## VISUAL SYSTEM

## OpenXP visual design system: **Aurora** (proposal)

The desktop shell currently draws most controls from flat colored rectangles and
5x7 ASCII glyphs. This is useful for bring-up, but it makes the OS feel like a
developer demonstration instead of a finished desktop. Do **not** hand-paint
individual screenshots or embed fixed window sizes in applications.

### Recommendation: art-driven, scalable window skins

Use **artist-authored reusable assets** for chrome, controls, menus and icons.
Keep the kernel's compositor and window-management policy unchanged. Render the
assets in Ring 3 `windowd`, without a new framebuffer privilege or extra
per-app art loaders. Begin with one coherent XP-era-inspired visual language,
then expose alternatives through Settings.

The reference style **Aurora** is softly dimensional, inspired by early-2000s
consumer desktops without duplicating their proprietary visual files:

- **Windows:** curved upper corners, graduated blue active title bar, inactive
  slate title bar, 1px highlights, 2px borders and subtle drop shadows.
- **Taskbar:** deep blue to light blue vertical gradient, clearly inset active
  task buttons, rounded mint Start button and a small recessed notification
  area; height remains 40px on the current 1024x768 screen.
- **Surfaces:** warm off-white (`#F6F7F9`), pale blue secondary (`#E6EFF9`),
  dark navy text (`#243B5D`) and selection blue (`#BBD8FB`).
- **System icons:** friendly 3/4 perspective at 32px, 16px small variants,
  a restrained highlight and one consistent light direction (top-left).
- **Typography:** choose a properly licensed bitmap/vector UI font. The
  existing 5x7 glyphs are diagnostics only, not a final interface font.
- **Microdetails:** consistently spaced 4/8px insets, matching hover/pressed
  states, separators, focus rectangles, and icons aligned to text baselines.

### Hand-designed source assets

| Artist's file | Working size | Runtime treatment |
|---|---:|---|
| `window/frame-active.png` | 96 x 96 | 9-slice with defined borders |
| `window/frame-inactive.png` | 96 x 96 | Same 9-slice margins |
| `window/caption-buttons.png` | 3 x 3 tiles of 24 x 24 | Minimize / maximize / close: normal, hover, pressed |
| `taskbar/bar.png` | 64 x 40 | 3-slice horizontal tiling or stretching |
| `taskbar/start.png` | 3 states of 120 x 40 | Normal / hover / pressed |
| `controls/buttons.png` | 3 x 3 tiles of 32 x 24 | Normal, hover, pressed, disabled etc. |
| `icons/*.png` | 16x16 and 32x32 | App icons, folders, files, drives, trash |
| `wallpaper/*.bmp` | 1024 x 768 BMP24 | Already supported by current build |
| `fonts/*` | font-specific | New font renderer needed before use |

These are **source-art targets**, not supported formats in the current OS. The
current wallpaper pipeline accepts only uncompressed 24-bit BMP. PNG decoding,
alpha blending, atlas decoding, 9-slice rasterization and stateful control
styles are **not implemented** by this update; a UI asset importer is the next
required programming step. Keep editable masters (Aseprite, Krita, Inkscape,
GIMP) separate from their eventual runtime RGB565/1-bit-mask output.

#### Safe implementation order

1. **Skin engine**: build and test a PNG/BMP-to-RGB565/alpha-mask *build-time*
   converter; store immutable assets in the initrd; render using 9-slice in
   `windowd`, clipping to screen rows. Use no extra supervisor calls.
2. **Window frames and caption buttons**: one frame design reused across all
   apps. This produces the greatest visible improvement for the least art.
3. **Taskbar / Start / desktop icons**: shared shell assets, stateful pointer
   hover/pressed handling, and a consistent 16px / 32px icon language.
4. **Common widgets and font**: reusable button, text field, list, toolbar,
   checkbox, scroll bar and dialog widgets; replace per-app rectangles.
5. **Application artwork**: update Explorer, WordPad and Paint to use the
   common widget set while retaining responsive layout and keyboard input.

#### Resize rules

Never scale a whole chrome screenshot. 9-slice the corners and borders, tile or
stretch only center strips, and let actual app content reflow. Runtime native
canvases are bounded to 640x360 logical pixels, with fractional nearest-neighbor
scaling for the final display. Use high-resolution art masters even when the
first renderer downconverts to RGB565.

#### Suggested contributors' workflow

Draw one active + inactive frame and three caption buttons first. Send those
editable assets along with a short palette/spacing guide. Keep each UI part
as its own named asset. Validate visual results in screenshots of 240x190,
470x300, 640x480 and maximized 1024x726 windows, including long text and a
partially scrolled document.

### Stage 1 implementation (supersedes the original proposal above)

A tested PNG-to-`OXSK` converter, read-only initrd resource, bounded RGB565+A8
rasterizer, 9-slice active/inactive Ring 3 window frames, caption atlas and
artwork-backed Start/taskbar **are now present**. Detailed instructions and
accurate source-art dimensions are in [`SKIN-ENGINE.md`](THEMING.md#skin-engine).
The earlier list of proposed image sizes remains only a concept for future
artist-authored assets; it should not be treated as the current supported API.

---

## SKIN ENGINE

## OpenXP Aurora Skin Engine (stage 1)

The desktop visual pipeline now supports real artist-authored skins **in Ring 3**.
The reference assets are original starter artwork (not copied Microsoft files),
intentionally simple so they are easy to replace in your preferred pixel-art tool.

### Quick replacement workflow

1. Open `assets/ui/aurora/` in **Aseprite**, **Krita**, **GIMP**, or another PNG editor.
2. Edit the individual assets without changing their canvas size, then save as
   **8-bit RGB or RGBA, non-interlaced PNG**. Preserve transparency where needed.
3. Run `make kernel` or push to GitHub. `Makefile` repacks changed images into
   `build/skin/aurora.oxs` and embeds that file in the boot initrd automatically.
4. Boot the updated ISO; `windowd` reads `windows/resources/aurora.oxs` once at startup.
   Invalid/missing skin resources fall back to the previous procedural design.

For a quick local asset check:

```shell
python3 tools/pack_skin.py build/skin/aurora.oxs --source assets/ui/aurora
make test-skin test-native-scene-skin
```

The converter uses **Python standard library only**; Python imaging packages
are not needed in CI. The runtime kernel/userspace does not decode PNG.

### Original art dimensions and pixel atlas

| File | Size | Runtime behavior |
| --- | --- | --- |
| `frame-active.png` | 64×64 | 9-slice; margins left 8, right 8, top 34, bottom 6 |
| `frame-inactive.png` | 64×64 | Same margins; visible on unfocused native windows |
| `caption-buttons.png` | 72×72 | Nine 24×24 tiles, rendered to 21×21 controls |
| `taskbar.png` | 48×40 | 9-slice 8/8/2/2 to full width taskbar |
| `start.png` | 120×40 | 9-slice 12/12/4/4 to 105×35 button |

Caption atlas layout (columns left to right: normal, hover, pressed):

```text
                     normal    hover    pressed
  Row 0: minimize     (0,0)   (24,0)    (48,0)
  Row 1: maximize     (0,24)  (24,24)   (48,24)
  Row 2: close        (0,48)  (24,48)   (48,48)
```

**9-slice rule:** artwork in the eight border/corner zones keeps its geometry;
only the center row/column areas stretch. Do not paint large details across
those center strips. Use transparent pixels for rounded frame corners.

For now `start.png` includes the START lettering as pixels. Change it directly
in the file. Desktop menus still use the original 5×7 debug font.

### What is implemented

- PNG (RGB/RGBA) -> validated `OXSK` version 1 converter at build time.
- A read-only resource included in the initrd, accessed with the existing
  `APP_FS_READ_SLICE` API; no new kernel privilege or framebuffer mapping.
- A bound-checked `OXSK` parser, fixed-capacity 64 KiB byte buffer, and
  transparent RGB565/A8 scanline blitting in userspace.
- 9-slice window chrome, active/inactive frames and 3-state caption atlas
  for native Ring 3 application windows.
- Art-driven taskbar and Start background, with existing live task buttons,
  system clock, and text drawn on top.
- Unit tests for malformed images, clipping, tiny/large windows, alpha
  transparency, hover/pressed art, with ASan+UBSan versions.
- Graceful fallback if a skin file cannot be loaded or parsed.

### Current limitations and next stages

The redesigned frame currently covers **Ring 3-native application windows**.
Legacy Ring 0 managed windows keep their old border until they are migrated to
user-space composition. Painting a skin does not silently bypass the existing
kernel window hit-test logic or change the syscall ABI.

Other remaining improvements:

1. Create shared controls (textbox, list, toolbar, scrollbar, menus) and fonts
   so application interiors match their beautiful new outer frame.
2. Supply a coherent 16×16/32×32 icon atlas and image-based desktop shortcuts.
3. Let Settings select entire theme resource packs at runtime (Aurora Blue,
   Aurora Olive, Aurora Midnight). The current theme setting changes the old
   palette/wallpaper; it does not yet switch the skin pack.
4. Extend skinning to legacy windows once their decoration is moved out of
   Ring 0. Retain a safe fallback for diagnostic and recovery boot paths.

### Format (OXSK v1)

Header: magic `OXSK`, `u16 version=1`, `u16 record_count`, `u32 total_bytes`,
`u32 reserved=0`. Each record is 20 bytes: seven `u16` fields (`id`, `width`,
`height`, `left`, `right`, `top`, `bottom`), `u16 flags=0` and `u32 pixel_offset`.
Pixels are tightly packed row-major `RGB565 little-endian` + `8-bit alpha`.
Maximum resource size: **65,536 bytes**; maximum 8 sprites; each sprite at most
256×256. The parser verifies counts, offsets, IDs, borders and pixel bounds
before rendering. Packed resource lives on the initrd, *not* a writable filesystem.

### Aurora 2: shared application controls and finished art masters

The first shared native control library is in `apps/native/ui_widgets.h`.
Four applications have been migrated to its reusable control primitives.
Original editable source art is shipped in `assets/ui/aurora/`; the only
repository preview is `docs/desktop-preview.png`. See `THEMING.md#aurora-widgets` for the exact component
coverage and the distinction between runtime skin images and future icon/
widget bitmap masters.

---

## AURORA WIDGETS

## Aurora 2: shared native UI controls + real editable art

This is the first unified application-interior styling pass, built on the
existing Ring 3 image-backed frame engine.

### What is connected today

- **windowd** displays original `assets/ui/aurora/frame-*.png`,
  `caption-buttons.png`, `start.png`, and `taskbar.png` from the OXSK resource.
- `apps/native/ui_widgets.h` provides a self-contained, reusable control
  drawing API with `oxp_ui_panel`, `oxp_ui_toolbar`, `oxp_ui_button`,
  `oxp_ui_textbox`, `oxp_ui_checkbox`, `oxp_ui_tab`, `oxp_ui_progress`,
  `oxp_ui_scrollbar`, and `oxp_ui_menu` plus hit-testing and clipped labels.
- Run now uses toolbar, framed content panel, textbox, and Run button.
- Settings now uses shared toolbar, panel, tabs, navigation buttons and Save.
- File Manager now uses shared toolbar, navigation buttons, scrolling track,
  and right-click menu background.
- WordPad now uses a shared toolbar and document scrollbar.

Controls are **drawn using the existing bounded native GUI commands** to
avoid adding a new pixel-transfer syscall in this pass. Text/control states,
keyboard commands, and window ownership remain application-specific. The
widget API is intentionally header-only so no new user-space linker symbols
or unmapped absolute addresses are introduced.

### Editable artwork included

Browse the original editable PNG assets in `assets/ui/aurora/`. Historical
artwork contact sheets were removed to reduce the repository file count. In Aurora 2, `widgets-atlas.png` and `icons-32.png` were design-only masters.
**Aurora 3 now loads them as actual bitmap resources** in supported apps.

The current OXSK pack has a **64 KiB maximum** and includes five shell
sprites. Adding large widget/icon atlases to it would exceed that capacity.
Aurora 3 introduces an independently validated UI atlas and a bounded native
bitmap upload syscall; Run, Settings, File Manager, and WordPad render the
first image-backed controls. See `THEMING.md#aurora-bitmap-controls`.

### Editing and testing

1. Edit the PNG masters in `assets/ui/aurora/`; preserve pixel dimensions
   and 9-slice margins from `THEMING.md#skin-engine`.
2. Run `make kernel` or push the updated files through GitHub Actions.
3. Test with `make test-native-widgets test-skin test-native-scene-skin`.
4. `make test-unit` also verifies artwork inventory/dimensions/PNG CRCs.
5. Boot the ISO in QEMU/VirtualBox to evaluate hit areas, caption spacing,
   narrow windows, and real frame latency. VM testing is separate from
   compilation and unit testing.

The retained README desktop preview is host-rendered, not evidence of a
successful QEMU or VirtualBox session.

**Aurora 3 update:** bitmap-backed widgets and icon artwork are now connected; see `THEMING.md#aurora-bitmap-controls`.

---

## AURORA BITMAP CONTROLS

## Aurora 3 — bitmap-backed controls

The original `widgets-atlas.png` (192×128) and `icons-32.png` (192×64)
are now **runtime resources**, not unused design references. They are packed
using standard-library-only `tools/pack_ui_atlases.py` into separate `OXUA`
RGB565+A8 files stored in the initrd under `windows/resources/`.

### Implemented

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

### Atlas coordinates (pixels)

`widgets-atlas.png`: buttons are 63×28 at (0,0), (64,0), (128,0);
textboxes are 63×27 at (0,32) and (64,32); tabs at (0,64) and
(64,64), 63×27; scrollbar track strip (55,97,8,27), thumb
(8,99,22,23). Each image maps to one themed control state.

`icons-32.png`: row-major six 32×32 icons in the first row:
folder, computer, notepad, paint, settings, terminal. These master
files can be replaced in-place at any time and repacked by `make kernel`.

### API and limits

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

### Validation

`make test-ui-atlas test-native-widgets`, `make test-unit`, and
`make test-sanitize` cover art packing and validation, clipping, ownership,
widget geometry, and the existing compositor tests. Actual GUI visuals and
latency still need QEMU/VirtualBox testing.

---

## AURORA DESKTOP

## Aurora 4 — Desktop & Explorer Polish

The Ring 3 shell now reads the same `assets/ui/aurora/icons-32.png` as File Manager.
The editable 192x64 PNG is packed into `windows/resources/aurora-icons.oxua`
by `tools/pack_ui_atlases.py` at build time. The first six 32x32 icons remain
unchanged. Six new original icons occupy the second 32-pixel row: Calculator,
Mines, Clock, Snake, Calendar, and Clipboard. Preserve tile positions when
replacing artwork because application icon IDs are stable.

### Behavior

- Five desktop shortcuts use transparent icon artwork, clear captions, and hover
  highlights. Shortcuts are My Computer, Terminal, Paint, Settings, WordPad.
- Shell icon hit testing and placement are centralized in `shell_layout.h`.
  Icons blocked by application windows cannot launch through those windows.
- The Start menu has a branded header, highlighted rows, and icons for all
  14 app entries. Keyboard selection and pointer targets share the same grid.
- Taskbar buttons display compact 18px icons when space allows, and task names
  are clipped to the visible button rather than overflowing adjacent buttons.
- All artwork decoding and alpha blending happens in `windowd` Ring 3. Kernel
  syscalls, filesystem policy and graphics privileges have not changed.
- If the atlas is unavailable or malformed, text-based shell controls remain
  functional; only their icons disappear. The bitmap format is bounded and
  validated before the shell uses it.

### Editing

Edit `assets/ui/aurora/icons-32.png` (192x64, transparent RGBA, 6x2 grid,
32x32 tiles) with Aseprite, GIMP or Krita. Building repacks the PNG; original
artwork is never regenerated automatically.

### Testing & limitations

`make test-shell-icons` and `make test-shell-icons-sanitize` cover resource
validation, alpha blending, clipping and hit-test geometry. The standard
`make test-unit` and `make test-sanitize` include these tests. Actual VM
appearance and usability still require an interactive QEMU/VirtualBox check.
Bitmap application interiors are not all reskinned; this phase focuses on
shell desktop, Start menu, taskbar, and cohesive icon reuse with File Manager.

---

## AURORA THEMES

## Aurora 5 — runtime appearance themes

Aurora 5 completes the connection between **Settings → Appearance** and the
Ring 3 graphical skin resources. The numeric theme ID in `home/appearance.cfg`
is unchanged: 0 = Aurora Blue, 1 = Olive Green, 2 = Midnight.

### How to switch

1. Start OpenXP with the Ring 3 desktop running.
2. Open **Settings / Display Properties** from Start.
3. Select the Blue, Olive, or Night swatch (keyboard shortcuts `1`, `2`, `3`).
4. The window manager validates the requested pack before applying it. A
   successful request changes the native window decorations, Start button,
   taskbar, desktop / Start icons, and shell accent colors immediately.
5. The preference is saved in the RAM-backed `home/appearance.cfg` and native
   Run, Settings, File Manager and WordPad automatically refresh their bitmap
   controls after detecting a preference change. They do not need restarting.

Selecting a color theme turns off a previously selected BMP wallpaper, as it
already did in Aurora 4. Choosing a BMP wallpaper does not replace your chosen
window skin. The appearance preference lives in RAM unless OpenXP's optional
storage persistence is explicitly configured and supported. Do not assume a
new boot retains it.

### Actual editable artwork — 24 PNG files (Aurora 6)

| Theme | Editable PNG directory | Read-only initrd resources |
| --- | --- | --- |
| Aurora Blue | `assets/ui/aurora/` | `aurora.oxs`, `aurora-widgets.oxua`, `aurora-icons.oxua` |
| Olive Green | `assets/ui/aurora/olive/` | `olive.oxs`, `olive-widgets.oxua`, `olive-icons.oxua` |
| Midnight | `assets/ui/aurora/midnight/` | `midnight.oxs`, `midnight-widgets.oxua`, `midnight-icons.oxua` |

Each directory contains these **eight image masters** at the same dimensions:
`frame-active.png` 64×64, `frame-inactive.png` 64×64,
`caption-buttons.png` 72×72, `taskbar.png` 48×40, `start.png` 120×40,
`widgets-atlas.png` 192×128, `icons-32.png` 192×64,
and `chrome-strip.png` 192×64. See [Aurora 6](AURORA-RELEASES.md#aurora-6) for the
app-header, toolbar and statusbar regions.

The Aurora theme sprite masters are preserved; redundant theme preview
contact sheets were removed. Source sprites can be edited with
Aseprite/GIMP/Krita. Preserve each image's dimensions, atlas tiles, and the
current 8-bit RGB/RGBA non-interlaced PNG encoding. The build converts images
to compact bounded OXSK/OXUA formats with Python standard library only.
The source PNG files are NEVER regenerated or overwritten during builds.

`tools/create_theme_variants.py` is the optional one-time Pillow-based **authoring**
script used to produce Olive/Midnight masters from the original Blue PNGs. It
will overwrite their files if explicitly invoked; do NOT run it after making
hand edits unless you intend to replace those edits. It is not part of make.

### Implementation and safety

- `apps/native/theme.h` centralizes stable IDs, full resource names, and
  shared appearance palette values. No new syscalls or elevated privileges.
- `windowd` validates the replacement skin and icon atlas before accepting
  a `WM_THEME` message; a corrupt pack falls back to the previous selection.
  The prior drawn frame and native scene are invalidated to force repaint.
- Four bitmap-native applications poll the tiny preference file only during
  idle GUI waits; a changed preference atomically refreshes their in-process
  widget and icon buffers before the next repaint. No busy redraw loop.
- All twelve resources are included in the OXRD initrd. The original Blue names
  remain unchanged for compatibility. Old procedural graphics remain fallback.
- Legacy Ring 0 windows and applications not yet converted to shared UI
  widgets do not receive complete bitmap-backed interior theming. This
  release is full theme resource switching for the existing Ring 3 Aurora
  pipeline, **not** full feature parity for every legacy screen.

### Verify

```sh
make kernel
make test-aurora-themes
make test-unit
make test-sanitize
make smoke-shell # needs graphical/headless QEMU + Limine bootloader
```

The build and host-side tests do not prove real VM visuals or performance.
Check all three selections in QEMU/VirtualBox before calling them tested there.

---


## ARTWORK-INDEX.txt (original asset index)

```text
OpenXP Aurora 4 — EDITABLE ARTWORK & SOURCE INCLUDED
====================================================
START HERE: README.md (single desktop preview) or edit the sprites directly

Original source artwork — assets/ui/aurora/
  frame-active.png       64x64    9-slice active window chrome
  frame-inactive.png     64x64    9-slice inactive window chrome
  caption-buttons.png    72x72    minimize / maximize / close states
  taskbar.png            48x40    taskbar 9-slice
  start.png             120x40    Start button 9-slice
  widgets-atlas.png     192x128   bitmap-backed app controls
  icons-32.png          192x64    12 32x32 icons (two rows)
  icons-32.png          192x64    original editable icon atlas

Aurora 4 connects the icon atlas to desktop shortcuts, Start menu, taskbar,
and File Manager. All icons can be modified in a PNG editor. The packers run
when building the OS; the optional art authoring script is not part of builds.

See THEMING.md#aurora-desktop, THEMING.md#aurora-widgets, THEMING.md#skin-engine.
VM runtime appearance still needs verification; the README image is host-rendered.

Aurora 6 editable chrome atlas per theme (192x64):
assets/ui/aurora/chrome-strip.png
assets/ui/aurora/olive/chrome-strip.png
assets/ui/aurora/midnight/chrome-strip.png
Redundant Aurora 6 contact-sheet image removed; editable source PNGs retained.
See AURORA-RELEASES.md#aurora-6 for 9-slice coordinates.

Aurora 7 editable Start-menu atlas (192x128 PNG, six source regions):
  assets/ui/aurora/menu-atlas.png
  assets/ui/aurora/olive/menu-atlas.png
  assets/ui/aurora/midnight/menu-atlas.png
Aurora 7 preview contact sheet removed; original menu-atlas PNGs retained.
```


## AURORA-8-ARTWORK-INDEX.txt (original asset index)

```text
Aurora 8 artwork map

fonts/aurora-font.png: 176 x 156, 95 ASCII cells regular + bold
toolbar-atlas.png: 192 x 64, toolbar + normal / hover / pressed buttons
olive/toolbar-atlas.png: Olive Green variant
midnight/toolbar-atlas.png: Midnight variant

The 3 themes also retain all earlier Aurora PNG masters.
Every PNG in artwork-only.zip is an actual editable source image.
See AURORA-RELEASES.md#aurora-8 for sprite grid coordinates, font size,
fallback behavior and tests.
```
