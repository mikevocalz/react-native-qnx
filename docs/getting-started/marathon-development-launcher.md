# Launch a Marathon development client locally

Use this source-distributed command on a Linux target with Node 26, a trusted
native runner already compiled for its architecture/libc/Qt installation,
and an existing Wayland session. It does not build or deploy the runner.
The default QNX commands remain unchanged.

```bash
pnpm qnx marathon inspect --runner /path/to/rnqnx_marathon_host --bundle /path/to/app.bundle.js
pnpm qnx marathon run --runner /path/to/rnqnx_marathon_host --bundle /path/to/app.bundle.js --dry-run
pnpm qnx marathon run --runner /path/to/rnqnx_marathon_host --bundle /path/to/app.bundle.js
```

`inspect` reads bounded ELF program headers and bundle metadata without running
native code or invoking `ldd`. It accepts dynamic 64-bit little-endian x64 or
arm64 executables matching the host and a recognized installed Linux loader.
This does not verify all dependent libraries, Qt plugins, binary authenticity
or application identity. The runner and bundle must be trusted regular files;
symlink paths and empty/oversized inputs are rejected. This is not protection
against another trusted user process changing a file after inspection.

`run` requires an explicit `WAYLAND_DISPLAY` socket name and an absolute,
private, current-user-owned `XDG_RUNTIME_DIR`. The socket must exist and be
owned by that user. These checks do not authenticate the compositor or prove
it is MarathonOS. The child uses `QT_QPA_PLATFORM=wayland`; inherited
`WAYLAND_SOCKET` is removed to avoid using a stale descriptor instead of the
checked socket. There is no fallback to an offscreen window.

The executable receives an argument vector containing only `--bundle` and the
absolute bundle path. No shell interprets paths. The timeout defaults to 300
seconds; use `--timeout` from 1 to 3600. Timeout and interruption stay nonzero
(exit 124, 130 or 143), even if the child exits cleanly. The command terminates
its owned process group; it never manages the compositor or unrelated apps.
A successful child exit alone is not a rendered-window receipt.

This is an unconfined development launch, not a signed or managed `.marathon`
package, sandbox, installer, permissions adapter or lifecycle handshake. The
existing runner still controls its current app ID and registered surface.
Do not claim a consumer identity change, managed-app support or physical
Marathon qualification from these commands.

Tests compile and execute a small native Linux ELF fixture, create a real
Unix socket and check arguments, failure, timeout, termination and cleanup.
That fixture is not Qt or a Wayland compositor. The actual Marathon runner,
compositor and target device need separate evidence. Core tests require no
premium package or licensed SDK. Remove this command and its bin delegation
to roll back; it writes no project, boot or application files itself.
