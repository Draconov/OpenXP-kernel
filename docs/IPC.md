# Process handles and native window-service IPC

OpenXP has a **small experimental capability IPC mechanism**, separate from its
older kernel-thread `sync/ipc.c` queues. This is the first transport intended
for communication between *independent Ring 3 processes*. The service now
also owns Ring 3 taskbar/Start-menu policy and drawing requests, while final
pixel composition and legacy windows remain in Ring 0.

## Try the window-management protocol pilot

The normal graphical boot automatically starts `bin/windowd` (Ring 3).
Open Terminal and run its separate demonstration client:

```text
RUN BIN/WINCLIENT
```

The client connects via capability IPC, creates a logical window, and then
moves, focuses, queries and closes it. The server validates channel ownership,
and a restricted `WM_DISPLAY` syscall forwards accepted changes to the real
native-window broker. The legacy Ring 0 compositor renders the resulting
positions on its next desktop-loop iteration.
The service remains alive after each demonstration client exits. It runs
headlessly so client windows retain all native GUI slots. The shell service
also manages global shortcuts, Start menu and taskbar rendering policy.
The kernel restores its own desktop shell on service failure.

The optional QEMU integration target auto-launches `windowd` and two
concurrent independent clients:

```bash
make smoke-window-ipc
```

Expected serial markers: `OPENXP_WM_BRIDGE_OK`,
`OPENXP_WM_MULTICLIENT_OK`, and `OPENXP_CAPIPC_OK`.
The first requires the live GUI broker to receive create/move, focus and close
requests; this is still not pixel-level screenshot verification.
Host tests do not substitute for a live QEMU or VirtualBox run.

### Window protocol

`apps/native/wm_protocol.h` defines the versioned 60-byte `wm_packet_t` wire
format, carried inside the existing bounded 64-byte IPC transport. Commands
are `WM_CREATE`, `WM_MOVE`, `WM_FOCUS`, `WM_QUERY`, `WM_CLOSE` and `WM_PING`.
Replies carry the reply flag, result code and current logical window geometry.
The Ring 3 service (`apps/native/wm_protocol.c`) owns a fixed table of up to
six logical windows, currently limited to one per client channel because the
native broker supports one window per PID. IDs contain a slot and generation number so deleted
window IDs cannot be reused as stale capabilities.

Window operations verify that the *accepted channel* owns the referenced
window. They validate geometry and desktop bounds, scrub control characters
from titles and clean up owned windows on disconnection. A different channel
cannot modify or query another client's window. This is a **userspace window-policy state machine** with a narrowly scoped
kernel compositor bridge; actual composition and hardware input remain in Ring 0.

Run the host regressions independently with:

```bash
make test-window-protocol
make test-window-protocol-sanitize
```

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
  Service-only `IPC_WAIT(UINT64_MAX, timeout)` atomically waits for **any**
  pending accept, accepted-channel message or client disconnect. The server
  therefore needs no continuous polling and services one request per client
  per pass. A bounded wait also checks for the service-window close event.
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
| `IPC_WAIT` (19) | Block until a message/connection/close or timeout; the listening service can pass `UINT64_MAX` to wait for any client |

`SEND` and `RECV` take `(handle, user_buffer, length)` in `RDI`, `RSI`, `RDX`.
Other calls use the existing two-argument syscall convention. Successful
connect/accept return an opaque positive handle; send/receive return a byte
count. Other successful calls return zero. Negative values indicate errors
(e.g. `-11` unavailable, `-32` disconnected, `-1` permission denied).

## Security boundaries and limitations

IPC rejects unowned handles, validates lengths and mapped user pages before
copying, and clears resources on exit. There is **no privilege escalation from
an IPC connection**: owning a channel does not grant framebuffer access or a
window-manager capability. The demo's displayed windows use the *existing* PID-checked GUI
syscalls, which still draw through the kernel compositor.

The service now starts automatically and has a narrow, process-authenticated
input/focus and shell-rendering interface. Full userspace window composition,
process launch permissions, service restart supervision, security auditing,
real QEMU/VirtualBox testing and SMP-safe locking are outstanding.

## Live window compositor bridge

`windowd` can now submit a `WM_DISPLAY` syscall (number 20) with an accepted
IPC handle and a copied `openxp_wm_display_cmd_t`. The kernel resolves that
handle to the **connected client PID**; user-provided PIDs are never trusted.
A disconnected, stale, unaccepted, or unauthorized channel is rejected.

`SET_RECT` requests a bounded window position and size; `RAISE` requests focus;
`REQUEST_CLOSE` delivers the existing sticky close event to that client's GUI
queue. The boot thread reconciles native-window metadata to the kernel's
existing compositor on its next desktop loop. Window contents remain
process-owned RGB565 surfaces, never direct framebuffer mappings.

**Limitations:** The final framebuffer compositor is still Ring 0, and the
protocol assumes a 1024×768 display. The Ring 3 service now owns taskbar/Start
rendering commands and mouse/keyboard focus requests, but dragging remains
kernel-hosted and only one native window exists per client. There is no general
Ring 3 compositor or automatic service restart. The old desktop is the fallback.
