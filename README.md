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
