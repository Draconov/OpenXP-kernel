# Files and folders

OpenXP presents an **XP-inspired virtual C: drive**. It is not FAT, NTFS, or a
Windows installation. Folder names are backed by VFS directory metadata, the
bundled read-only OXRD boot archive, and a tiny writable RAM store. Some system
folders are empty placeholders for future drivers and resources.

## Directory structure

```text
C:\
├── Windows\
│   ├── System32\         native OpenXP application aliases (*.exe)
│   │   └── Drivers\       reserved for future drivers
│   ├── Web\Wallpaper\    included BMP images
│   ├── Fonts\             reserved
│   ├── Resources\         reserved
│   └── Temp\              reserved
├── Program Files\OpenXP\ native OpenXP applications (*.exe)
├── System Apps\          native app shortcuts presented as files (*.exe)
├── Documents and Settings\
│   └── User\My Documents\  aliases the writable HOME store
├── My Documents\         writable HOME aliases
├── My Pictures\, My Music\, My Videos\  reserved
├── Home\                 writable RAM files, lost at reboot
├── Bin\                  immutable boot ELF64 executables
├── Docs\                 bundled read-only documentation
└── Temp\, Recycler\      reserved
```

### Are the `.exe` files Windows programs?

**No.** `C:\Windows\System32\notepad.exe` and `C:\System Apps\notepad.exe`
point to the *same* bundled `bin/notepad` **OpenXP ELF64** application. The
alias is a file in the VFS; it is checked against an explicit allowlist before
launch. It does not copy executable bytes or load Windows PE programs. The
catalog is defined in `kernel/fs/app_catalog.h`.

The native File Manager opens folders with **Enter** or a second click on an
already selected folder; **Backspace** navigates upward. Select an `.exe`
entry and press **Enter** to launch it. Copy, delete and rename remain limited
to the temporary writable HOME area, and copying an application to HOME does
not make it executable.

The native Terminal supports `CD`, `DIR`, `PWD`, `CAT` and `RUN`. For instance:

```text
CD "Windows"               # In the native terminal use: CD WINDOWS
CD SYSTEM32
DIR
RUN C:\Windows\System32\notepad.exe
```

The simple terminal's path input is case-insensitive for commands but paths in
the actual VFS are stored as lowercase ASCII. It does not implement arbitrary
Windows shell quoting, wildcard expansion or Windows application compatibility.

## Add more wallpaper images

Place **uncompressed 24-bit BMP** images into:

```text
assets/wallpapers/
```

The `make iso` / `make kernel` build automatically includes every `.bmp` file
found there in the read-only boot archive under:

```text
C:\Windows\Web\Wallpaper\
```

Three original sample images (`azure.bmp`, `meadow.bmp`, `twilight.bmp`) are
included. Supported dimensions are **1–1024 pixels wide** and **1–768 pixels
high**, with a standard 40-byte BMP information header and no compression.
Use short ASCII filenames; the underlying OXRD path cannot exceed 47 bytes.
The boot archive currently supports up to 64 files and 4 MiB total.

**Current limitation:** Wallpapers are bundled, listed, and retrievable as
ordinary read-only files, but **BMP wallpaper selection/decoding is not wired
into the Ring 3 desktop yet**. Appearance Settings still controls its existing
procedural themes. Adding an image does not automatically apply it as the
wallpaper. It will appear in the system folder after rebuilding the ISO.

## Persistence and permissions

- `Home` and `My Documents` share the same temporary RAM storage. RAM files
  disappear when OpenXP reboots.
- `Windows`, `System Apps`, `Program Files`, and the bundled wallpaper files are
  read-only in the default ISO.
- Empty placeholder directories aren't backed by a real disk and cannot yet
  contain newly installed applications.
- No FAT/NTFS driver, program installer, dynamic library loader, or general
  directory-creation API is provided.
- Experimental IDE snapshot storage is documented in [Storage](STORAGE.md).

See [Roadmap](ROADMAP.md) for future work: wallpaper selection, writable disk
folders, drag-and-drop, icon associations and installable applications.
