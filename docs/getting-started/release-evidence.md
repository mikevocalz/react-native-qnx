# Check performance and cold-boot evidence

Use this command to inspect a capture before submitting it for release review.
It checks the data, hashes the referenced raw files, and prints a JSON report.
It does not connect to hardware, reboot a board, collect measurements, change
release gates, sign artifacts or approve redistribution.

Use the repository's documented Node version. No additional packages are needed.
Keep the capture JSON beside its raw logs:

```bash
node scripts/qualify-qnx-evidence.mjs artifacts/qualification/capture.json
```

Add `--require-complete` to make incomplete captures fail a qualification job:

```bash
node scripts/qualify-qnx-evidence.mjs artifacts/qualification/capture.json --require-complete
```

Exit status is `0` for a valid report, `1` for invalid data or failed file
verification, and `2` for an incomplete capture with `--require-complete`.
The command only reads files. Redirect stdout to a **different** filename when
saving a report; shell redirection to the input filename would erase the input
before the command starts.

## Identify the source and target

Every capture has `schemaVersion: 1`, a `kind` of `performance` or `cold-boot`,
a `provenance` object, and an `artifacts` array.

The provenance object requires:

- `sourceCommit`: the complete 40-character Git commit; `lockfileSha256`,
  `binarySha256` and `bundleSha256`: lowercase 64-character SHA-256 values.
- `lane`: `host`, `qnx-physical`, `qnx-vm` or `marathon-physical`;
  `board` and `image`: the named board and exact image/build identity.
- `runtime`: the SDP, Qt, RN, Hermes and Nitro versions and build flags;
  `captureMethod`: the collection method, clock alignment, display mode and
  instrument boundary; `limitations`: everything the capture does not prove.

Each artifact is an object with `path` and `sha256`. Paths are relative to the
capture JSON directory. Supply the raw logs/traces, not just a screenshot or
this command's report. A capture accepts 1-256 nonempty regular artifacts,
each at most 64 MiB. Absolute paths, traversal, duplicate paths and symlink
components are rejected. JSON input is limited to 4 MiB.

Hash syntax and file integrity do not authenticate an author, establish that a
commit exists, or prove observations were made on the declared board. A person
must review the source identity and raw contents. This is not a sandbox against
another process changing directory entries during verification. Use a private,
unchanging capture directory and do not include credentials or licensed SDKs.

## Record the three performance workloads

A performance capture includes a `workloads` object with exactly these keys:
`hello-world`, `list-1000` and `drawer-drag`.

For an unavailable workload, record its actual status rather than invented
measurements:

```json
{
  "status": "blocked",
  "reason": "The physical gesture path has not been qualified."
}
```

`not-run` also requires a `reason`. A recorded workload uses `status: "recorded"`
and these fields:

- `clock: "monotonic"`; `frameBoundary`: `presented`, `queued` or `js-callback`;
  `refreshHz`: the measured display refresh rate, not an assumed 60 Hz.
- `startupMs`: startup-to-first-frame samples; `frameIntervalsMs`: intervals
  from the named frame boundary; `rssBytes`: resident-memory samples in bytes.
- `warmupIntervals`: the number of initial frame intervals to exclude.
  At least one measured interval must remain after exclusion.

Each sample array has 1-100,000 positive finite values; RSS must be integer
bytes. The report includes sample counts, min/max, and nearest-rank p50/p95/p99.
It derives the interval budget as `1000 / refreshHz` and counts intervals
strictly above that budget. That count is **not** a missed-vsync or dropped-frame
measurement. The script does not invent a pass/fail performance threshold or
statistical confidence from a small sample.

Only a `qnx-physical` capture with all three workloads recorded at the
`presented` boundary can have `captureComplete: true`. Queued-frame and JavaScript
callback intervals remain labelled and incomplete for presentation evidence.
Qt's [frameSwapped documentation](https://doc.qt.io/qt-6/qquickwindow.html#frameSwapped)
describes a frame queued for presentation; do not rename that signal to physical
presentation without separate instrument evidence.

Startup, frame intervals and RSS are summarized independently. Keep trial
pairing, clock conversion, collection duration and raw traces in the capture
method and referenced artifacts. This first reporter does not collect those
measurements or make comparative performance claims.

## Record supervised cold boots

A cold-boot capture includes a nonempty `recoveryProcedure` and an `attempts`
array. Preserve every observed attempt, including failures; never remove failed
boots to manufacture a passing sequence.

Each attempt records `sequence` starting at 1 without gaps; a unique `bootId`;
`startedAt` and `completedAt` as UTC timestamps including milliseconds;
`reset` as `power-cycle`, `reboot` or `app-relaunch`; `status` as `passed`,
`failed` or `blocked`; boolean `graphicsReady`, `appReady` and
`recoveryAvailable`; and `log`, naming a file in the hashed artifacts array.
A failed or blocked attempt also requires a nonempty `reason`.

Timestamps must be ordered and non-overlapping. Passing attempts must report
all three readiness observations. Duplicate boot identities are rejected.
Only 20 or more consecutive **final power-cycle** successes on `qnx-physical`
make the capture structurally complete. Reboots, application relaunches and
failed/blocked attempts reset the sequence. They never count as cold boots.
A shared aggregate log may support several attempts, but the reviewer must be
able to find each boot identity and observation inside it.

This command never powers off, flashes or changes graphics startup. Perform
boot tests only under the existing supervised board/recovery runbook.

## Review before changing a release gate

Every report returns `releaseGatePassed: false`, even for complete captures.
`captureComplete` means the declared observations satisfy the schema and count
rules, not that they are authentic, sufficiently sampled or production-ready.
Confirm hashes, board/image, runtime closure, raw logs and actual test scope
before a reviewer changes the [v1 readiness record](../releases/v1-readiness.md).
The command never edits that record or treats host evidence as QNX proof.

Keep stock Pi 4 QSTI and the custom Pi 400 image in separate captures. No
performance number or boot success in the unit-test fixtures is a hardware
result; those files use explicitly synthetic data.
