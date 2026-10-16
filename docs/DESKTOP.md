# Desktop and input

OpenXP includes a small XP-inspired desktop rendered directly into a
32-bit RGB framebuffer. It is a **kernel-hosted development interface**, not
a protected userspace windowing system.

![Desktop preview](desktop-preview.png)

*Host-side renderer output; not a QEMU screenshot.*

## What you can do

| Feature | Interaction |
| --- | --- |
| Start menu | Two columns of built-in programs, with mouse or keyboard selection |
| Windows | Click to focus; drag title bars; minimize, maximize/restore or close |
| Taskbar | Reopen or switch built-in windows |
| Terminal | Type commands, use Enter and Backspace |
| Keyboard | Windows key opens Start; arrow keys and Enter select its items |
| File browser | XP-inspired Explorer with folders, file-type icons, associated Open actions, context menu and temporary HOME operations |
| Notepad | Edit text, Ctrl+N for new, Ctrl+S/Ctrl+O to save/open a temporary HOME file |
| WordPad | Editable text (4,095 chars), bold/zoom display, temporary HOME save/open |
| Calendar | Gregorian month grid, date highlighting from guest CMOS RTC, keyboard and pointer controls |
| Calculator | Four-operation integer arithmetic; buttons and keyboard |
| Paint | Eight-color 160 x 100 drawing surface with temporary HOME save/open |
| System Monitor | Uptime, free 4 KiB pages and open window status (not a process list) |
| Appearance | Three color schemes plus bundled BMP wallpapers, reset on reboot |
| Mines | 9 x 9, ten mines, guaranteed safe first click; right-click flags |
| Clock & Stopwatch | Stopwatch and countdown timer driven by the monotonic 100 Hz PIT tick count |
| Snake | Grid-based arcade game; use arrow keys, Space to pause, N to restart |
| Run | Launch built-in program names, or try a supported bundled `bin/` ELF |

Mouse and keyboard input use **legacy PS/2 IRQ1 and IRQ12** when i8042
initialization succeeds. The interrupt handlers queue raw bytes; the foreground
loop handles decoding and GUI events. If the controller refuses initialization,
the system falls back to PS/2 polling at the PIT interval. The serial COM1
console also feeds characters into the command interpreter. In QEMU, the normal
`make run` target shows a graphical display and uses the terminal for serial output.

## Keyboard shortcuts

| Keys | Action |
| --- | --- |
| `Win` or `Ctrl+Esc` | Toggle the Start menu |
| `↑` / `↓`, `Enter`, `Esc` | Navigate, launch, or dismiss the Start menu |
| `Win+E` | Open My Computer |
| `Win+R` | Open the Run program launcher |
| `Win+T` | Open Terminal |
| `Win+K` | Open Clock |
| `Win+Y` | Open Snake |
| `Win+N` | Open Notepad |
| `Win+C` | Open Calculator |
| `Win+P` | Open Paint |
| `Win+M` | Open System Monitor |
| `Win+S` | Open Appearance Settings |
| `Win+G` | Open Mines |
| `Win+W` | Open WordPad |
| `Win+A` | Open Calendar |
| `1`-`8`, `X` in Paint | Select a palette color, clear the canvas |
| `1`-`3` in Appearance | Select a wallpaper preset |
| Right-click or hover + `F` in Mines | Flag/unflag a hidden cell |
| `N` in Mines | Reset the board |
| Space / `R` / `M` in Clock | Start/pause, reset, toggle stopwatch/timer |
| `+` / `-` in Clock | Set countdown length from 1 to 60 minutes |
| Arrow keys / Space / `N` in Snake | Steer, pause/resume, restart |
| `Enter`, `Esc`, Backspace in Run | Launch, cancel, edit the entered command |
| `Ctrl+N` in Notepad | Clear the in-memory text (no undo) |
| `Ctrl+S` / `Ctrl+O` in Notepad, WordPad, Paint | Save/open that program's temporary HOME file |
| `Win+D` | Show desktop; press again to restore windows |
| `Alt+Tab` | Switch between open windows |
| `Alt+F4` | Close the focused window |
| `Tab` | Cycle window focus (basic legacy behavior) |
| `↑` / `↓` in Terminal | Browse command history |
| `←` / `→`, `Home` / `End` | Edit the Terminal input line |
| `Delete`, `Backspace` | Remove input characters |
| `Ctrl+C` in Terminal | Clear the current input line (not clipboard copying) |
| `Ctrl+L` in Terminal | Clear terminal scrollback |
| `↑` / `↓`, `Enter`, Backspace in My Computer | Navigate and open files or folders, go up a directory |
| `Ctrl+B`, `Ctrl+N` in WordPad | Toggle bold display; start a blank RAM document |
| `←` / `→`, `↑` / `↓`, `T` in Calendar | Previous/next month, next/previous year, return to VM date |

