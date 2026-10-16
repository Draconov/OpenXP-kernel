# Userspace and application loading

OpenXP contains an **experimental Ring 3 runtime** for small, statically linked
x86-64 ELF programs. It demonstrates a privilege boundary and separate process
address spaces, but it is not a mature process system or a security boundary
that has been verified under QEMU.

The desktop, terminal and legacy built-in applications still run in **Ring 0**.
Small native applications use a separate Ring 3 GUI syscall interface.

## Process model

A process owns:

- A unique process ID and an entry in a small, fixed-capacity process table.
- A private PML4 root, user code/data pages and guarded user stack.
- A separate guarded kernel-entry stack used through TSS RSP0.
- Exit or fault status, followed by explicit resource reclamation.

The implementation provides a sequential runner with interrupts disabled and
an **experimental** timer-preemptible runner for small numbers of processes.
The desktop now allocates up to three Ring 3 launch workers, each owning its
own text window. Parallel GUI execution still needs live VM verification.
The latter switches CR3 and TSS RSP0 with the saved interrupt context. It must
not be assumed safe until live CPU-level behavior is validated.

A process can be created from a supported ELF image, run, inspected for
completion and reaped. `process_wait_ticks()` polls completion using short PIT
sleeps; it is a kernel API, not a userspace `waitpid` syscall. The process
table is limited to four slots.

## Executable format

The loader (`kernel/user/elf.c`) accepts a **restricted** subset of little-endian
AMD64 ELF64:

- Static `ET_EXEC` images and relocation-free, position-independent `ET_DYN`.
- Up to 16 loadable segments, at most 1 MiB per segment, and 256 pages total.
- Checked offsets, alignment, executable entry points and page bounds.
- Readable segments, zero-filled memory and rejection of writable-executable
  mappings (W^X).

Dynamic linking, ELF interpreters, relocations, TLS and standard general-purpose
PIE executables are unsupported. A failed load can leave partial mappings;
the caller must destroy that address space rather than reusing it.

The build packs `bin/hello`, `bin/fault`, native programs `bin/calculator`,
`bin/clock`, `bin/notepad`, a native ABI probe, and a text file into the embedded
**OXRD** archive. That archive has bounded paths, sizes and CRC32 checks, and
is read-only; it is not a persistent disk filesystem.

## Memory access and isolation

Each process gets private low-half user mappings while inheriting supervisor-only
kernel-half mappings. User pages must carry the user bit at each page-table
level; stacks are writable, non-executable and bordered by unmapped guards.
The normal process stack starts after a guard page at user offset `0x3f0000`
inside the private 4 MiB range. This leaves native ELF code and its larger
zero-filled RW/BSS mapping (2 MiB for `windowd`) separate from the stack.
The layout is defined in `kernel/user/process_layout.h`; the initrd image test
verifies that bundled ELF segments never overlap the reserved stack pages.

`aspace_copy_from_user` and `aspace_copy_to_user` validate the full user range,
then access data through checked physical aliases instead of dereferencing
untrusted user pointers directly. This design is **not concurrency-safe**
against a second thread modifying the same address space during a copy.

## System-call interface

Calls use **`int 0x80`** with the call number in `RAX`. Relevant arguments are
passed through general-purpose registers (shown below).

| Number | Name | Purpose |
| --- | --- | --- |
| `0` | `SYS_VERSION` | Return the kernel ABI identifier |
| `1` | `SYS_PUTC` | Write one ASCII character to serial |
| `2` | `SYS_EXIT` | Terminate the current process |
| `3` | `SYS_TICKS` | Read timer ticks |
| `4` | `SYS_WRITE` | Write up to 1,024 validated user bytes to serial (`RDI` pointer, `RSI` length) |
| `5` | `SYS_GETPID` | Return the current process ID |
| `6` | `SYS_GUI_OPEN` | Open the native text window (`RDI` user title, `RSI` byte count); single-owner |
| `7` | `SYS_GUI_TEXT` | Set native window text (`RDI` pointer, `RSI` byte count; max 383) |
| `8` | `SYS_GUI_EVENT` | Poll one queued keyboard/close event, or zero if none |
| `9` | `SYS_RAM_READ` | Read `home/native-note.txt` into user-writable memory, max 2048 bytes |
| `10` | `SYS_RAM_WRITE` | Replace `home/native-note.txt` from user memory, max 2048 bytes |

Invalid calls or invalid pointers return errors where applicable. This small
ABI is internal to the prototype; it is **not** the Windows API.

## Integration tests

```bash
make smoke-process          # Private mappings, ELF startup and cleanup
make smoke-user             # Ring 3 entry and syscall path
make smoke-user-preempt     # Two timer-preempted user processes
make smoke-user-fault       # Deliberate user fault
make smoke-user-protection  # Rejected supervisor-memory access
make smoke-initrd           # Packaged application and fault-recovery test
make smoke-native           # Native GUI and RAM-file ABI probe
```

These require Limine, Xorriso and QEMU. Until these paths successfully execute
in a VM, the security and fault-isolation claims remain **unverified**. See the
[Roadmap](ROADMAP.md) for verification and development priorities.

## Native application SDK

See [Native applications](NATIVE-APPS.md) for sample programs, the minimal
freestanding user runtime, the build/linker script, and explicit current limits.
The native ABI is unstable and the renderer/window manager remain privileged.
