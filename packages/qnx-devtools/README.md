# Inspect a provisioned QNX device

These source-checkout tools provide target status, slog2 logs, BMP capture and
foreground TCP forwarding. They do not install a runtime, boot the graphics
stack, enable Hermes debugging or qualify a release.

Use a Linux or macOS development host with the repository's Node 26/pnpm
prerequisites, Bash and OpenSSH. The QNX device must already have SSH access
and an independently verified host key in your known-hosts file. Authenticate
using a key or your SSH agent. Unknown or changed host keys and password-only
authentication fail rather than prompting or bypassing verification.

The commands reuse `scripts/lib/qnx-target.sh` for target defaults. Configure
`QNX_TARGET_IP`, `QNX_TARGET_USER`, `QNX_SSH_PORT`, `QNX_SSH_KEY`,
`QNX_SSH_MACS`, `QNX_SSH_CONNECT_TIMEOUT` and `QNX_TARGET_DIR` as necessary.
Defaults are `qnxuser@qnxpi.local`, port 22, and the user's `rnqnx` directory.
These restricted commands ignore SSH config files, reject `QNX_SSH_OPTS` and
force batch authentication. They do not inherit proxy commands, extra forwards
or local commands from an SSH configuration. Existing deployment scripts keep
their previous behavior.

## Check the target and read logs

From the repository root:

```bash
pnpm qnx:device status
pnpm qnx:device logs
pnpm qnx:device logs --follow
```

`status` reads the reported OS and architecture and checks whether `slog2info`
and `screenshot` are available. A non-QNX target fails. Missing tools are
reported as missing; their absence is not disguised as a passing readiness
check. This does not check Screen startup or the React Native application.

Finite commands time out after 30 seconds. Set `--timeout 60` for a slower
capture or connection; the allowed range is 1-120 seconds. Log following and
port forwarding remain in the foreground until Ctrl-C. The CLI forwards
termination to its owned SSH child and escalates to a kill after one second.
Timeouts and interruption are nonzero results, including when the child exits
cleanly after receiving the signal.

## Capture the current display

A screenshot may include private screen content. Nothing is uploaded.

```bash
mkdir -p build
pnpm qnx:device screenshot --out build/qnx-display.bmp
```

The destination's parent directory must exist. Existing files and symlinks
are refused, including files created during the capture. The command uses the
existing target `screenshot -file=...` utility, streams the result over SSH,
checks BMP header/length consistency and a 32 MiB limit, then exclusively
creates a local file with mode 0600. This is not a complete image decoder or
a claim that the correct app was visible.

On the target, capture uses a fresh private directory under `QNX_TARGET_DIR`.
The remote shell removes its own capture on normal completion and handled
errors. Connection loss, power loss or an uncatchable signal can leave that
private directory behind; inspect it before removal. Boot files, the running
app and other capture directories are untouched. Local disk-write failure
can leave an incomplete new output file; no existing file is replaced.

## Forward a known target service

Use the port of a service you have independently verified is listening. The
numbers below are examples, not an assertion that QNX Hermes listens there:

```bash
pnpm qnx:device port-forward --local-port 9229 --remote-port 9229
```

The requested tunnel binds only `127.0.0.1` on the development host and forwards
to `127.0.0.1` on the target. No background daemon or reused SSH control master
is created. A local bind failure is an error. A connected SSH tunnel does not
prove that a target service accepts connections; OpenSSH forwarding setup and
destination-service availability are distinct. This command does not add a
Hermes CDP server, advertise an inspector or launch React Native DevTools.

See the upstream [ssh manual](https://man.openbsd.org/ssh.1) for `-L`, `-N`
and `-F`, and [ssh_config](https://man.openbsd.org/ssh_config.5) for
`ExitOnForwardFailure`, `StrictHostKeyChecking` and `BatchMode`.

## Preview or recover

Add `--dry-run` to print the local executable, arguments, timeout and output
path without starting SSH, validating the remote environment or writing a
capture. It is an argument plan, not a device test.

Authentication failure: verify the target host key independently and load the
correct key into your agent or set `QNX_SSH_KEY`. Do not disable host-key
checking. Missing screenshot utility or permission denial: use the named
board's provisioning guide; this tool never escalates to root. Occupied local
port: choose a free unprivileged port. Existing capture: choose a new filename.

## Evidence boundary

The automated tests execute the real wrapper and remote shell scripts against
synthetic SSH, QNX identity and screenshot tools on the host. They verify
arguments, bounds, failures and cleanup without opening a network connection.
Physical QNX transport/capture and an actual inspector session require their
own receipts. No component-support or release-readiness record is promoted.
This directory remains a source-checkout tool, not a published npm package.
