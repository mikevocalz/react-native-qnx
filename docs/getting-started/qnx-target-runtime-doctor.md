# Check QNX runtime observations offline

Use this check when diagnosing a Qt application on a new QNX image. It reads
observations you collected on the target and reports missing prerequisites.
It never connects to the target, runs commands from the report, reads target
paths on the host, changes environment variables or starts an application.

```sh
pnpm qnx doctor --target-runtime-report observations.json --verbose
pnpm qnx project doctor --target-runtime-report observations.json --json
```

The root spelling is an alias only when `--target-runtime-report` is supplied.
Ordinary `qnx doctor` keeps its existing environment/toolchain checks. Both
spellings accept the same observation options.

This mode is separate from `project doctor --project <directory>`, which keeps
its static project checks. Do not combine `--project`, `--entry` or
`--identity-tool` with `--target-runtime-report`. `--platform marathon` is
rejected in this mode. Linux/Wayland diagnostics are unchanged.

Exit status is 0 when every supplied prerequisite observation passes, 1 when
an observation identifies a missing prerequisite, and 2 for an invalid report
or invocation. A passing report has `runtimeVerified: false` and
`evidenceTier: "offline-observation"`, `observationOrigin: "user-supplied"`
and `collectionVerified: false`. The report may be stale or forged; there is no
trusted collector, attestation or freshness verification. It does not establish a native build,
ABI compatibility, plugin load, displayed frame or release gate.

## Collect the observations

Use the application user and the environment intended for its launcher.
An interactive SSH shell may have different paths. Read the launcher's exports;
do not source a launcher, since that can start the application. Keep the raw
collection log locally and remove credentials, private device addresses and
unrelated process details before sharing it.

The following are read-only collection examples to run yourself on the
licensed target. The checker does not execute them:

```sh
# Process names only; inspect for Screen and slogger2.
pidin -F '%N'
# Existence/access observations, not functional service tests.
test -d /tmp && test -w /tmp && test -x /tmp
test -r /dev/random
# Print only the relevant launcher environment, never the entire environment.
printf 'LD_LIBRARY_PATH=%s\n' "$LD_LIBRARY_PATH"
printf 'QT_PLUGIN_PATH=%s\n' "$QT_PLUGIN_PATH"
printf 'QML2_IMPORT_PATH=%s\n' "$QML2_IMPORT_PATH"
printf 'QT_QPA_FONTDIR=%s\n' "$QT_QPA_FONTDIR"
printf 'QT_QPA_QNX_DISPLAY_CONFIG=%s\n' "$QT_QPA_QNX_DISPLAY_CONFIG"
printf 'QQNX_PHYSICAL_SCREEN_SIZE=%s\n' "$QQNX_PHYSICAL_SCREEN_SIZE"
```

Verify directories and font/config files with the image's read-only file
inspection tools. A process name or readable device node alone does not prove
service functionality. Set `random` true only after a functional random-device
check using your image's documented diagnostics. Check IPv6 capability from the
actual installed stack and image configuration; using IPv4 does not remove Qt's
IPv6 requirement. Check the reported display count/order, physical dimensions
and any display configuration file. Do not guess dimensions from pixel
resolution. A zero millimetre dimension needs a measured
`QQNX_PHYSICAL_SCREEN_SIZE=width,height` fallback.

Record the actual resolved library, plugin and QML directories, whether selected
by environment, installation defaults or application configuration. Unset
variables alone do not mean those facilities are absent. Record fontconfig when
used, or the directory and file of an actual configured/default/registered
font. Reporting a file does not prove that Qt loaded it; retain that limitation
in the device receipt.

Supply the exact board, image/package build, SDP installation label and UTC
collection time. These are labels supplied by the collector, not verified
identities. Do not reuse a fixture as a physical-device receipt. If an observation
is unknown, collect it before completing the report; missing or malformed
fields are rejected, and an unestablished prerequisite must not be marked true.

## Observation format

This is a synthetic example. Replace every observation with your actual result:

```json
{
  "schema": "rnqnx-qnx-runtime-observation/v1",
  "capturedAt": "2026-11-14T12:00:00Z",
  "target": {
    "board": "fixture board",
    "image": "fixture image build",
    "sdp": "8.0.5 fixture installation"
  },
  "services": { "screen": true, "slogger2": true },
  "tmp": true,
  "random": true,
  "ipv6": true,
  "fontconfig": false,
  "directories": ["/runtime/lib", "/runtime/plugins", "/runtime/qml", "/app/assets/fonts"],
  "files": ["/app/assets/fonts/Test.ttf"],
  "environment": {
    "LD_LIBRARY_PATH": "",
    "QT_PLUGIN_PATH": "",
    "QML2_IMPORT_PATH": "",
    "QT_QPA_FONTDIR": "",
    "QT_QPA_QNX_DISPLAY_CONFIG": "",
    "QQNX_PHYSICAL_SCREEN_SIZE": ""
  },
  "resolvedDirectories": {
    "libraries": "/runtime/lib",
    "plugins": "/runtime/plugins",
    "qml": "/runtime/qml",
    "fonts": ["/app/assets/fonts"]
  },
  "display": {
    "widthMm": 530,
    "heightMm": 300,
    "configurationVerified": true
  }
}
```

All keys are required; unknown keys are refused. Environment values may be
empty. The file must be regular, not a symlink, and at most 64 KiB. Path lists
have at most 128 unique, absolute, normalized paths each. The checker rejects
control characters and relative path components. It never executes report
values or probes them on the host. `.ttf`, `.otf` and `.ttc` files are accepted
as font observations. Dimensions are finite millimetres from 0 through 10000;
a configured fallback must contain two positive values no greater than 10000.
These bounds validate report shape, not hardware support.

`configurationVerified` means the collector checked the count and order of
displays. An unset `QT_QPA_QNX_DISPLAY_CONFIG` is permitted when the default
configuration was checked. A configured path must be in `files`. The checker
cannot parse or validate that target configuration from a path alone.

`--verbose` suggests `QT_DEBUG_PLUGINS=1`, `LD_DEBUG=1` and
`QT_LOGGING_TO_CONSOLE=1` for a separately supervised first-start investigation.
It does not apply them. Debug logs may contain paths and application data;
review them before sharing. This mode grants no launch permission and changes
no boot, graphics, font, window or scheduling configuration.

See [Qt's QNX target requirements](https://doc.qt.io/qt-6/qnx-target-requirements.html)
and [QNX platform support](https://doc.qt.io/qt-6/qnx-support.html), inspected
as Qt 6.12 on 2026-11-14. The existing [Pi setup guide](raspberry-pi-400.md)
remains the device runbook. Runtime packaging and exact SDP/library compatibility
remain separate checks under [packaging](../packaging.md).
