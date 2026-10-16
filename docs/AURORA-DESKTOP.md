# Aurora 4 — Desktop & Explorer Polish

The Ring 3 shell now reads the same `assets/ui/aurora/icons-32.png` as File Manager.
The editable 192x64 PNG is packed into `windows/resources/aurora-icons.oxua`
by `tools/pack_ui_atlases.py` at build time. The first six 32x32 icons remain
unchanged. Six new original icons occupy the second 32-pixel row: Calculator,
Mines, Clock, Snake, Calendar, and Clipboard. Preserve tile positions when
replacing artwork because application icon IDs are stable.

## Behavior

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

## Editing

Edit `assets/ui/aurora/icons-32.png` (192x64, transparent RGBA, 6x2 grid,
32x32 tiles) with Aseprite, GIMP or Krita. Building repacks the PNG; original
artwork is never regenerated automatically.

## Testing & limitations

`make test-shell-icons` and `make test-shell-icons-sanitize` cover resource
validation, alpha blending, clipping and hit-test geometry. The standard
`make test-unit` and `make test-sanitize` include these tests. Actual VM
appearance and usability still require an interactive QEMU/VirtualBox check.
Bitmap application interiors are not all reskinned; this phase focuses on
shell desktop, Start menu, taskbar, and cohesive icon reuse with File Manager.
