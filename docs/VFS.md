# Virtual filesystem foundation

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

## Important limitations

- There are no open file handles, seek operations, true directories, ACLs,
  per-process file capabilities, or general-purpose mounts.
- There is no disk filesystem. An optional small IDE-backed snapshot can
  restore RAMFS files, but requires a deliberately formatted disposable
  virtual disk, separate build configuration, and further VM testing.
- Metadata and RAMFS are single-CPU structures, not SMP-safe.
- Do not attach an existing physical or virtual system drive for testing.

## Verify

```sh
make test-unit         # Includes namespace and permission boundary tests
make test-sanitize
make kernel
make kernel-storage    # Compile-only check; does NOT verify disk hardware
```

See [experimental storage](STORAGE.md) and the [roadmap](ROADMAP.md) for the
remaining filesystem and reliability work.
