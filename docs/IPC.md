# Process handles and native window-service IPC

OpenXP has a **small experimental capability IPC mechanism**, separate from its
older kernel-thread `sync/ipc.c` queues. This is the first transport intended
for communication between *independent Ring 3 processes*. It does **not** yet
replace the Ring 0 desktop, Start menu, taskbar or compositor.

## Try the two sample programs

The normal boot archive contains two independent native applications:

```text
RUN BIN/WINDOWD
RUN BIN/WINCLIENT
```

Start `WINDOWD` first. It is the kernel-authorized service process, which opens
an ordinary native graphics window, listens for a connection and returns `PONG`
to an IPC `PING` message. The client has its own PID, address space, window and
opaque channel handle. Both demonstration programs terminate after the exchange.

The normal kernel does not start them automatically. Use `PS` to inspect running
processes. The **optional** QEMU integration target launches both automatically:

```bash
make smoke-window-ipc
```

The expected serial marker is `OPENXP_CAPIPC_OK`. Compilation and host-side
unit tests do not establish whether that marker is actually emitted by QEMU.

## Kernel design

- `kernel/user/capipc.c` owns every endpoint, message queue and handle table.
  User programs never receive mapped kernel buffers or pointers.
- The trusted kernel launcher grants a **single server role** only to
  `bin/windowd` from the *read-only boot archive*. Other Ring 3 callers cannot
  register the service. This is a boot-image trust assumption, **not** package
  signing or an application security policy.
- A client connects to the singleton service, receiving a **process-owned
  opaque handle**. The server accepts and receives a different handle.
- Handles contain a slot number and a generation counter. All operations check
  the caller's PID; close and slot reuse invalidate stale handles.
- Each direction is a separate queue with at most four messages of **1–64
  bytes**. The server and each client can send and receive. Clients are limited
  to two concurrent channels; the global fixed table supports twelve endpoints.
- Requests return immediately if a queue is full or empty. `IPC_WAIT` uses
  the existing scheduler's atomic park/wake path and optional timer timeout.
- Process exit or fault closes that process's handles, wakes peers and releases
  service registration. A peer can drain already queued messages after closure.
- This is a single-CPU design guarded by local interrupt masking. It is **not**
  an SMP-safe implementation or a general-purpose secure microkernel.

## Ring 3 system calls

`apps/native/oxp.h` provides the demonstration wrappers:

| Call | Purpose |
| --- | --- |
| `IPC_LISTEN` (13) | Claim the authorized singleton service |
| `IPC_ACCEPT` (14) | Accept the oldest pending connection |
| `IPC_CONNECT` (15) | Connect to the listening service |
| `IPC_SEND` (16) | Copy 1–64 bytes into the peer's queue |
| `IPC_RECV` (17) | Copy the next message into a validated user buffer |
| `IPC_CLOSE` (18) | Release the caller's channel handle |
| `IPC_WAIT` (19) | Block until a message/connection/close or timeout |

`SEND` and `RECV` take `(handle, user_buffer, length)` in `RDI`, `RSI`, `RDX`.
Other calls use the existing two-argument syscall convention. Successful
connect/accept return an opaque positive handle; send/receive return a byte
count. Other successful calls return zero. Negative values indicate errors
(e.g. `-11` unavailable, `-32` disconnected, `-1` permission denied).

## Security boundaries and limitations

IPC rejects unowned handles, validates lengths and mapped user pages before
copying, and clears resources on exit. There is **no privilege escalation from
an IPC connection**: owning a channel does not grant framebuffer access or a
window-manager capability. The demo's windows use the *existing* PID-checked GUI
syscalls, which still draw through the kernel compositor.

Before migrating the desktop, the service must gain a narrowly specified window
protocol, explicit process launch authority, comprehensive failure handling,
owner-authenticated graphics events, and a userspace shell. Security auditing,
real QEMU/VirtualBox testing and SMP-safe locking are outstanding.
