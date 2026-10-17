# OpenXP

**An open-source, Windows XP-inspired operating system experiment.**

OpenXP is an original x86-64 kernel written mainly in freestanding C and
assembly. It includes a software-rendered desktop, basic filesystem, and
experimental Ring 3 native applications. It is **not** a Windows distribution
and cannot run ordinary Windows `.exe` files.

> [!WARNING]
> **Research prototype.** OpenXP is not secure or ready for everyday use.
> Run it in a disposable VM. A successful build or CI smoke test does not
> guarantee that the graphical desktop behaves correctly in VirtualBox.

![OpenXP desktop preview](docs/desktop-preview.png)

*Host-rendered project preview, not a screenshot of a running VM.*

## Highlights

- **Classic desktop:** Start menu, taskbar, movable/resizable windows, File Manager,
  Settings, and bundled tools such as Terminal, Paint, Notepad, and WordPad.
- **Aurora appearance:** Original editable Blue, Olive, and Midnight theme artwork
  in [`assets/ui/aurora/`](assets/ui/aurora/).
- **Native application experiments:** Ring 3 ELF64 programs and a window-service
  compositor, with legacy fallbacks for parts of the desktop.
- **Files and storage:** A bounded writable HOME filesystem. The standard ISO is
  RAM-only; persistent storage is experimental and opt-in.

See the [desktop and applications guide](docs/DESKTOP-AND-APPS.md),
[theming guide](docs/THEMING.md) and [storage guide](docs/FILES-AND-STORAGE.md)
for features, limitations and implementation details.

## Build and run

Use **Ubuntu** or **WSL2 Ubuntu** with the following development dependencies:

```bash
sudo apt update
sudo apt install -y clang lld make git python3 xorriso qemu-system-x86
```

From the repository root:

```bash
make test-unit       # Run host-side unit tests
make test-sanitize   # Run ASan/UBSan checks
make kernel          # Build build/openxp.elf
make iso             # Build build/openxp.iso
make run             # Boot in graphical QEMU
```

The first ISO build downloads the [Limine bootloader](https://github.com/limine-bootloader/limine)
and therefore needs internet access. Use `make smoke` for boot-marker checks;
other test targets are documented in [core engineering](docs/CORE-ENGINEERING.md)
and the Makefile. **QEMU is the primary test environment.**

<details>
<summary><strong>Quick VirtualBox setup</strong></summary>

Build `openxp.iso`, or download the ISO artifact from a successful GitHub
Actions run. Create an **Other/Unknown (64-bit)** VM with **512 MB RAM**, **one
CPU**, **BIOS** firmware (EFI off), **PIIX3**, **PS/2 Mouse**, and **VBoxSVGA**
graphics with **3D acceleration disabled**. Attach the ISO as an optical disk
and boot. No virtual hard disk is needed for the standard RAM-only build.

For detailed settings and troubleshooting, follow the
[VirtualBox setup guide](docs/DESKTOP-AND-APPS.md#virtualbox-setup).

</details>

## Documentation

Start with the [documentation index](docs/INDEX.md):

| Guide | What it covers |
| --- | --- |
| [Desktop and applications](docs/DESKTOP-AND-APPS.md) | UI, shortcuts, native apps, VM setup |
| [Core engineering](docs/CORE-ENGINEERING.md) | Kernel, Ring 3, IPC, compositor and testing |
| [Files and storage](docs/FILES-AND-STORAGE.md) | Filesystem, wallpaper assets, experimental persistence |
| [Aurora theming](docs/THEMING.md) | Editable sprites, skin engine and theme packs |
| [Aurora release history](docs/AURORA-RELEASES.md) | Technical changes across Aurora versions |
| [Roadmap](docs/ROADMAP.md) | Planned work and unverified features |

### Project status

Aurora 17 includes source-level improvements to file management, theming and
native graphics. **Live theme switching, focused Terminal responsiveness and
cursor placement still require verification on the exact VM build used.**
Details of the recent runtime fixes are in
[core engineering](docs/CORE-ENGINEERING.md#aurora-17-runtime-maintenance-notes).

## License

OpenXP source code is licensed under [MIT](LICENSE). Third-party components,
including Limine, retain their respective licenses.
