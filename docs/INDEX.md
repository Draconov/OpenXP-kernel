# Documentation index

OpenXP documentation is organized by subsystem rather than by every small feature release.

- [Aurora release notes](AURORA-RELEASES.md) — original Aurora 6–17 history, unchanged in substance
- [Theming and artwork](THEMING.md) — graphics, sprites, atlas editing and themes
- [Kernel, userspace and compositor](CORE-ENGINEERING.md) — architecture and system APIs
- [Filesystem and storage](FILES-AND-STORAGE.md) — VFS, file management and persistent disk
- [Desktop and native applications](DESKTOP-AND-APPS.md) — interaction, apps and [VirtualBox setup](DESKTOP-AND-APPS.md#virtualbox-setup)
- [Roadmap](ROADMAP.md) — future work
- [Historical test reports](VERIFICATION-HISTORY.md) — release audit trail
- `FONT-LICENSE.txt` — original licensing notice (unmodified)

Host tests are consolidated into seven suite `.c` files. The Makefile still
compiles and executes each original test case independently through the same
`make test-unit` and `make test-sanitize` targets; it passes an `OPENXP_TEST_*`
selector to choose the test within its suite. This preserves isolation of
compiler flags, executable names and sanitizer modes without 60+ separate
source files.

## Host C test suites

| Suite | Original test cases |
|---|---|
| `tests/suite_aurora.c` | `aurora13_edit`, `aurora14_workflow`, `aurora15_workflow`, `aurora16_workflow`, `advanced_input`, `native_pointer`, `native_pointer_kernel`, `native_text_edit`, `native_paint_codec` |
| `tests/suite_filesystem.c` | `ramfs`, `vfs`, `persist`, `initrd`, `initrd_image` |
| `tests/suite_graphics.c` | `fb`, `font_engine`, `font_fit`, `skin`, `native_scene`, `native_scene_skin`, `native_theme`, `shell_raster`, `compositor_integration`, `shell_menu_art`, `shell_icons`, `wallpaper` |
| `tests/suite_shell.c` | `desktop`, `desktop_present`, `shell`, `shell_bridge`, `shell_interaction` |
| `tests/suite_apps.c` | `calculator_layout`, `native_layout`, `native_text_layout`, `native_scroll`, `native_widgets`, `native_games`, `native_window`, `native_wait` |
| `tests/suite_kernel.c` | `crash`, `irq_state`, `timer`, `pci`, `pmm`, `vmm`, `heap`, `heap_stress`, `string`, `sync`, `scheduler`, `context_switch`, `preempt`, `sleep`, `input` |
| `tests/suite_process.c` | `process`, `process_manager`, `app_launch`, `capipc`, `syscall`, `wm_protocol`, `wm_capipc` |

Do not compile these suite files without selecting one case. The Makefile manages the selectors and preserves separate test execution.
