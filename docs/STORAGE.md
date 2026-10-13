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
`OXPDISK1` format/version header. Neither formatting nor overwriting an
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

This creates `build/openxp-data.img` **only when absent**, and boots a
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
