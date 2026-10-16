# Aurora 5 — runtime appearance themes

Aurora 5 completes the connection between **Settings → Appearance** and the
Ring 3 graphical skin resources. The numeric theme ID in `home/appearance.cfg`
is unchanged: 0 = Aurora Blue, 1 = Olive Green, 2 = Midnight.

## How to switch

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

## Actual editable artwork — 24 PNG files (Aurora 6)

| Theme | Editable PNG directory | Read-only initrd resources |
| --- | --- | --- |
| Aurora Blue | `assets/ui/aurora/` | `aurora.oxs`, `aurora-widgets.oxua`, `aurora-icons.oxua` |
| Olive Green | `assets/ui/aurora/olive/` | `olive.oxs`, `olive-widgets.oxua`, `olive-icons.oxua` |
| Midnight | `assets/ui/aurora/midnight/` | `midnight.oxs`, `midnight-widgets.oxua`, `midnight-icons.oxua` |

Each directory contains these **eight image masters** at the same dimensions:
`frame-active.png` 64×64, `frame-inactive.png` 64×64,
`caption-buttons.png` 72×72, `taskbar.png` 48×40, `start.png` 120×40,
`widgets-atlas.png` 192×128, `icons-32.png` 192×64,
and `chrome-strip.png` 192×64. See [Aurora 6](AURORA-6.md) for the
app-header, toolbar and statusbar regions.

`assets/ui/aurora/AURORA-5-THEME-PREVIEW.png` is an overview of the actual PNG
sprites; **it is not a screenshot from a running VM**. Files can be edited with
Aseprite/GIMP/Krita. Preserve each image's dimensions, atlas tiles, and the
current 8-bit RGB/RGBA non-interlaced PNG encoding. The build converts images
to compact bounded OXSK/OXUA formats with Python standard library only.
The source PNG files are NEVER regenerated or overwritten during builds.

`tools/create_theme_variants.py` is the optional one-time Pillow-based **authoring**
script used to produce Olive/Midnight masters from the original Blue PNGs. It
will overwrite their files if explicitly invoked; do NOT run it after making
hand edits unless you intend to replace those edits. It is not part of make.

## Implementation and safety

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

## Verify

```sh
make kernel
make test-aurora-themes
make test-unit
make test-sanitize
make smoke-shell # needs graphical/headless QEMU + Limine bootloader
```

The build and host-side tests do not prove real VM visuals or performance.
Check all three selections in QEMU/VirtualBox before calling them tested there.
