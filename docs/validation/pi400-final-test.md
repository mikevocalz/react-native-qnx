# Final supervised Raspberry Pi 400 physical test

Tracks [Issue #43](https://github.com/mikevocalz/react-native-qnx/issues/43) and the exact current [QNX-RN-008 / QNX-RN-009 queue](../handoff/QNX_VALIDATION_QUEUE.md). This is a **physical validation procedure**, not proof of a completed physical result.

## What the issue already proves

The Raspberry Pi 400 with the QNX 8.0.5 custom CTI image boots; Ethernet, SSH, partition mounts and corrected PAM paths survived a reboot. The image comes from CTI 8.0.5r1, NOT the Pi 4/5 stock QSTI 0.5.0 build 50. Wi-Fi, a QNX Screen window, Hermes/React/Fabric/Qt, user-visible frame, interaction and performance were not proven by Issue #43. Never promote a Pi 400 result to stock QSTI or vice versa.

## Inspect without touching the device

From a clean checkout using Node 26 and pnpm 12.8.1:

    node scripts/final-pi400-test.mjs plan

This prints six stages with status **not-run**, never invokes SSH, installs packages, builds, reboots or alters the device. Host CI runs this path and its argument/permission tests without hardware. The execution regression tests use inert local commands and fixture artifacts, not a simulated device qualification pass.

## Supervised physical execution

Use only an **authorized Pi 400** with a verified SSH host key, a non-privileged qnxuser account, visible QNX Screen and an eligible QNX/Qt runtime. Have a local recovery path ready. Source the licensed SDP 8.0.5 environment on the development machine first and install the exact locked dependencies.

Execute from Linux or macOS. Every stage uses batch-mode SSH, strict host-key verification, a 10-second connection timeout and the configured `QNX_SSH_KEY`/`QNX_SSH_PORT`. Arbitrary `QNX_SSH_OPTS` are refused; configure your verified known-hosts file in SSH config instead. Root, numeric usernames and option-like usernames/hostnames are rejected before device access. The first remote command verifies a nonzero UID and the QNX/Pi 400/AArch64 identity before preflight or deployment.

Before executing, create the shared Expo app, build its QNX bundle, compile that JavaScript to **matching Hermes .hbc**, and use project prepare-native with the exact HBC input. Any validated HBC filename below `build/qnx/` is accepted, including `release.hbc`; the runner uses the bundle selected and hashed by the native descriptor, not a fixed filename. Confirm the native app identity matches the actual AppRegistry component and the generated descriptor hashes the HBC. The source tree must be the intended candidate.

    export RNQNX_PI_BOARD=pi400
    export RNQNX_FINAL_TEST_CONFIRM=pi400
    export QNX_TARGET_USER=qnxuser
    export QNX_TARGET_IP=<verified-pi400-hostname>
    # QNX_HOST and QNX_TARGET must come from the licensed SDP environment
    node scripts/final-pi400-test.mjs execute --consumer-project /absolute/path/to/expo-app

After the identity check, the runner executes the existing _real_ Pi preflight, Screen deployment, combined Hermes/Nitro/Expo runtime probe, and full Qt/Fabric/Hermes QNX consumer cross-build, in that order. QNX preflight reads processor architecture from `uname -p`; `uname -m` identifies the board. It then creates a **new isolated** application subdirectory below the configured target directory, copies the executable, matched bytecode and the built `libhermesvmlean.so`, attempts a bounded native screenshot render and copies the PNG back. The consumer's local library path therefore includes its staged Hermes runtime, rather than relying on a copy in the parent deployment directory. Existing deployment tools may change the ordinary RNQNX deployment directory, but the runner never flashes, reboots, changes boot configuration, disables SSH host verification, uses root or overwrites an earlier isolated final-test directory.

It **fails closed** if any executable, library, Qt plugin, QNX screen, Hermes bytecode, app registration, runtime module or frame capture is absent. A Linux preview, source validator, screenshot from the Linux host or completed upload cannot satisfy physical execution.

The canonical AArch64 toolchain also selects `QNX_TARGET_ARCH=aarch64` for shared dependency imports and rejects a conflicting architecture, including on a fresh configure. An external target Qt SDK can be supplied through `CMAKE_FIND_ROOT_PATH`; the toolchain preserves those target roots, adds the QNX sysroot once, and retains target-only package/library/header filtering. For a custom Qt installation outside the system loader paths, configure the consumer's own CMake runtime search paths and verify the installed Qt plugins and fonts. By default, the final `LD_LIBRARY_PATH=.` supplies only the staged Hermes library; an explicit process-local profile below can supply the installed Qt dependency closure. Record any Qt host paths, target dependency roots, RPATH and debug-symbol settings as part of the exact candidate's build profile; do not change system libraries or bypass a failed stage to manufacture a pass.

If the verified target Qt uses FreeType without fontconfig and its default font directory is absent, explicitly select an **existing target** font directory with `RNQNX_PI_QT_FONTDIR=/absolute/target/font-directory`. The identity stage checks that directory and at least one readable regular `.ttf`, `.otf`, `.pfa` or `.pfb` file before any deployment. The selected path is recorded in the receipt and passed as `QT_QPA_FONTDIR` only to the final consumer process; no global Qt, SSH or boot configuration is changed. An unset selection preserves Qt's default lookup. This is a file-access preflight, not font-format, licensing or glyph correctness validation: record the chosen font fingerprints separately and inspect the actual rendered text. The directory is not searched recursively.

Device evidence is written locally under a private, timestamped **artifacts/pi-final/** directory with a unique suffix. The result contains the candidate commit, HBC/executable/Hermes hashes, each observed stage outcome, and a native PNG hash if it exists. A render stage passes only after its marker is found; a capture stage passes only after regular-file, size, signature and hash checks. Missing markers or invalid captures are recorded as a failure of that exact stage, not only a top-level error. Scrub device hostnames, paths and other sensitive system information before sharing artifacts; the workflow does not automatically upload private physical-run logs. The independent reviewer must inspect the raw records and actual screen output.

## Optional process-local Qt runtime profile

For Qt installed outside system loader paths, `RNQNX_PI_LIBRARY_PATH` accepts a
colon-separated list of existing absolute target directories (at most 16 entries,
4096 characters total). Empty entries, relative paths, parent traversal and shell
metacharacters are rejected before SSH. The identity stage verifies directory
read/search access before any deployment. Only the final consumer receives
`LD_LIBRARY_PATH=.:<selected-directories>`; the isolated staged Hermes stays first.
Unset preserves `LD_LIBRARY_PATH=.`. Verify the transitive dependency closure with
this exact process-local profile; the executable's RPATH alone may not resolve
dependencies of installed Qt libraries that retain their own SDK RPATH.

For an explicitly selected Qt plugin installation, set
`RNQNX_PI_QT_PLUGIN_PATH=/absolute/target/qt/plugins`. The identity stage requires
a readable regular `platforms/libqqnx.so` before deployment. Only the consumer
receives `QT_PLUGIN_PATH` and `QT_QPA_PLATFORM_PLUGIN_PATH` (the selected directory
and its `platforms` subdirectory). Unset preserves default Qt plugin lookup.
These selections and the effective library path are recorded in the receipt.
Fingerprint the actual selected Qt libraries/plugin separately; directory/file
access checks do not establish ABI compatibility, licensing or device execution.
No global loader, Qt, SSH, system-library or boot setting is changed.

## Handle a failed stage

The combined runtime probe consumes two bytecode fixtures, not JavaScript source:
the lean Hermes VM cannot compile source at runtime. The runtime validation script
uses the executable host compiler selected in the Hermes build cache to compile
the Nitro and Expo checks, records their source/compiler/HBC hashes, stages both
files, and invokes `rnqnx_runtime_qnx_probe <nitro.hbc> <expo.hbc>`. The probe rejects
missing, empty, oversized or invalid bytecode and reports exceptions as failures.
These fixtures are distinct from the generated consumer's descriptor-selected HBC.
Every qualification SSH/SCP stage explicitly enforces `IdentitiesOnly=yes`,
including the prerequisite scripts; a configured key does not rely on agent order.

Each command owns a local process group. A timeout, interruption or output above 8 MiB stops the group, escalating to SIGKILL after at most 250 ms and closing inherited pipes so descendants cannot hold the result open. The harness stops before subsequent stages.

Local cancellation does **not** prove that a command already accepted by the target stopped, nor does it roll back an upload or deployment. Failure receipts set `remoteCleanupVerified=false` and require operator inspection before retry. Inspect the target's processes and deployment directories through a verified connection; do not blindly retry, reboot, delete evidence or replace boot files. Keep the working recovery path and existing app intact.

## Separate remaining acceptance

Even if the Pi 400 native screenshot passes, **all seven v1 release gates remain under independent review**, including a separate eligible open-source Qt distribution closure, candidate CI, 20 consecutive **supervised cold power cycles**, stock Pi 4/5 QSTI device parity, real presented-frame/RSS metrics for the three workloads, independent developer setup, and versioned release documentation. This runner never edits docs/releases/v1-readiness.json or announces release readiness. Its source code also does not assert Expo Router, arbitrary Nitro/Expo module compatibility, accessibility, Reanimated, RNGH or live MarathonOS shell parity.

The QNX qualification reviewer owns the device execution and disposition; a separate maintainer reviews evidence. Until the actual run takes place, results are **not-run**, and the PR must remain draft.

## Optional GitHub workflow

The dedicated workflow has an ordinary Ubuntu test job and a manually dispatched Pi 400 job requiring a separately provisioned self-hosted runner labeled qnx-pi400. Only explicit selection of execute and an absolute consumer path invokes physical operations. A missing runner, QNX SDK, target host or device results in a blocked/not-run physical lane; it is never treated as a pass. No temporary SSH key or SDP license is uploaded.
