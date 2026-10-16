# Aurora 2: shared native UI controls + real editable art

This is the first unified application-interior styling pass, built on the
existing Ring 3 image-backed frame engine.

## What is connected today

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

## Editable artwork included

Look at `assets/ui/aurora/AURORA-ARTWORK-PREVIEW.png` to see every asset at a
glance; individual originals are adjacent. In Aurora 2, `widgets-atlas.png` and `icons-32.png` were design-only masters.
**Aurora 3 now loads them as actual bitmap resources** in supported apps.

The current OXSK pack has a **64 KiB maximum** and includes five shell
sprites. Adding large widget/icon atlases to it would exceed that capacity.
Aurora 3 introduces an independently validated UI atlas and a bounded native
bitmap upload syscall; Run, Settings, File Manager, and WordPad render the
first image-backed controls. See `docs/AURORA-BITMAP-CONTROLS.md`.

## Editing and testing

1. Edit the PNG masters in `assets/ui/aurora/`; preserve pixel dimensions
   and 9-slice margins from `docs/SKIN-ENGINE.md`.
2. Run `make kernel` or push the updated files through GitHub Actions.
3. Test with `make test-native-widgets test-skin test-native-scene-skin`.
4. `make test-unit` also verifies artwork inventory/dimensions/PNG CRCs.
5. Boot the ISO in QEMU/VirtualBox to evaluate hit areas, caption spacing,
   narrow windows, and real frame latency. VM testing is separate from
   compilation and unit testing.

The artwork preview is a source-art mockup, not evidence of a successful
QEMU or VirtualBox session.

**Aurora 3 update:** bitmap-backed widgets and icon artwork are now connected; see `docs/AURORA-BITMAP-CONTROLS.md`.
