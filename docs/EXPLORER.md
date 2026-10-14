# Explorer, file types and desktop backgrounds

OpenXP includes an experimental **Ring 3 Explorer** (`bin/filemanager`), a
native Display Properties app (`bin/settings`), and an XP-inspired virtual
`C:\` directory layout. These are not Windows Explorer or Windows PE support.

## Explorer

Open **My Computer** from Start or `WIN+E`. Its compact 320×180 native window
provides folder shortcuts, Back and Up navigation, an address display, colored
file-type icons, selection highlighting and a five-action right-click menu.
Click an item to select it; click it again or press Enter to open it. Up/Down
scrolls the current list. The sidebar jumps to C:, My Documents, System Apps,
Wallpapers, Program Files, or System32.

The right-click menu offers **Open, Copy to HOME, Rename, Delete, Properties**.
Copy, rename and delete are restricted by the VFS: bundled system files remain
read-only. Large files exceeding the RAM filesystem's size limit cannot be
copied into HOME. `HOME` storage is ephemeral unless a separate experimental
persistent-storage mode is used.

## File types and associations

| Extension | Display | Open action |
| --- | --- | --- |
| Folder | Folder icon | Navigate into directory |
| `.exe` | Application icon | Launch allowlisted **OpenXP ELF64** executable alias |
| `.txt`, `.log`, `.md` | Text icon | Open with native WordPad |
| `.cfg`, `.ini` | Settings/document icon | Open with native WordPad |
| `.oxp` | Drawing icon | Open with native Paint |
| `.bmp` inside `Windows\Web\Wallpaper` | Picture icon | Apply as desktop background |
| Other | Generic icon | Show properties; no automatic execution |

For a text or Paint document, Explorer writes a one-use RAM request under
`HOME` and launches the associated program. WordPad/Paint read the requested
file rather than blindly opening their default document. Saving an edited
read-only system document writes a **HOME** copy; it never overwrites system
files. Application launches still pass through the kernel's fixed allowlist.

## Add wallpaper images

Put **uncompressed 24-bit BMP** files under:

```text
assets/wallpapers/
```

Use simple lowercase alphanumeric names, hyphens or underscores, and `.bmp`.
Images must be 1–1024 pixels wide, 1–768 pixels tall, and comply with the
4 MiB boot-archive capacity and 64-entry limit. BMPs are validated during ISO
build, then exposed read-only in:

```text
C:\Windows\Web\Wallpaper\
```

Open **Settings → Display Properties** and click one of the backgrounds in the
list. The left/right arrows page through more names. Alternatively, double-
click a listed `.bmp` in Explorer. The image is decoded in Ring 3, downsampled
to a 160×120 RGB565 cache, and scaled across the desktop background by the
userspace compositor. Desktop icons remain on top of the wallpaper. The three
previous color schemes remain available.

Wallpaper selection is stored in a small **RAM preference** and restored if
`windowd` restarts, but disappears across a normal reboot. Choosing a color
scheme disables the previously selected image. Files are bundled into the ISO
at build time: copying BMP files into the ISO at runtime is not supported.

## Limits and verification

- The Explorer window is still small and lacks drag-and-drop, thumbnails,
  arbitrary directory creation, user-configurable file associations, and a
  generic persistent disk filesystem.
- Applications cannot execute arbitrary Windows `.exe` programs. Named system
  `.exe` entries resolve to trusted OpenXP ELF64 files only.
- The native wall painter is software-only; no GPU hardware acceleration,
  vblank synchronization or unrestricted framebuffer access is introduced.
- Host tests cover BMP header rejection, safe resampling, file classification,
  VFS slice reads and icons surviving image composition. **VirtualBox/QEMU
  interaction and visual correctness remain to be verified.**
