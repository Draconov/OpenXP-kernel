# OpenXP

**An open-source, Windows XP-inspired operating system experiment.**

OpenXP is a small, original x86-64 kernel with a software-rendered desktop,
command interpreter, basic window manager, and experimental ELF64 application
support. It is written primarily in freestanding C and x86-64 assembly.

> [!WARNING]
> **Research prototype — not a secure or everyday operating system.**
> The kernel and desktop compile and have host-side tests, but a complete
> graphical boot, hardware input, and user-mode execution have **not been
> verified in QEMU**. Use a disposable virtual machine only.

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
| **Kernel crash or reboot** | Review the GitHub Actions QEMU smoke-test logs. A passing compile does not prove VirtualBox compatibility. |

For supported VM settings and optical-drive controls, see the
[VirtualBox user guide](https://docs.oracle.com/en/virtualization/virtualbox/7.2/user/create-vm.html).


</details>

## Explore the desktop

- **Start menu:** open Terminal, My Computer, or About OpenXP.
- **Windows:** focus, drag, minimize, or close the three built-in windows.
- **Terminal:** type commands with a PS/2 keyboard or through the COM1 serial console.
- **My Computer:** browse bundled read-only files and launch supported ELF64 examples.
- **Taskbar:** switch between windows and view elapsed uptime (not wall-clock time).

Try `HELP` in the terminal to see commands such as `DIR`, `CD`, `TYPE`, `MEM`,
`UPTIME`, `PS`, and `RUN BIN/HELLO`. Application output currently goes to COM1.

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
