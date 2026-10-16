# OpenXP visual design system: **Aurora** (proposal)

The desktop shell currently draws most controls from flat colored rectangles and
5x7 ASCII glyphs. This is useful for bring-up, but it makes the OS feel like a
developer demonstration instead of a finished desktop. Do **not** hand-paint
individual screenshots or embed fixed window sizes in applications.

## Recommendation: art-driven, scalable window skins

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

## Hand-designed source assets

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

### Safe implementation order

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

### Resize rules

Never scale a whole chrome screenshot. 9-slice the corners and borders, tile or
stretch only center strips, and let actual app content reflow. Runtime native
canvases are bounded to 640x360 logical pixels, with fractional nearest-neighbor
scaling for the final display. Use high-resolution art masters even when the
first renderer downconverts to RGB565.

### Suggested contributors' workflow

Draw one active + inactive frame and three caption buttons first. Send those
editable assets along with a short palette/spacing guide. Keep each UI part
as its own named asset. Validate visual results in screenshots of 240x190,
470x300, 640x480 and maximized 1024x726 windows, including long text and a
partially scrolled document.

## Stage 1 implementation (supersedes the original proposal above)

A tested PNG-to-`OXSK` converter, read-only initrd resource, bounded RGB565+A8
rasterizer, 9-slice active/inactive Ring 3 window frames, caption atlas and
artwork-backed Start/taskbar **are now present**. Detailed instructions and
accurate source-art dimensions are in [`SKIN-ENGINE.md`](SKIN-ENGINE.md).
The earlier list of proposed image sizes remains only a concept for future
artist-authored assets; it should not be treated as the current supported API.