Notepad holds up to 2,047 characters; `Ctrl+S` writes a RAM filesystem copy
and `Ctrl+O` reopens it. Its saved file disappears when the guest shuts down. Calculator handles
nonnegative integers and shows an error on divide-by-zero, negative answers,
or overflow. Paint images can be saved temporarily in HOME, but not exported to disk.
Appearance choices are temporary as well. Mines generates ten mines after the first click
and protects that first cell. Clock uses **uptime** rather than wall-clock
or calendar time; its countdown does not play audio. Snake pauses its movement
while another program is focused. Run recognizes selected built-in aliases such
as `CMD`, `EXPLORER`, `CALC`, `CLOCK`, `SNAKE`, `NOTEPAD`, plus bundled paths
such as `BIN/HELLO`; Windows executables are not supported. All these desktop
tools run in Ring 0, **not** in separate protected processes.

WordPad holds up to 4,095 ASCII characters; toolbar bold and zoom are document-wide display settings rather than rich-text formatting. Opening a text file from My Computer copies its contents into RAM; the original read-only archive is never modified. Calendar reads the PC CMOS RTC only to select and highlight a date; UTC/local timezone interpretation depends on VM configuration, and date changes during an active session are not currently polled. Calendar does not store appointments.

The keyboard decoder currently assumes a US set-1 PS/2 layout. These shortcuts
work only when the VM passes the keys to OpenXP. The Windows key might be
intercepted by host operating-system or VM keyboard settings.

## Terminal commands

| Command | Action |
| --- | --- |
| `HELP` | List command names |
| `VER`, `UNAME` | Show basic kernel identification |
| `CLS`, `CLEAR` | Clear Terminal output |
| `ECHO hello` | Print text |
| `DIR`, `LS` | List boot and temporary RAM files |
| `CD BIN`, `CD DOCS`, `CD HOME`, `CD ..` | Change between supported virtual folders |
| `PWD` | Show current virtual directory |
| `TREE` | Show the available virtual-folder structure |
| `TYPE DOCS/README.TXT`, `CAT DOCS/README.TXT` | Read bundled text |
| `MEM`, `FREE` | Show free physical page count |
| `UPTIME` | Show PIT tick count |
| `PS` | Show the experimental process-status placeholder |
| `HISTORY` | Display recent Terminal commands (up to eight) |
| `CALC 8 * 7` | Built-in basic integer calculator (`+`, `-`, `*`, `/`) |
| `WHOAMI`, `HOSTNAME` | Display prototype user and host identity |
| `SYSINFO` | Show the CPU target, framebuffer and current limitations |
| `APPS` | Show built-in tools and instructions for running ELF samples |
| `RUN BIN/HELLO` | Attempt to launch a supported ELF64 sample application |
| `EXIT` | Minimize Terminal |

`RUN` is synchronous and uses the experimental process launcher. Program output
is sent to the **serial console**, not piped back into the graphical Terminal.
The desktop may stop responding until that program exits.

## How the desktop works

```text
PS/2 IRQ or polling fallback ─> decoded input ─> desktop and applications
COM1 serial input ───────────────────────┘              │
                                                  software renderer
                                                        │
                                                   RGB framebuffer
```

The desktop uses a software framebuffer, a cursor underlay and clipped redraws
for window dragging. Paint also repaints just the rectangle touched by its
brush; large changes still redraw more of the interface. The input subsystem
buffers raw bytes in its IRQ handlers and processes them in the foreground;
PS/2 polling is available as a fallback. The window manager currently handles
fourteen fixed built-in windows.

The implementation lives in `kernel/input/`, `kernel/video/`,
`kernel/gui/desktop.c`, and `kernel/shell/`. It has no GPU acceleration,
writable persistent filesystem or graphics process isolation.

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

## Native application window

The Ring 3 application pilot supports up to three separate native text windows
on top of the existing desktop. Launch `RUN BIN/CALCULATOR`, `RUN BIN/CLOCK` or
`RUN BIN/NOTEPAD` from Terminal or Run, or select the program in My Computer.
Its output arrives through a checked syscall and input is queued for the app.
Native Notepad uses Ctrl+S and Ctrl+O for the temporary
`HOME/NATIVE-NOTE.TXT` file. Separate native windows have individual PID-checked
event queues. Focused keyboard input and client clicks are routed to the
respective owner. Concurrent runtime behavior still requires VirtualBox/QEMU
verification. Built-in graphical tools are still kernel-hosted. See [Native applications](NATIVE-APPS.md).

