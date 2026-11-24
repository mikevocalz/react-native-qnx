# ADR-0025: Pi 400 performance budgets, proposed from the 2026-11-24 runs

## Status

Proposed, 2026-11-24. Owner: Mike. Nothing in this ADR is applied. `docs/releases/performance-budgets.json` keeps every `pi400`
field at `unmeasured`, and the `performance` gate in `docs/releases/v1-readiness.json` stays `pending`. The status moves to Accepted
only when Mike says so, and the pull request that Accepts it carries the budget edit below and no other change.

## Context

[ADR-0023](0023-pi400-stands-in-for-device-parity.md) makes the Pi 400 the board for every gate. The S4 receipt
([receipt](../evidence/s4-budgets-2026-11-20/README.md)) set host ceilings by one rule and left every `pi400`, `qnx-vm` and
`release-board` field `unmeasured`, because the timing probe had no QNX reader. The QNX reader was written on 2026-11-23, and on 2026-11-24
the board ran 20 processes of each of three workloads
([receipt](../evidence/pi400-v1-gates-2026-11-24/README.md), section Performance).

Labels in this ADR: **Pi 400** is `qnxuser@192.168.2.2`, QNX 8.0.0 `RaspberryPi400 aarch64le`. **host** is a development or CI
machine. **QNX VM** is the QEMU target. Nothing here was measured on the QNX VM or the host, and nothing ran on a stock Pi 4 QSTI.

The runs: `org.rnqnx.playground` `279e300f...` (Release `-O2`, Qt 6.12.0, `opengl` render backend, Hermes as an interpreter with
`HERMESVM_ALLOW_JIT=0`), the timing probe on, one process per run, no warm-up run discarded, 60 result files in total.

## Method

The repository's rule is the one in `performance-budgets.json` and `scripts/validate-budgets.mjs`: at least 20 runs
(`method.baselineMinRuns`), then `ceiling = ceil(baseline.max * 1.5)` (`method.regressionFactor`, nearest-rank percentiles). A check
compares the median of a run set with the ceiling.

**The factor 1.5 is a method choice.** The S4 receipt says the same about the host lane: it leaves room for noise and is not derived
from a measurement. Twenty runs on one afternoon, on one board, from one build do not show how much the Pi 400 varies across boots,
temperatures, SD cards or builds. A different factor is a different decision, and Mike can ask for one when accepting.

### Recomputed from the raw files

I did not read the numbers from the receipt prose. A Python script read every `*.json` under the three directories below, rejected
any file with `droppedFrames` above 0, a platform other than `qnx`, or fewer than 30 swaps in the input window, and applied the
validator's rules: startup is `firstFrameMs`, RSS is `peakRssKiB`, and frame intervals are the gaps between `frameSwapped` instants
that fall between the first and last input (`wheelFirstMs` to `wheelLastMs` for the list, `inputFirstMs` to `inputLastMs` for the
observed drag). Then `node scripts/validate-budgets.mjs --derive` ran on a copy of the same files, and its output is byte-identical to
the committed `perf/derive.json`. The two computations agree on every min, p50 and max below.

| Workload (Pi 400, 20 runs)                | Raw directory                                           | Metric                           | min     | p50 (nearest rank) | p95     | max     | Ceiling `ceil(max * 1.5)` |
| ----------------------------------------- | ------------------------------------------------------- | -------------------------------- | ------- | ------------------ | ------- | ------- | ------------------------- |
| Hello World                               | `perf/hello-world/hello-world-N.json`                   | startup to first frame, ms       | 645.040 | 688.717            | 812.458 | 836.427 | 1255                      |
| Hello World                               |                                                         | peak RSS, KiB                    | 118596  | 118696             | 118904  | 118904  | 178356                    |
| 1000-row list, 40 wheel notches at 100 ms | `perf/list-1000/list-1000-N.json`                       | startup to first frame, ms       | 700.469 | 752.921            | 794.142 | 841.279 | 1262                      |
| 1000-row list                             |                                                         | frame interval p95 in window, ms | 43.546  | 44.835             | 46.148  | 47.074  | 71                        |
| 1000-row list                             |                                                         | frame interval max in window, ms | 48.980  | 56.399             | 61.063  | 75.436  | 114                       |
| 1000-row list                             |                                                         | peak RSS, KiB                    | 139336  | 140620             | 141620  | 141728  | 212592                    |
| Drawer drag (not proposed, see below)     | `perf/drawer-drag-observed/drawer-drag-physical-N.json` | startup to first frame, ms       | 717.963 | 739.902            | 767.833 | 794.751 | 1193                      |
| Drawer drag                               |                                                         | frame interval p95 in window, ms | 33.397  | 33.404             | 33.874  | 33.972  | 51                        |
| Drawer drag                               |                                                         | frame interval max in window, ms | 45.619  | 47.083             | 47.843  | 50.065  | 76                        |
| Drawer drag                               |                                                         | peak RSS, KiB                    | 121448  | 121796             | 122220  | 122276  | 183414                    |

