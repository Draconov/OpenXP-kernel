# OpenXP

**An open-source, Windows XP-inspired operating system experiment.**

OpenXP is a small, original x86-64 kernel with a software-rendered desktop,
command interpreter, basic window manager, and experimental ELF64 application
support. It is written primarily in freestanding C and x86-64 assembly.

> [!WARNING]
> **Research prototype — not a secure or everyday operating system.**
> OpenXP has reached the graphical desktop in a VirtualBox test, but hardware
> input, security boundaries, and user-mode applications remain experimental.
> Host tests do not replace a complete QEMU/VirtualBox integration test.
> Use a disposable virtual machine only.

![OpenXP desktop preview](docs/desktop-preview.png)

*Rendered by the project's desktop code in a host test, not captured from a
running virtual machine.*

## Get started

On Ubuntu or Windows with WSL2 (Ubuntu), open a terminal in the project root:

```bash
sudo apt update
sudo apt install -y clang lld make git python3 xorriso qemu-system-x86

make test-unit       # Host-side subsystem tests
make test-sanitize   # AddressSanitizer and UBSan checks
make kernel          # Creates build/openxp.elf
make iso             # Creates build/openxp.iso
make run             # Boots the ISO in graphical QEMU
```

The ISO build fetches the [Limine](https://github.com/limine-bootloader/limine)
bootloader on first use, so an internet connection is needed. An ISO exists
only after the build succeeds. If the guest fails to boot, check the QEMU
serial output and the test logs rather than assuming compilation proves bootability.

For headless boot checks:

```bash
make smoke          # Desktop boot markers over serial
make smoke-deep     # Additional kernel and user-mode diagnostics
```

GitHub Actions runs the build and tests and uploads available kernel/ISO build
artifacts. An uploaded artifact is **not** a guarantee that all tests passed.

<details>
<summary><strong>Run OpenXP in VirtualBox — setup guide</strong></summary>

OpenXP boots as a **live ISO**. You can try it in
[Oracle VirtualBox](https://www.virtualbox.org/) without installing an OS or
creating a virtual hard disk. Its virtual hardware support is experimental;
**QEMU remains the primary testing environment**.

### 1. Get the ISO

Build it with `make iso` (see above), or open your repository's **Actions** tab,
select a **Kernel Build** workflow run, and download its build artifact from
**Artifacts**. Extract the artifact ZIP and locate `openxp.iso`.

> [!NOTE]
> An uploaded ISO does not necessarily mean the full CI test suite passed.
> Check the workflow results before reporting a successful boot.

### 2. Create a virtual machine

In VirtualBox Manager, choose **New** and use these starting settings:

| Setting | Recommended value |
| --- | --- |
| Name | `OpenXP` |
| OS type | **Other** → **Other/Unknown (64-bit)** (wording may vary) |
| Memory | **512 MB** (1 GB is fine) |
| Processors | **1 CPU** (the kernel currently targets a single CPU) |
| Firmware | **BIOS** (disable **EFI** initially) |
| Chipset | **PIIX3** |
| Pointing device | **PS/2 Mouse**, not USB Tablet |
| Graphics | **VBoxSVGA**, **32 MB** video memory, **3D acceleration off** |
| Storage | **No virtual hard disk required** |

If the new-VM wizard asks to install an OS unattended, **skip unattended
installation**. This ISO starts OpenXP directly; it is not an installer.

### 3. Attach the ISO and boot

1. Open the VM's **Settings → Storage**.
2. Select the virtual optical drive (add one under an IDE controller if needed).
3. Choose **Select a disk file** and open `openxp.iso`.
4. In **Settings → System → Motherboard**, make sure **Optical** is first in
   the BIOS boot order.
5. Click **Start**. If boot succeeds, OpenXP should display its experimental
   desktop, taskbar, and Start menu.

The boot image is read-only and does not save files or settings across reboots.
USB input, Guest Additions, networking, shared folders, and disk installation
are **not supported** by the current prototype.

### Troubleshooting

| Symptom | What to try |
| --- | --- |
| **No bootable medium** | Confirm `openxp.iso` is attached to the optical drive and **Optical** comes first in the boot order. |
| **UEFI shell or firmware screen** | Disable **EFI** and retry BIOS boot. |
| **Black screen or display problems** | Disable 3D acceleration, use **VBoxSVGA**, and retry. Compare with `make run` in QEMU. |
| **Keyboard or mouse does not respond** | Set the pointing device to **PS/2 Mouse** and click inside the VM to capture input. VirtualBox input compatibility is still experimental. |
| **Windows key doesn't open Start** | Click inside the VM so the keyboard is captured. The host or VirtualBox may intercept some shortcuts. |
| **Kernel crash or reboot** | Review the GitHub Actions QEMU smoke-test logs. A passing compile does not prove VirtualBox compatibility. |

For supported VM settings and optical-drive controls, see the
[VirtualBox user guide](https://docs.oracle.com/en/virtualization/virtualbox/7.2/user/create-vm.html).


</details>

## Explore the desktop

- **Start menu:** browse nine built-in desktop windows in two columns.
- **Windows:** focus, drag, minimize, close, or switch between open tools.
- **Terminal:** type commands with a PS/2 keyboard or through the COM1 serial console.
- **My Computer:** browse bundled read-only files and launch supported ELF64 examples.
- **Notepad:** edit a RAM-only scratch document (up to 2,047 characters). No saving yet.
- **Calculator:** basic nonnegative integer arithmetic, with keyboard and clickable buttons.
- **Paint:** draw with an eight-color pencil on a 160 x 100 RAM-only canvas.
- **System Monitor:** inspect uptime, available memory pages, and open windows.
- **Appearance Settings:** choose from three wallpapers (not retained after reboot).
- **Mines:** a 9 x 9 minefield with ten mines, first-click safety, flags and restart.
- **Taskbar:** switch between windows and view elapsed uptime (not wall-clock time).

**Keyboard shortcuts:** `Win` or `Ctrl+Esc` opens Start; use arrow keys and
`Enter` to choose an item. `Win+E` opens My Computer, `Win+R` opens Terminal,
`Win+D` toggles the desktop, `Win+N` opens Notepad, `Win+C` opens Calculator,
`Win+P` opens Paint, `Win+M` opens System Monitor, `Win+S` opens Appearance,
`Win+G` opens Mines, `Alt+Tab` switches windows, and `Alt+F4` closes
the active window. In Terminal, use `↑`/`↓` for history, `←`/`→` and
`Home`/`End` to edit, `Ctrl+C` to clear the input line and `Ctrl+L` to clear
the output. Notepad accepts text, Enter for line breaks, Home/End, arrow keys, Backspace,
and Delete; `Ctrl+N` clears its in-memory document. Calculator uses digits,
`+`, `-`, `*`, `/`, Enter (equals), and `C` (clear). In Paint, press `1`-`8`
to choose a color or `X` to clear the image. In Mines, right-click (or hover
and press `F`) to flag a square, and press `N` to start a new game.
Appearance Settings accepts `1`-`3`. These are OpenXP shortcuts,
not Windows compatibility features.

**New terminal commands:** `LS`, `CAT`, `CLEAR`, `PWD`, `TREE`, `HISTORY`,
`CALC 8 * 7`, `WHOAMI`, `HOSTNAME`, `UNAME`, `SYSINFO` and `APPS`, in addition
to `HELP`, `DIR`, `CD`, `TYPE`, `MEM`, `UPTIME`, `PS`, and `RUN BIN/HELLO`.
Use `HELP` to see the command list. File access is read-only, and supported
user-program output currently goes to COM1.

The desktop uses IRQ-driven PS/2 keyboard/mouse input when the legacy
controller permits it, with a fallback polling mode. Dragging windows repaints
only the affected framebuffer rectangle. Actual smoothness on VirtualBox and
other hardware still needs hands-on testing.

The desktop and its command interpreter run in **kernel mode**. They are not
isolated graphical applications; ELF64 user-mode execution is experimental.
OpenXP does **not** support Windows programs or drivers.

## Documentation

| Guide | Contents |
| --- | --- |
| [Architecture](docs/ARCHITECTURE.md) | Kernel design, boot sequence, memory and scheduling |
| [Desktop and input](docs/DESKTOP.md) | Controls, rendering, shell and limitations |
| [Userspace](docs/USERSPACE.md) | ELF loader, process model and system calls |
| [Roadmap](docs/ROADMAP.md) | Planned work, priorities and verification goals |

## License

OpenXP source code is licensed under [MIT](LICENSE). Limine and any other
third-party dependencies retain their own licenses.
