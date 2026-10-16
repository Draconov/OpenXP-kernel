# Experimental persistent storage

OpenXP normally uses an **in-memory HOME folder**, which disappears on reboot.
A separate opt-in IDE prototype can save a bounded snapshot to a dedicated
virtual disk. It is **not verified in QEMU or VirtualBox** and is **not safe for
important data**. It does not provide FAT, NTFS, a full filesystem with directory handles,
multi-user permissions or general disk access.

## Why it is disabled by default

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

## QEMU test with a disposable disk

```bash
# Requires the standard build prerequisites and QEMU.
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

## Build and test

```bash
make kernel-storage    # Compile the opt-in ATA code
make test-unit         # Includes simulated two-slot recovery tests
make test-sanitize     # Includes sanitizer checks on snapshot handling
```

The default `make iso` and GitHub Actions release build do **not** enable
IDE writes. Even in a storage-enabled build, no writes occur unless a correctly
formatted dedicated image is detected. Never treat this as production storage.

The minimal `kernel/fs/vfs.c` API is a shared path namespace, not a guarantee
of persistent storage. See [VFS](VFS.md) for its boundaries.

## Aurora 15 manual save and status

When the storage-enabled ISO recognizes a correctly formatted dedicated IDE
data disk, File Manager's **F** key requests a save. A status message reports
whether the disk is available. Requesting a save *queues* a checkpoint; it
does not immediately guarantee the snapshot has reached the disk. A minimum
retry interval prevents repeated write attempts after an I/O error. RAM files
are still the active working set, and the normal ISO never saves across boots.
See [Aurora 15](AURORA-15.md) for the API details.

## Aurora 17 — capacity and disk versions

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