The p95 column is the p95 across the 20 per-run values of each metric, shown for spread. The validator does not use it. The true
median of 20 values (what a check compares) is the mean of the 10th and 11th values and differs slightly from the nearest-rank p50:
688.909 against 688.717 for Hello World startup, 56.849 against 56.399 for list frame-interval max, 140754 against 140620 KiB for
list RSS. No ceiling above depends on it.

All 60 files have `droppedFrames` 0 and `renderBackend` `opengl`. The list files have `inputKind` `posted-wheel`; the drawer files have
`observed-pointer` (the probe posted nothing and recorded when `rnqnx-inject` pointer events reached the window).
The 20 `drawer-drag-posted-no-motion` files are not used: the drawer did not move in them.

## What has a measured Pi 400 baseline

- Hello World: startup to first frame and peak RSS. Frame intervals are `not-applicable` (the workload does not animate) and stay so.
- 1000-row list: startup to first frame, frame interval p95, frame interval max, peak RSS.

Raw data for both is present, 20 runs each. These six fields are what the patch below fills.

## What is blocked or missing

- **Drawer drag: nothing proposed.** The raw data exists (20 observed-pointer runs), so the numbers are in the table for reference.
  The budgets file marks the workload `blocked` on the host lane, and gives this reason for all four metrics: the workload needs
  the qualified Gesture Handler and Worklets path, and a scripted position change would not qualify it. The S4 receipt says the same.
  That text sits on the host lane only. Its Pi 400 fields say `unmeasured`, and `apps/performance-lab/fixtures.js` now describes the
  drawer as the shipped `JsDrawer` (PanResponder edge strip, Animated progress), dragged by pointer events, with `available-source`
  status. The two statements disagree, and I am not resolving it here. Mike decides whether the old reason still applies. If it does
  not, a follow-up ADR or an amendment to this one adds the four drawer fields from the table (ceilings 1193 ms, 51 ms, 76 ms,
  183414 KiB).
  A second reason to hold it back: a drawer-drag run depends on timed pointer events from `rnqnx-inject` through Screen, and
  its window (about 3 s, 100 moves of 30 ms) is shorter and shaped differently from the list's wheel window. Its p95 of 33.4 ms in all
  20 runs, with a spread of 0.6 ms, is two swap intervals at 60 Hz. That tightness reads like a pacing effect of the injector, so
  a ceiling derived from it may say more about the injector than about the drawer. This is a reading of the numbers, not a finding.
- **`qnx-vm` and `release-board`:** no measurement. Untouched.
- **Peak RSS on the Pi 400 is a sample.** The QNX reader samples the resident set every 100 ms and reports the highest sample
  (`rssSource` `sampled-procfs`). The host lane reads the OS high-water mark. A short peak between samples is missed, so
  `baseline.max` may sit below the true peak, and a 1.5 factor applied to it carries that gap into the ceiling.

## Decision (proposed)

Add six `pi400` budgets to `docs/releases/performance-budgets.json` with the method above and the baselines in the table:

| Field (Pi 400)                      | Ceiling    |
| ----------------------------------- | ---------- |
| hello-world, startup to first frame | 1255 ms    |
| hello-world, peak RSS               | 178356 KiB |
| list-1000, startup to first frame   | 1262 ms    |
| list-1000, frame interval p95       | 71 ms      |
| list-1000, frame interval max       | 114 ms     |
| list-1000, peak RSS                 | 212592 KiB |

The ceilings are loose by design of the 1.5 rule: the list p95 ceiling (71 ms) is 1.5 times the worst of 20 runs and about 1.6 times
the median run. They catch a large regression, not a small one.

### What this ADR does not decide

