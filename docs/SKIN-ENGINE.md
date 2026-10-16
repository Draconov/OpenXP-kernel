# OpenXP Aurora Skin Engine (stage 1)

The desktop visual pipeline now supports real artist-authored skins **in Ring 3**.
The reference assets are original starter artwork (not copied Microsoft files),
intentionally simple so they are easy to replace in your preferred pixel-art tool.

## Quick replacement workflow

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

## Original art dimensions and pixel atlas

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

## What is implemented

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

## Current limitations and next stages

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

## Format (OXSK v1)

Header: magic `OXSK`, `u16 version=1`, `u16 record_count`, `u32 total_bytes`,
`u32 reserved=0`. Each record is 20 bytes: seven `u16` fields (`id`, `width`,
`height`, `left`, `right`, `top`, `bottom`), `u16 flags=0` and `u32 pixel_offset`.
Pixels are tightly packed row-major `RGB565 little-endian` + `8-bit alpha`.
Maximum resource size: **65,536 bytes**; maximum 8 sprites; each sprite at most
256×256. The parser verifies counts, offsets, IDs, borders and pixel bounds
before rendering. Packed resource lives on the initrd, *not* a writable filesystem.

## Aurora 2: shared application controls and finished art masters

The first shared native control library is in `apps/native/ui_widgets.h`.
Four applications have been migrated to its reusable control primitives.
All original image sources and an artwork preview **are physically shipped**
in `assets/ui/aurora/`. See `docs/AURORA-WIDGETS.md` for the exact component
coverage and the distinction between runtime skin images and future icon/
widget bitmap masters.
