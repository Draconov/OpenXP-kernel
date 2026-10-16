# Aurora artwork: original source PNGs INCLUDED

**Start with `aurora/AURORA-ARTWORK-PREVIEW.png`.** It is a design mockup
assembled from original Aurora art, not a screenshot of the running OS.

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
| Full-scale visual concept sheet | `AURORA-ARTWORK-PREVIEW.png` | 1240x800 | Preview only |

Runtime sprites load via `windows/resources/aurora.oxs` in the initrd. Build-time
packing uses **only** Python's standard library. UI design is original and
free to modify; no Microsoft assets are copied.

To regenerate this first set of sprites programmatically, use
`python3 tools/create_aurora_art.py` (Pillow required only for this optional
*authoring* script). You can instead edit individual PNG files by hand in
GIMP, Krita, or Aseprite. The build does not call the authoring script, so
manual changes are preserved. The standard `make kernel` workflow repacks
and embeds the five window sprites plus the two OXUA atlases automatically.

See `docs/SKIN-ENGINE.md` and `docs/AURORA-WIDGETS.md`.

Aurora 4 also includes `AURORA-4-ARTWORK-PREVIEW.png` at the repository root.

## Aurora 6: editable application chrome

The three theme folders each include a 192×64 `chrome-strip.png`.
It is loaded by native widgets for application headers, toolbars and status bars;
see `docs/AURORA-6.md` for safe editing and 9-slice margins.
