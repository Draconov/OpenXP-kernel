# Aurora artwork: original source PNGs INCLUDED

**The single repository preview is [`docs/desktop-preview.png`](../../docs/desktop-preview.png).**
It is host-rendered, not a screenshot from a running VM. Theme sprites remain editable
under `assets/ui/aurora/`; historical contact sheets were removed.

All editable PNG source files are under `assets/ui/aurora/`, in **both** the
updated-files and full-source ZIP archives:

| Artwork | File | Size | Used at runtime? |
| --- | --- | --- | --- |
| Active/inactive XP-style window frame | `frame-active.png`, `frame-inactive.png` | 64x64 | **Yes**, 9-slice |
| Caption controls (min/max/close × normal/hover/pressed) | `caption-buttons.png` | 72x72 | **Yes**, atlas |
| Taskbar texture | `taskbar.png` | 48x40 | **Yes**, 9-slice |
| Start button | `start.png` | 120x40 | **Yes**, 9-slice |
| Reusable bitmap control artwork | `widgets-atlas.png` | 192x128 | **Yes**, native widget atlas |
| Explorer/application icon art | `icons-32.png` | 192x64 | **Yes**, File Manager + desktop + Start + taskbar |

Runtime sprites load via `windows/resources/aurora.oxs` in the initrd. Build-time
packing uses **only** Python's standard library. UI design is original and
free to modify; no Microsoft assets are copied.

To regenerate this first set of sprites programmatically, use
`python3 tools/create_aurora_art.py` (Pillow required only for this optional
*authoring* script). You can instead edit individual PNG files by hand in
GIMP, Krita, or Aseprite. The build does not call the authoring script, so
manual changes are preserved. The standard `make kernel` workflow repacks
and embeds the five window sprites plus the two OXUA atlases automatically.

See `docs/THEMING.md#skin-engine` and `docs/THEMING.md#aurora-widgets`.

Aurora 4 icon source art is preserved in `assets/ui/aurora/icons-32.png`.

## Aurora 6: editable application chrome

The three theme folders each include a 192×64 `chrome-strip.png`.
It is loaded by native widgets for application headers, toolbars and status bars;
see `docs/AURORA-RELEASES.md#aurora-6` for safe editing and 9-slice margins.
