# Files And Storage

Consolidated from the Aurora 17 source tree. All original technical instructions are retained below, grouped by topic.

## Sections

- [FILESYSTEM](#filesystem)
- [VFS](#vfs)
- [STORAGE](#storage)
- [EXPLORER](#explorer)

---

## FILESYSTEM

## Files and folders

OpenXP presents an **XP-inspired virtual C: drive**. It is not FAT, NTFS, or a
Windows installation. Folder names are backed by VFS directory metadata, the
bundled read-only OXRD boot archive, and a bounded writable RAM store. Some system
folders are empty placeholders for future drivers and resources.

### Directory structure

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

#### Are the `.exe` files Windows programs?

**No.** `C:\Windows\System32\notepad.exe` and `C:\System Apps\notepad.exe`
point to the *same* bundled `bin/notepad` **OpenXP ELF64** application. The
alias is a file in the VFS; it is checked against an explicit allowlist before
launch. It does not copy executable bytes or load Windows PE programs. The
catalog is defined in `kernel/fs/app_catalog.h`.

The native File Manager opens folders with **Enter** or a second click on an
already selected folder; **Backspace** navigates upward. Select an `.exe`
entry and press **Enter** to launch it. Copy, delete and rename remain limited
to the writable HOME area, and copying an application to HOME does
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

### Add more wallpaper images

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

### Persistence and permissions

- `Home` and `My Documents` share the same temporary RAM storage. RAM files
  disappear when OpenXP reboots unless experimental persistence is enabled.
- `Windows`, `System Apps`, `Program Files`, and the bundled wallpaper files are
  read-only in the default ISO.
- Empty placeholder directories aren't backed by a real disk and cannot yet
  contain newly installed applications.
- User-created nested folders, folder rename/move, file copy and empty-folder removal
  are supported under HOME and its Documents aliases. Recursive deletion and
  directory-tree copying are not supported.
- Up to 48 RAMFS records (including directory markers) can each store up to
  16 KiB. Full-capacity snapshots require an explicitly created OXPDISK2 disk.
- No FAT/NTFS driver, program installer or dynamic library loader is provided.
- Experimental IDE snapshot storage is documented in [Storage](FILES-AND-STORAGE.md#storage).

See [Roadmap](ROADMAP.md) for future work: wallpaper selection, writable disk
folders, drag-and-drop, icon associations and installable applications.

---

## VFS

## Virtual filesystem foundation

OpenXP currently has a deliberately small, single-CPU, path-based VFS bridge
(`kernel/fs/vfs.c`). It unifies **read-only boot files** and **temporary HOME
files** for the kernel-hosted terminal and the native note-file syscall.

| Namespace | Implementation | Writes | Survives reboot |
| --- | --- | --- | --- |
| `bin/`, `docs/` | CRC-validated `OXRD` initrd | No | Embedded in ISO |
| `home/` | Bounded RAMFS | Yes | No, by default |

API: `vfs_count`, `vfs_name_at`, `vfs_kind`, `vfs_read`, `vfs_write`,
`vfs_copy`, `vfs_rename`, and `vfs_remove`. This abstraction allows the
terminal and file-manager callbacks to stop depending on multiple filesystem
implementations directly.

All writes, copies, renames, and deletes require `home/` paths and still
pass the RAMFS path validator. Boot files cannot be overwritten or deleted.
The native note syscall remains restricted to `home/native-note.txt`: the VFS
change does **not** give Ring 3 applications unrestricted filename access.

### Important limitations

- There are no open file handles, seek operations, true directories, ACLs,
  per-process file capabilities, or general-purpose mounts.
- There is no disk filesystem. An optional small IDE-backed snapshot can
  restore RAMFS files, but requires a deliberately formatted disposable
  virtual disk, separate build configuration, and further VM testing.
- Metadata and RAMFS are single-CPU structures, not SMP-safe.
- Do not attach an existing physical or virtual system drive for testing.

### Verify

```sh
make test-unit         # Includes namespace and permission boundary tests
make test-sanitize
make kernel
make kernel-storage    # Compile-only check; does NOT verify disk hardware
```

See [experimental storage](FILES-AND-STORAGE.md#storage) and the [roadmap](ROADMAP.md) for the
remaining filesystem and reliability work.

---

## STORAGE

## Experimental persistent storage

OpenXP normally uses an **in-memory HOME folder**, which disappears on reboot.
A separate opt-in IDE prototype can save a bounded snapshot to a dedicated
virtual disk. It is **not verified in QEMU or VirtualBox** and is **not safe for
important data**. It does not provide FAT, NTFS, a full filesystem with directory handles,
multi-user permissions or general disk access.

### Why it is disabled by default

The ATA PIO backend talks only to the **secondary IDE master** (legacy ports
`0x170`–`0x177`). The driver refuses devices outside a deliberately tiny
2–4 MiB capacity range. It can only mount a disk with the project-specific
`OXPDISK1` or `OXPDISK2` format/version header. Neither formatting nor overwriting an
unrecognized drive is attempted by the kernel.

The implementation uses a two-slot, CRC32-verified, generation-numbered
snapshot journal. It invalidates the inactive destination, writes its payload,
flushes, and writes the new commit header last. Host tests simulate a power
interruption and verify recovery of the previous valid checkpoint. These are
**logical recovery tests**, not proof of crash safety on every storage
controller or filesystem.

### QEMU test with a disposable disk

```bash
## Requires the standard build prerequisites and QEMU.
make run-storage
```

This creates an `OXPDISK2` image at `build/openxp-data.img` **only when absent**, and boots a
separate storage-enabled ISO in QEMU with the image attached as secondary IDE
master. After saving a file, allow several seconds for snapshot commitment,
then shut down normally. Restart `make run-storage` with the same disk image
to test whether HOME contents are restored.

**Do not use your real disk, an existing VM disk, or any file containing
valuable information.** The image is a raw disk, not a Windows `.vdi`.
For an experimental VirtualBox test, convert that disposable raw image with
`VBoxManage convertfromraw` and attach the converted VDI as **IDE Secondary
Master**, leaving the boot ISO on the optical drive. Do not attach a normal
Windows/Linux VM drive in this position.

### Build and test

```bash
make kernel-storage    # Compile the opt-in ATA code
make test-unit         # Includes simulated two-slot recovery tests
make test-sanitize     # Includes sanitizer checks on snapshot handling
```

The default `make iso` and GitHub Actions release build do **not** enable
IDE writes. Even in a storage-enabled build, no writes occur unless a correctly
formatted dedicated image is detected. Never treat this as production storage.

The minimal `kernel/fs/vfs.c` API is a shared path namespace, not a guarantee
of persistent storage. See [VFS](FILES-AND-STORAGE.md#vfs) for its boundaries.

### Aurora 15 manual save and status

When the storage-enabled ISO recognizes a correctly formatted dedicated IDE
data disk, File Manager's **F** key requests a save. A status message reports
whether the disk is available. Requesting a save *queues* a checkpoint; it
does not immediately guarantee the snapshot has reached the disk. A minimum
retry interval prevents repeated write attempts after an I/O error. RAM files
are still the active working set, and the normal ISO never saves across boots.
See [Aurora 15](AURORA-RELEASES.md#aurora-15) for the API details.

### Aurora 17 — capacity and disk versions

- RAMFS now has **48 record slots**, including `.oxpdir` directory metadata,
  with **16 KiB maximum per record** (768 KiB maximum file contents).
- New images from `python3 tools/mkstorage.py NEWFILE.img` use **OXPDISK2**,
  version 2, still only 2 MiB. Each of the two journal slots reserves 1,700
  sectors, so even the maximum file count at maximum size fits. Every commit
  records a CRC-verified snapshot and makes its header visible last.
- **Previously formatted OXPDISK1 disks are recognized without modification**
  and continue using the original 400-sector journal layout. Saving an expanded
  filesystem that no longer fits an old slot fails safely; the last committed
  snapshot is preserved. There is **no automatic disk-format migration**.
- To create an old-format disk only for compatibility tests, use
  `python3 tools/mkstorage.py --legacy-v1 LEGACY.img`.
- Do not delete or overwrite a data image containing documents. To transition
  from OXPDISK1 to OXPDISK2, copy important files out using a trusted test
  mechanism first, shut down, and attach a **new empty, disposable** image.
  There is no supported in-OS migration or import utility yet.
- Standard `make iso` remains **RAM-only**. This is still an experimental
  secondary-IDE checkpoint store, **not** a general reliable filesystem.
  Large full-image commits may temporarily pause the desktop because disk
  I/O is synchronous. A crash-proof real-hardware persistence guarantee has
  not been established.

Tests: `make test-aurora17` and `make test-aurora17-sanitize` cover both
versions, maximum-fill snapshot round trips, interruption recovery and
appearance-preference restoration. They do not replace QEMU/VirtualBox tests.

---

## EXPLORER

## Explorer, file types and desktop backgrounds

OpenXP includes an experimental **Ring 3 Explorer** (`bin/filemanager`), a
native Display Properties app (`bin/settings`), and an XP-inspired virtual
`C:\` directory layout. These are not Windows Explorer or Windows PE support.

### Explorer

Open **My Computer** from Start or `WIN+E`. Its responsive native window
provides folder shortcuts, Back and Up navigation, an address display, colored
file-type icons, selection highlighting and a five-action right-click menu.
Click an item to select it; click it again or press Enter to open it. Up/Down or the on-screen scrollbar
scrolls the current list. Larger windows display more rows. The sidebar jumps to C:, My Documents, System Apps,
Wallpapers, Program Files, or System32.

The right-click menu offers **Open, Copy to HOME, Rename, Delete, Properties**.
Copy, rename and delete are restricted by the VFS: bundled system files remain
read-only. Large files exceeding the RAM filesystem's size limit cannot be
copied into HOME. `HOME` storage is ephemeral unless a separate experimental
persistent-storage mode is used.

### File types and associations

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

### Add wallpaper images

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

### Limits and verification

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

---