- It does not flip the `performance` gate. The gate text asks for presented-frame intervals, and the 2026-11-24 receipt records that
  HDMI output was black, the display was not observed, and a swap is the end of a scene-graph swap, not a presented frame. It also
  asks for drawer drag.
- It does not set a drawer-drag budget (see above), any `qnx-vm` or `release-board` budget, or a different regression factor.
- It does not add a Pi 400 regression check to CI. `--check-run` only reads the host lane (`validate-budgets.mjs`, `checkRun`),
  and no runner drives the board. A Pi 400 ceiling in the file is a recorded number until a supervised run compares against it.
- It does not claim the Pi 400 numbers carry to a stock Pi 4 QSTI, a different SD card, or a build with the JIT on
  (`knownConstraints` `hermes-no-jit` already requires a new measurement and an ADR for that).
- It does not say the startup figures are good. They are about 250 ms above the 2026-11-23 numbers because this binary carries the
  title bar, the Nitro windows API and AppState ([receipt](../evidence/pi400-v1-gates-2026-11-24/README.md)).

## The patch if Mike accepts

`validate-budgets.mjs --apply-baseline <dir> --source <text>` is the repository's way to write a baseline and the ceiling.
**It cannot write these fields.** Reading `applyBaseline` in the script: it writes only `budgets.host`, so pointed at the Pi 400
files it would try to replace the host ceilings (156 ms for Hello World startup) with Pi 400 values and stop with
`refusing to raise ceiling 156 to 1255; that needs an ADR and the owner`. I ran it that way on a scratch copy to confirm: exit 1,
file unchanged. Writing the Pi 400 lane with the script needs a lane option in the script, which is a code change and not part of
this docs-only pull request. Until it exists, the edit is made by hand from this patch (RFC 6902, paths into the budgets file).
Values come from `--derive` on the receipt files and ceilings from the script's own `ceilingFor`.

```json
[
  {
    "op": "replace",
    "path": "/workloads/hello-world/metrics/startupToFirstFrameMs/budgets/pi400",
    "value": {
      "status": "measured",
      "baseline": {
        "runs": 20,
        "min": 645.04,
        "p50": 688.717,
        "max": 836.427,
        "source": "Pi 400: org.rnqnx.playground 279e300f, QNX 8.0.0 RaspberryPi400 aarch64le, Qt 6.12.0 QNX Screen opengl, Release -O2, Hermes interpreter (HERMESVM_ALLOW_JIT=0), 20 runs per workload, one process per run, docs/evidence/pi400-v1-gates-2026-11-24/perf"
      },
      "ceiling": 1255
    }
  },
  {
    "op": "replace",
    "path": "/workloads/hello-world/metrics/peakRssKiB/budgets/pi400",
    "value": {
      "status": "measured",
      "baseline": {
        "runs": 20,
        "min": 118596,
        "p50": 118696,
        "max": 118904,
        "source": "Pi 400: org.rnqnx.playground 279e300f, QNX 8.0.0 RaspberryPi400 aarch64le, Qt 6.12.0 QNX Screen opengl, Release -O2, Hermes interpreter (HERMESVM_ALLOW_JIT=0), 20 runs per workload, one process per run, docs/evidence/pi400-v1-gates-2026-11-24/perf"
      },
      "ceiling": 178356
    }
  },
  {
    "op": "replace",
    "path": "/workloads/list-1000/metrics/startupToFirstFrameMs/budgets/pi400",
    "value": {
      "status": "measured",
      "baseline": {
        "runs": 20,
        "min": 700.469,
        "p50": 752.921,
        "max": 841.279,
        "source": "Pi 400: org.rnqnx.playground 279e300f, QNX 8.0.0 RaspberryPi400 aarch64le, Qt 6.12.0 QNX Screen opengl, Release -O2, Hermes interpreter (HERMESVM_ALLOW_JIT=0), 20 runs per workload, one process per run, docs/evidence/pi400-v1-gates-2026-11-24/perf"
      },
      "ceiling": 1262
    }
  },
  {
    "op": "replace",
    "path": "/workloads/list-1000/metrics/frameIntervalP95Ms/budgets/pi400",
    "value": {
      "status": "measured",
      "baseline": {
        "runs": 20,
        "min": 43.54600000000028,
        "p50": 44.835000000000036,
        "max": 47.074000000000524,
        "source": "Pi 400: org.rnqnx.playground 279e300f, QNX 8.0.0 RaspberryPi400 aarch64le, Qt 6.12.0 QNX Screen opengl, Release -O2, Hermes interpreter (HERMESVM_ALLOW_JIT=0), 20 runs per workload, one process per run, docs/evidence/pi400-v1-gates-2026-11-24/perf"
      },
      "ceiling": 71
    }
  },
  {
    "op": "replace",
    "path": "/workloads/list-1000/metrics/frameIntervalMaxMs/budgets/pi400",
    "value": {
      "status": "measured",
      "baseline": {
        "runs": 20,
        "min": 48.98000000000047,
        "p50": 56.39900000000034,
        "max": 75.43600000000015,
        "source": "Pi 400: org.rnqnx.playground 279e300f, QNX 8.0.0 RaspberryPi400 aarch64le, Qt 6.12.0 QNX Screen opengl, Release -O2, Hermes interpreter (HERMESVM_ALLOW_JIT=0), 20 runs per workload, one process per run, docs/evidence/pi400-v1-gates-2026-11-24/perf"
      },
      "ceiling": 114
    }
  },
  {
    "op": "replace",
    "path": "/workloads/list-1000/metrics/peakRssKiB/budgets/pi400",
    "value": {
      "status": "measured",
      "baseline": {
        "runs": 20,
        "min": 139336,
        "p50": 140620,
        "max": 141728,
        "source": "Pi 400: org.rnqnx.playground 279e300f, QNX 8.0.0 RaspberryPi400 aarch64le, Qt 6.12.0 QNX Screen opengl, Release -O2, Hermes interpreter (HERMESVM_ALLOW_JIT=0), 20 runs per workload, one process per run, docs/evidence/pi400-v1-gates-2026-11-24/perf"
      },
      "ceiling": 212592
    }
  }
]
```

