# Bounded application trials

This lane exercises the existing React Native/Fabric/Hermes/Qt Quick runner. It
does not modify the recovery image, graphics startup, package graph or window
defaults. Run the application as its normal unprivileged account.

The screenshot runner checks errors after React startup, captures its frame,
stops its surface and checks cleanup errors before saving the PNG. A success
marker is insufficient: the supervising process must also observe normal exit.
The startup and cleanup failure fixtures deliberately throw through the real
AppRegistry bindings and must produce neither a success marker nor a PNG.

## Host supervisor

Supply an operator-owned JSON provenance declaration with source revision,
working-tree changes, lockfile/bundle/binary hashes, dependency versions, build
flags, compiler/sysroot, Qt plugins and target image/display identity. The tool
records that declaration; it does not independently verify it.

```sh
python3 scripts/runtime-baseline.py \
  --candidate /absolute/candidate.json --output /absolute/new-trial-directory \
  --trials 10 --timeout 35 --width 1920 --height 1080 \
  -- /absolute/application-launcher '{screenshot}'
```

The launcher must run the selected Hello World Nitro bundle, pass the unique
screenshot path and exit with the application's status. The supervisor requires
the Nitro and screenshot markers, a complete supported PNG of the requested
dimensions and exit zero. It records logs, image hashes and incremental JSON
receipts. It stops at the first failure. Existing output directories are refused.
An optional `--health-command /absolute/executable` runs a bounded health check
after each trial. Do not put credentials in command arguments or provenance.

Captured dimensions and markers do not establish the expected title, pixels,
physical presentation or startup latency. Inspect expected content separately.
The supervisor's elapsed duration includes launch, capture and shutdown.

## QNX child ownership

The existing host and QNX playground build lanes provide
`rnqnx_runtime_baseline_supervisor`, a standalone POSIX executable without Qt
dependencies. On the target:

```sh
/absolute/rnqnx_runtime_baseline_supervisor 35 /absolute/new-application.log \
  -- /absolute/application-launcher /absolute/new-frame.png
```

The launcher must `exec` the application. The supervisor owns one direct child,
uses a monotonic deadline, sends TERM then KILL after a one-second grace period
and reaps the child. It never kills by process name. Exit 124 means deadline,
125 means interrupted supervision, 2 means supervisor setup failure; otherwise
the child's exit status or 128 plus its terminating signal is returned. Logs are
created exclusively with mode 0600. Use a new log path for every invocation.
The supervisor's own status marker is separate from the application's log.

This is a process watchdog, not cancellation of native operations or a hard
real-time guarantee. It does not manage daemonized descendants. A remote adapter
must invoke this target supervisor, retrieve its log and PNG, preserve status and
verify target health. Killing only a local SSH client cannot prove remote cleanup.
The host supervisor can wrap such an adapter but does not provide authentication
or remote transfer itself.

## Regression commands

```sh
node tests/js/runtime-baseline.test.mjs
node tests/js/runtime-baseline-qnx.test.mjs
cmake --build /absolute/full-host-build --parallel 4 \
  --target rnqnx_playground_host rnqnx_runtime_baseline_supervisor
ctest --test-dir /absolute/full-host-build -R rnqnx_playground_ --output-on-failure
```

The JavaScript tests use simulated children; the target supervisor test compiles
the same standalone C++ source with the host compiler. These tests cover errors,
crash, timeout, interruption cleanup, stale artifacts and recovery. They are not
physical-device evidence. The runner CTests exercise actual host Hermes/Qt and
AppRegistry startup/cleanup failure. Record QNX execution separately.

Rollback is to use the recorded earlier application executable. This slice has
no boot, library, dependency, default window or dispatcher-flag changes.
