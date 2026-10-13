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
make smoke-native   # Native GUI/RAM ABI probe in Ring 3
make smoke-window-ipc # Ring 3 window service and compositor bridge test
make smoke-shell      # Confirm the Ring 3 desktop/taskbar policy service starts
```

The trusted `bin/windowd` Ring 3 process now starts automatically on normal
boot, owns Start-menu/taskbar drawing commands, global keyboard shortcuts,
focus and title-bar drag policy, desktop icon layout, and IPC window requests.
Ring 3 submits separate background and foreground drawing layers; the kernel
validates commands and paints pixels. The legacy desktop is restored if the
service exits or stops refreshing its frame. **Final window rasterization and several older application UIs still execute
in Ring 0.** Calculator, Notepad, Calendar, Mines and Snake now prefer separately compiled
Ring 3 ELF programs from Start, falling back to legacy windows when a native
process cannot launch. Clock, System Monitor and About have experimental native
binaries that can be tried through `RUN BIN/NAME` without replacing their
more complete legacy Start-menu implementations. Paint, WordPad, Terminal, File Manager, Run and Settings now have separate
Ring 3 ELF executables and Start attempts to launch these first. If a native
process cannot launch, the previous Ring 0 implementation remains as fallback.
These replacements are not yet feature-equivalent, and the native launcher,
settings IPC, and input behavior still require VirtualBox verification. See [Ring 3 Shell](docs/RING3-SHELL.md),
[IPC](docs/IPC.md) and the [Roadmap](docs/ROADMAP.md). The new mode needs real
QEMU/VirtualBox testing before it should be considered reliable.

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

The default boot image is read-only. HOME files are writable in RAM but lost at reboot.
A separate, opt-in dedicated experimental IDE disk build is described in
[Storage](docs/STORAGE.md); it is not enabled in GitHub's standard ISO.
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

## Native application experiments

OpenXP also bundles separate Ring 3 ELF64 applications. Open **Run** or
**Terminal** and try `RUN BIN/CALCULATOR`, `RUN BIN/CLOCK`,
`RUN BIN/NOTEPAD`, or `RUN BIN/CANVAS`. They now use independent bounded
pixel surfaces instead of sharing the framebuffer or living inside the
kernel. The original legacy app windows remain for compatibility.
Actual concurrent Ring 3 GUI behavior must still be verified in a VM.
See [Native applications](docs/NATIVE-APPS.md) for the developer ABI.

**New Ring 3 application pilots:** From the existing Terminal use
`RUN BIN/PAINT`, `RUN BIN/WORDPAD`, `RUN BIN/TERMINAL`,
`RUN BIN/FILEMANAGER`, `RUN BIN/RUN`, or `RUN BIN/SETTINGS`.
The OXRD boot archive now supports 32 entries. The new file API supports
bounded HOME file operations and a restricted program launch queue; there
is still no general installed-application system or persistent filesystem.
The latest source adds live theme requests from native Settings to windowd,
boot-file copying into HOME, expanded native file commands, 8-color native Paint,
and cursor-based WordPad editing. HOME is RAM-backed, so changes vanish at
shutdown. These source changes have host tests, not verified guest behavior.
The new native apps are still not fully feature-equivalent to the legacy ones.

**Window-service experiment:** `windowd` starts automatically at normal boot.
Use `RUN BIN/WINCLIENT` to exercise IPC-managed native window creation,
movement, focus and closure. The kernel remains the final renderer and the
legacy desktop is the fallback. See [IPC](docs/IPC.md).

## Explore the desktop

- **Start menu:** browse fourteen built-in desktop windows in two columns.
- **Windows:** focus, drag, minimize, close, or switch between open tools.
- **Terminal:** type commands with a PS/2 keyboard or through the COM1 serial console.
- **My Computer:** browse boot files and temporary HOME documents; open saved texts and Paint images, and launch bundled ELF64 demos.
- **Notepad:** edit text (2,047 characters); save and reopen a temporary `HOME/NOTEPAD.TXT` file.
- **WordPad:** edit text (4,095 characters), adjust display, and save/reopen `HOME/WORDPAD.TXT` in RAM.
- **Calendar:** navigate Gregorian months and years; highlights the VM RTC date when readable (timezone not guaranteed).
- **Calculator:** basic nonnegative integer arithmetic, with keyboard and clickable buttons.
- **Paint:** draw with an eight-color pencil on a 160 x 100 canvas and save/reopen `HOME/PAINTING.OXP` in RAM.
- **System Monitor:** inspect uptime, available memory pages, and open windows.
- **Appearance Settings:** choose from three wallpapers (not retained after reboot).
- **Mines:** a 9 x 9 minefield with ten mines, first-click safety, flags and restart.
- **Clock:** stopwatch and one-to-sixty-minute countdown timer with pause, reset and alert.
- **Snake:** keyboard-controlled 20 x 14 grid game with food, scoring, pause and restart.
- **Run:** XP-style launcher for bundled desktop programs and supported RAMDISK ELF demos.
- **Taskbar:** switch between windows and view elapsed uptime (not wall-clock time).

**Keyboard shortcuts:** `Win` or `Ctrl+Esc` opens Start; use arrow keys and
`Enter` to choose an item. `Win+E` opens My Computer, `Win+R` opens Run,
`Win+T` opens Terminal, `Win+K` opens Clock, `Win+Y` opens Snake,
`Win+D` toggles the desktop, `Win+N` opens Notepad, `Win+C` opens Calculator,
`Win+P` opens Paint, `Win+M` opens System Monitor, `Win+S` opens Appearance,
`Win+G` opens Mines, `Win+W` opens WordPad, `Win+A` opens Calendar, `Alt+Tab` switches windows, and `Alt+F4` closes
the active window. In Terminal, use `↑`/`↓` for history, `←`/`→` and
`Home`/`End` to edit, `Ctrl+C` to clear the input line and `Ctrl+L` to clear
the output. Notepad accepts text, Enter for line breaks, Home/End, arrow keys, Backspace,
and Delete; `Ctrl+N` clears the text. In Notepad, WordPad and Paint, `Ctrl+S` saves and `Ctrl+O` opens their respective temporary HOME files. The built-in SAVE and OPEN buttons do the same. WordPad also supports `Ctrl+B` for bold display and the toolbar for text zoom. Calendar uses Left/Right for months, Up/Down for years and `T` to return to the VM date. In My Computer, use Enter to open a folder or file, and Backspace or the Up button to go back. Calculator uses digits,
`+`, `-`, `*`, `/`, Enter (equals), and `C` (clear). In Paint, press `1`-`8`
to choose a color or `X` to clear the image. In Mines, right-click (or hover
and press `F`) to flag a square, and press `N` to start a new game.
Appearance Settings accepts `1`-`3`. Clock uses Space to start/pause,
`R` to reset, `M` to change modes and `+`/`-` to adjust the countdown length.
Snake uses arrow keys, Space to pause and `N` to restart. Run accepts program
names such as `WORDPAD`, `WRITE`, `CALENDAR`, `CALC`, `SNAKE`, `CMD` or `BIN/HELLO`; it is not an
arbitrary Windows `.exe` launcher. These are OpenXP shortcuts,
not Windows compatibility features.

**New terminal commands:** `LS`, `CAT`, `CLEAR`, `PWD`, `TREE`, `HISTORY`,
`CALC 8 * 7`, `WHOAMI`, `HOSTNAME`, `UNAME`, `SYSINFO`, `APPS`, `COPY`,
`REN` and `DEL`, in addition
to `HELP`, `DIR`, `CD`, `TYPE`, `MEM`, `UPTIME`, `PS`, and `RUN BIN/HELLO`.
Use `HELP` to see the command list. Terminal can inspect temporary files with
`CD HOME`, `DIR`, and `TYPE NOTEPAD.TXT`/`TYPE WORDPAD.TXT`. Files are writable
through the desktop applications; the shell can also copy, rename and delete
existing `HOME` files (without overwriting destination files). All temporary files are
**lost when the VM shuts down or reboots**. Persistent storage and disk drivers
are still future work. Supported
legacy `SYS_WRITE` output goes to COM1; native GUI programs write to their window.

The desktop uses IRQ-driven PS/2 keyboard/mouse input when the legacy
controller permits it, with a fallback polling mode. Dragging windows repaints
only the affected framebuffer rectangle. Actual smoothness on VirtualBox and
other hardware still needs hands-on testing.

## Native applications (experimental)

OpenXP now builds **separate Ring 3 ELF64 executables** for a small native-app
pilot. They are stored in the read-only boot archive, not linked into the
kernel's desktop code. Unlike a Windows `.exe`, these use a restricted OpenXP
ELF64 ABI. The original kernel-hosted desktop programs are retained so existing
features do not disappear while the application platform matures.

| Native file in My Computer | Launch from Terminal or Run | Controls |
| --- | --- | --- |
| `BIN/CALCULATOR` | `RUN BIN/CALCULATOR` | Digits, `+ - * / =`, `C` clear, Esc exit |
| `BIN/CLOCK` | `RUN BIN/CLOCK` | Live uptime clock; Esc exit |
| `BIN/NOTEPAD` | `RUN BIN/NOTEPAD` | Type, Backspace, Enter; Ctrl+S save and Ctrl+O reopen a RAM note |

The native programs each have their own user memory and PID. Independent
worker tasks launch them without blocking the desktop, and the GUI communicates
with each program through checked system calls and a PID-owned event queue.
`PS` now lists live process slots and their states; `RUN` acknowledges the
launch request rather than claiming the program has already finished. Click the
native window to focus it; use its `X` button or `Alt+F4` to request exit.

**Current constraints:** up to **three native graphical windows** at a time,
with separate PIDs, checked RGB565 drawing surfaces and focused input queues
(host-tested; interactive VM verification outstanding). Native apps use
scheduler-backed `GUI_WAIT` instead of busy-polling; the new
`make smoke-native-wait` integration target checks the preemptible Ring 3 path.
The read-only boot archive and writable RAM files now share a simple VFS path
API. There are no general file handles, persistent default storage, installable
programs, or reusable widget toolkit yet. Existing WordPad, Paint, games,
window composition, and the desktop shell **still run in kernel mode**.
None of this implies Windows `.exe` compatibility or full process isolation.

For syscall definitions, native C sources, build instructions and migration
status see [Native applications](docs/NATIVE-APPS.md).

OpenXP does **not** support Windows programs or drivers.

## Native desktop shell

The boot-time Ring 3 `windowd` service controls the Start-menu and taskbar
layout and global focus/shortcut policy through bounded, kernel-validated
commands. It does not have unrestricted framebuffer access. Legacy applications
and window painting remain in Ring 0; the kernel restores its original shell
if the service stops responding. The new integration test is `make smoke-shell`.
See [Ring 3 Shell](docs/RING3-SHELL.md).

## Native window IPC

The same boot-time `windowd` service also accepts native application IPC clients.
`RUN BIN/WINCLIENT` demonstrates its window-management protocol, and
`make smoke-window-ipc` checks the diagnostic two-client path under QEMU.
See [Process IPC](docs/IPC.md) for security and verification limits.

## Documentation

| Guide | Contents |
| --- | --- |
| [Process IPC](docs/IPC.md) | Process-owned handles, message transport, Ring 3 server prototype |
| [Architecture](docs/ARCHITECTURE.md) | Kernel design, boot sequence, memory and scheduling |
| [Desktop and input](docs/DESKTOP.md) | Controls, rendering, shell and limitations |
| [Userspace](docs/USERSPACE.md) | ELF loader, process model and system calls |
| [Experimental storage](docs/STORAGE.md) | Dedicated IDE disk test and safety rules |
| [Roadmap](docs/ROADMAP.md) | Planned work, priorities and verification goals |
| [VFS foundation](docs/VFS.md) | Read-only boot files, writable HOME namespace, and limitations |

## License

OpenXP source code is licensed under [MIT](LICENSE). Limine and any other
third-party dependencies retain their own licenses.
