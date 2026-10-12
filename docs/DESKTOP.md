# Desktop and input

OpenXP includes a small XP-inspired desktop rendered directly into a
32-bit RGB framebuffer. It is a **kernel-hosted development interface**, not
a protected userspace windowing system.

![Desktop preview](desktop-preview.png)

*Host-side renderer output; not a QEMU screenshot.*

## What you can do

| Feature | Interaction |
| --- | --- |
| Start menu | Open Terminal, My Computer and About OpenXP |
| Windows | Click to focus; drag title bars; minimize or close |
| Taskbar | Reopen or switch built-in windows |
| Terminal | Type commands, use Enter and Backspace |
| Keyboard | Tab cycles visible windows |
| File browser | Inspect bundled read-only files and run supported ELF64 examples |
| Clock | Displays uptime, not date or local time |

Mouse and keyboard input come from **legacy PS/2 polling**. The serial COM1
console also feeds characters into the command interpreter. In QEMU, the normal
`make run` target shows a graphical display and uses the terminal for serial output.

## Terminal commands

| Command | Action |
| --- | --- |
| `HELP` | Show available commands |
| `VER` | Show build information |
| `CLS` | Clear terminal output |
| `ECHO hello` | Print text |
| `DIR` | List entries in the current read-only folder |
| `CD BIN`, `CD DOCS`, `CD ..` | Change the limited directory view |
| `TYPE DOCS/README.TXT` | Read a bundled text file |
| `MEM` | Show free physical pages |
| `UPTIME` | Show PIT ticks |
| `PS` | Show an experimental process-status placeholder |
| `RUN BIN/HELLO` | Run a supported sample ELF64 application |
| `EXIT` | Minimize Terminal |

`RUN` is synchronous and uses the experimental process launcher. Program output
is sent to the **serial console**, not piped back into the graphical Terminal.
The desktop may stop responding until that program exits.

## How the desktop works

```text
PS/2 polling ─┐
COM1 input ───┴─> input events ─> shell / window actions
                                  │
                           software renderer
                                  │
                            RGB framebuffer
```

The implementation spans `kernel/input/`, `kernel/video/`,
`kernel/gui/desktop.c` and `kernel/shell/`. It repaints the framebuffer instead
of using GPU acceleration or a compositor. A fixed table represents three
built-in windows with focus, position and visibility state.

The i8042 keyboard and mouse are polled together; their hardware interrupts
remain masked. The mouse decoder expects standard three-byte PS/2 packets.
Only supported framebuffer formats and legacy virtual hardware are targeted.

## Verify the implementation

```bash
make test-desktop    # Shell, input, drawing and window-manager host tests
make kernel          # Compile the kernel
make run             # Attempt an interactive graphical QEMU boot
make smoke           # Headless serial-marker boot check
```

Host tests validate isolated components but **do not confirm working guest
input, drawing or application launches**. The desktop runs in Ring 0 and must
not be treated as secure. Verification goals and other planned changes are in
the [Roadmap](ROADMAP.md).