Apply it (host, from the repository root, after Accepting this ADR):

```sh
node scripts/validate-budgets.mjs --derive docs/evidence/pi400-v1-gates-2026-11-24/perf-set   # confirm the baseline numbers
# apply the six replace operations above to docs/releases/performance-budgets.json, then:
node scripts/validate-budgets.mjs --check-file
node --test tests/js/performance-budgets.test.mjs
```

`perf-set` stands for a directory that holds the 20 `hello-world-N.json`, the 20 `list-1000-N.json` files and the 20 drawer files
(`--derive` reads files named `<workload>-<n>.json`, and `drawer-drag-physical-N.json` matches `drawer-drag-`). I applied the patch to
a scratch copy of the budgets file and ran `checkBudgetsFile` on it: no problems. I did not edit the committed file.

## Consequences

- If accepted, six `pi400` fields hold a baseline, a source line and a ceiling. `checkBudgetsFile` enforces the 20-run minimum, the
  ceiling formula and a source line that names `Pi 400` for each of them.
- The `performance` gate stays `pending` for the reasons above. The patch gives it numbers and does not satisfy its text.
- `lanes.pi400.description` still says "Not measured." The same pull request that applies the patch should reword it. That is a
  text edit the script does not touch.
- If rejected, nothing changes and the 2026-11-24 files stay as the receipt.

## Alternatives considered

- **A factor other than 1.5.** Rejected for now: the repository has one rule, and a Pi 400 factor with no variance data behind it
  would be a second unmeasured choice. A repeat on another day would give a spread to size it from.
- **Use the median of the runs as the ceiling base.** Rejected: the rule is `baseline.max`, and changing it changes the host lane's
  meaning too.
- **Include drawer drag now.** Held back for the two reasons above.
- **Wait for 20 more runs.** Possible, and cheap on the board. It would show whether the max moves; it does not change the method.

## Related files

- `docs/releases/performance-budgets.json`
- `docs/releases/v1-readiness.json`
- `scripts/validate-budgets.mjs`
- `docs/evidence/pi400-v1-gates-2026-11-24/README.md`
- `docs/evidence/pi400-v1-gates-2026-11-24/perf/derive.json`
- `docs/evidence/s4-budgets-2026-11-20/README.md`
- `apps/performance-lab/fixtures.js`

## Notes

The raw files are in the repository, so every number can be recomputed with `--derive`. Open items for Mike: accept, reject or change
the 1.5 factor; decide whether the drawer-drag block still applies on the Pi 400; decide whether the script gets a `--lane` option.