## Temporary file operations

The kernel-hosted Terminal provides `COPY HOME/A.TXT HOME/B.TXT`,
`REN HOME/B.TXT HOME/C.TXT` and `DEL HOME/C.TXT`. These commands work only
with RAM-backed `HOME` files, never the read-only boot archive or any disk.
Copy and rename reject existing destination names and invalid paths. No
folder creation, Save As dialog or persistent storage is available yet.

## Experimental process-owned graphics

`RUN BIN/CANVAS` launches a separate native process that renders a bounded
RGB565 surface. Native Calculator, Clock and Notepad also use the same batched
pixel/bitmap-glyph ABI. The Ring 0 desktop currently composes these surfaces
into the native window slots. These native programs and their mouse/keyboard
handling still require end-to-end VM verification. The older built-in tools
remain available and are not yet migrated out of Ring 0.

## Native application migration

Calendar, Mines, Snake, System Monitor and About join the previously bundled
Calculator, Clock, Notepad and Canvas as independent ELF64 applications.
Start and shortcuts prefer the native Calendar, Mines and Snake alongside
Calculator and Notepad; the other standalone demos remain opt-in via Run or
Terminal until their missing legacy features are ported. A full three-window
native broker triggers a legacy fallback instead of silently failing.

- `RUN BIN/CALENDAR`: arrows change month/year, click selects a day, T returns
  to today's RTC date if available.
- `RUN BIN/MINES`: left click reveals, right click flags, N restarts.
- `RUN BIN/SNAKE`: arrow keys steer, Space pauses/starts, N restarts.
- `RUN BIN/MONITOR`: read-only uptime and available physical page count.
- `RUN BIN/ABOUT`: informational window.

These windows now have a bounded 640×360 RGB565 backing store and a logical
canvas that expands with their size instead of always stretching 320×180. Native
app drawing and pointer hit areas reflow after the `OXP_RESIZE` event. There
is still no GPU acceleration. Native graphics and input require VM testing.

## Native migration update

The Start menu now attempts Ring 3 versions of Paint, WordPad, Terminal,
File Manager, Run and Appearance Settings; Ring 0 equivalents are retained as
fallbacks while guest verification and feature parity are incomplete.

- **Native Settings:** select one of three schemes for a live `windowd`
  wallpaper/taskbar change. Save writes `home/appearance.cfg` to RAM; values
  disappear when the VM powers down unless experimental storage is enabled.
- **Native File Manager:** `C` copies a selected file to HOME (using `.copy`
  for HOME-to-HOME copies), `R` starts a new-filename prompt on HOME files,
  `D` deletes a HOME file, and `O` or Enter previews/opens.
- **Native Terminal:** `LS`, `CAT`, `RUN`, `MEM`, `UPTIME`, `COPY`, `REN`,
  `DEL`, `WRITE` and `TOUCH`. `COPY docs/readme.txt home/readme.txt` is a
  useful example. Commands and filenames are intentionally length-limited.
- **Native Paint:** 64x40 pixels with eight colors and OXP2 save data; the
  previous OXP1 drawing format is imported. Drawing is still click-based.
- **Native WordPad:** limited cursor insertion/deletion and whole-document
  zoom/bold; not a fully featured rich-text editor.

Final pixel rendering and legacy window controllers still run in Ring 0.
Moving those out requires a separately tested userspace compositor and full
application parity; the fallback must remain available until then.

## XP-style file browsing

The native File Manager now shows **immediate children** of `C:\` folders,
instead of a flat list of every boot file. Use Enter or click an already
selected folder to open it, Backspace or the Up control to return to its parent,
and Enter on supported `.exe` entries to request an OpenXP Ring 3 launch.

The built-in system folders and wallpaper asset path are explained in
[Filesystem and wallpapers](FILESYSTEM.md). The native Terminal supports
`CD`/`DIR` navigation, and Run accepts supported app names and `.exe` aliases.

For current native file associations, Explorer controls, and custom image requirements, see [Explorer and wallpapers](EXPLORER.md).

## Window sizing

Every legacy and native application window has three title-bar controls: **Minimize**,
**Maximize/Restore** and **Close**, arranged left-to-right as in Windows XP.
Maximizing uses the screen area above the taskbar. Click the middle button again
to return to the saved window dimensions and location. Minimized maximized
windows remain maximized when restored through the taskbar.

The Ring 3 desktop also supports **Win+Up** to maximize and **Win+Down** to
restore (or minimize when already in the normal state). The native SDK now
reports the logical canvas dimensions and emits resize events whenever they
change; the principal native applications reflow their content and click targets.
The recovery desktop keeps the same title-bar controls.
