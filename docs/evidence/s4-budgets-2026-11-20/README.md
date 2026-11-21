# S4 receipt: performance budgets, host lane, timing probe, image-load investigation

Date: 2026-11-20. Base: `746f52d`. Branch `s4-budgets`, PR #194. Slice S4 feeds the
`performance` gate. **The gate stays `pending`.** Nothing here was measured on a
QNX VM, a Pi 400 or the release board.

## What this slice added

| Item                                                                                       | Path                                                                                                                           |
| ------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------ |
| Budgets file: Hello World, 1000-row list, drawer drag; host, QNX VM, Pi 400, release board | `docs/releases/performance-budgets.json`                                                                                       |
| Validator: schema check, baseline derive/apply, host regression check                      | `scripts/validate-budgets.mjs`, `tests/js/performance-budgets.test.mjs`                                                        |
| Host lane workflow, labelled host                                                          | `.github/workflows/performance-budgets.yml`                                                                                    |
| Timing probe (`--timing-json`), default OFF, Linux and macOS only                          | `vnext/probes/playground/TimingProbe.{h,cpp}`, `main.cpp`, `CMakeLists.txt` (`RNQNX_ENABLE_TIMING_PROBE`)                      |
| Probe contract (the `api-design` and `build-nitro-modules` output)                         | `docs/nitro/API_CONTRACTS.md`, section "S4 frame and startup timing probe"                                                     |
| Probe test under ASan and UBSan                                                            | `tests/native/timing-probe/`                                                                                                   |
| Image-load stage probe and its server                                                      | `apps/performance-lab/image-probe.js`, `image-probe.config.json`, `assets/probe-640x427.jpg`, `scripts/image-probe-server.mjs` |
| Clock and mount logging in the gallery card                                                | `apps/component-gallery/RemoteImageCard.js`                                                                                    |

## Which numbers are measured and which are not

| Lane                                                                              | Hello World                 | 1000-row list                                           | Drawer drag                                                                                                                                          |
| --------------------------------------------------------------------------------- | --------------------------- | ------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| host (GitHub ubuntu-24.04, Qt 6.11.1, offscreen, software, Release, base runtime) | measured: startup, peak RSS | measured: startup, frame interval p95 and max, peak RSS | blocked: `apps/performance-lab/fixtures.js` says the gesture and Worklets path is not qualified, and a scripted position change would not qualify it |
| QNX VM                                                                            | unmeasured                  | unmeasured                                              | unmeasured                                                                                                                                           |
| Pi 400                                                                            | unmeasured                  | unmeasured                                              | unmeasured                                                                                                                                           |
| release board                                                                     | unmeasured                  | unmeasured                                              | unmeasured                                                                                                                                           |

Hello World has no frame-interval budget because it does not animate. The probe has no
QNX reader (CMake rejects the option on QNX), so every device and VM field holds
`status: unmeasured` and a reason, with no number. The validator rejects a number in such a
field, and rejects an `unmeasured` host field while the host lane is enforced.
`tests/js/performance-budgets.test.mjs` covers both.

Known constraint recorded in the budgets file: `HERMESVM_ALLOW_JIT=0`. Hermes is an
interpreter in every build here (`docs/handoff/HERMES_QNX.md`, and
`HERMESVM_ALLOW_JIT:STRING=0` in
`docs/evidence/pr149-on-native-build-2026-11-14/CMakeCache.final.txt`). Every budget was
measured without a JIT.

## Host baseline and ceilings (host lane, enforced)

Source: GitHub Actions run 44849611367 at `cc20adfe`, job "Host lane, startup, frame swaps and
RSS". Runner: ubuntu-24.04, 4 CPUs, Intel Xeon Platinum 8573C. Qt 6.11.1, offscreen platform,
software backend, Release, base runtime (feature flags OFF), `RNQNX_ENABLE_TIMING_PROBE=ON`.
20 runs of each workload, no dropped frames. Raw files and bundle hashes:
`ci-host-run-44849611367/`.

| Workload      | Metric                                        | min   | p50   | max   | Ceiling (`ceil(max * 1.5)`) |
| ------------- | --------------------------------------------- | ----- | ----- | ----- | --------------------------- |
| Hello World   | startup to first frame (ms)                   | 100.1 | 101.3 | 103.4 | 156                         |
| Hello World   | peak RSS (KiB)                                | 71264 | 71652 | 71840 | 107760                      |
| 1000-row list | startup to first frame (ms)                   | 157.7 | 160.1 | 164.7 | 248                         |
| 1000-row list | frame interval p95 over the wheel window (ms) | 18.49 | 18.65 | 19.54 | 30                          |
| 1000-row list | frame interval max over the wheel window (ms) | 19.72 | 21.63 | 21.92 | 33                          |
| 1000-row list | peak RSS (KiB)                                | 96248 | 96596 | 96908 | 145362                      |

The check compares the median of the 20 runs with the ceiling. Run on the same files:
`node scripts/validate-budgets.mjs --check-run docs/evidence/s4-budgets-2026-11-20/ci-host-run-44849611367`
prints six `ok` rows (medians 101.3 ms, 71664 KiB, 160.2 ms, 18.66 ms, 21.64 ms and 96618 KiB).

The ceiling is `ceil(baseline.max * 1.5)`. The factor 1.5 is a method choice made here, not a
measurement. It leaves room for runner noise. A ceiling changes only through an ADR and the
owner, and `--apply-baseline` refuses to raise one that already holds a value.

A swap is the end of a scene-graph swap on the offscreen platform, not a display
presentation. Frame intervals are the gaps between swaps that fall between the first and the
last of 40 posted wheel notches, 100 ms apart.

What a failing check looks like is covered by `tests/js/performance-budgets.test.mjs`: a
median of 400 ms against a 156 ms ceiling fails the lane and names the workload and metric.

## Host timing: macOS development machine (informational, not a budget)

Twenty runs of each workload with the same probe on an Apple M3 Pro, macOS 27.0, Qt 6.12.0
(Homebrew), offscreen platform, software backend, Release, with other build jobs running on the
machine. Raw files: `host-macos-runs/`. This is not the enforced lane and sets no ceiling. It
shows the probe running on a second host and operating system.

| Workload      | Metric                                        | min    | p50    | max    |
| ------------- | --------------------------------------------- | ------ | ------ | ------ |
| Hello World   | startup to first frame (ms)                   | 282.3  | 294.5  | 723.9  |
| Hello World   | peak RSS (KiB)                                | 86640  | 86960  | 87280  |
| 1000-row list | startup to first frame (ms)                   | 328.8  | 349.4  | 511.1  |
| 1000-row list | frame interval p95 over the wheel window (ms) | 19.8   | 21.7   | 23.1   |
| 1000-row list | frame interval max over the wheel window (ms) | 23.4   | 28.3   | 38.2   |
| 1000-row list | peak RSS (KiB)                                | 113328 | 113872 | 115856 |

Every list run stored 256 to 262 swaps and dropped none.

## What CI found along the way

The first Linux run of the probe test failed on `rssSource`: `QFile::atEnd()` is true at once on
a procfs file, so the Linux reader found no `VmRSS`. The reader now reads `/proc/self/status`
in one call (commit "read /proc/self/status whole"). Nothing on macOS could have shown this.

## Image-load investigation (RemoteImageCard)

### The raw numbers

| Date       | Source                              | Value         | Where                                                                                     |
| ---------- | ----------------------------------- | ------------- | ----------------------------------------------------------------------------------------- |
| 2026-11-13 | Pi 400, same URL                    | 654 ms        | `docs/evidence/worklets-frame-2026-11-13/device-before-flush.txt`                         |
| 2026-11-13 | Pi 400, same URL                    | 677 to 721 ms | stated in `docs/evidence/pr149-pi400-run2-2026-11-19/README.md`; those logs were not kept |
| 2026-11-13 | Pi 400, same URL                    | 338 to 688 ms | `docs/image-loader.md`                                                                    |
| 2026-11-19 | Pi 400 key-hold run                 | 30905 ms      | `docs/evidence/pr149-pi400-keyhold-2026-11-19/app.txt`                                    |
| 2026-11-19 | Pi 400 run 2, first line            | 154149 ms     | `docs/evidence/pr149-pi400-run2-2026-11-19/app.txt`                                       |
| 2026-11-19 | Pi 400 run 2, second line, same run | 618203 ms     | same file                                                                                 |

### What the number measures

From `apps/component-gallery/RemoteImageCard.js` as it stood before this PR: `startedAt` is
`Date.now()` at mount and again whenever `uri` or `attempt` changes. `onLoad` logs
`Date.now() - startedAt` and does not reset it. So the figure is wall-clock time from mount to an
`onLoad` callback. It is not a fetch time, and a second `onLoad` on the same mount reports a
larger figure measured from the first mount. Run 2 logged two values for one URL in one run, so
the log cannot tell whether those were two loads or one mount reporting twice. Run 2 started at
07:39:26 UTC and stopped at 07:52:30 UTC (784 s, from its `run.txt`), so 618203 ms fits inside the
run. 30905 ms is 905 ms above 30000 ms, and the image fetcher is constructed with a 30000 ms
timeout (`vnext/src/runtime/QtImageCache.cpp:12`, `vnext/include/react-native-qnx/runtime/QnxImageLoader.h:33`).
This is arithmetic on the logs and a reading of the code. It names no cause.

### What this slice measured

`apps/performance-lab/image-probe.js` times, on one JS clock pair (`performance.now()` and
`Date.now()`), three stages per attempt on a URL no earlier stage touched, while a 50 ms
heartbeat timer records how late the JavaScript task queue drains:

- `prefetch`: `Image.prefetch(url)`, which is fetch, decode and the completion reaching JS
- `getSizeHit`: `Image.getSize(url)` right after, one JS to native to JS round trip with the
  decoded image already cached
- `onLoad`: `<Image onLoad>` from mount, the number the card reports

`scripts/image-probe-server.mjs` logs, per request, when it began and how long it took to send,
on its own clock. Host, macOS (same machine and caveat as above), loopback, 5 attempts. Raw
files: `image-probe/`.

| Server delay | prefetch (ms)                               | getSizeHit (ms) | onLoad (ms)                   | Max JS queue lateness (ms)      |
| ------------ | ------------------------------------------- | --------------- | ----------------------------- | ------------------------------- |
| 0            | 246.3 (cold first attempt), then 2.1 to 3.4 | 0.1             | 76.9 (first), then 2.5 to 2.9 | 166.5 (first attempt), then 0.0 |
| 2000         | 2005 to 2027                                | 0.1             | 2003 to 2022                  | 2.6 to 6.5                      |

What the host run shows about the code on `main` at this commit, on this host:

- With a server that waits 2000 ms, `prefetch` and `onLoad` take 2003 to 2027 ms. Fetch time moves
  the number one for one. Decode plus JS drain add about 3 to 27 ms (the high end is the cold first
  attempt).
- Decode and the JS task drain do not take seconds. The JS queue was late by at most 6.5 ms after
  the first attempt. The first attempt was late by 166.5 ms while other builds were running on the
  machine.
- A wall clock step would show up as a gap between the two clocks. The gap stayed within 0.6 ms in
  every logged attempt.
- `Image.getSize` on a cached URL costs 0.1 ms. The server saw each loader URL twice in a first run
  because that server sent `no-store`, which the loader will not retain (`docs/image-loader.md`).
  The server now sends `max-age`, and the table above is from that server.

### What is not established

The cause of 30 to 618 s on the Pi 400 is **not established**. Nothing in the existing evidence
separates fetch, decode and JS-task drain on the board, and the board is offline. The host run says
the code path has no multi-second decode or JS stall on a fast host. It cannot say whether the
board's network path (DNS through the Mac's connection sharing, TLS to `images.unsplash.com`), its
clock, its CPU load or a repeated `onLoad` explains the spread. `nitro-fetch` was not used: the
investigation did not reach a network layer this repo replaces, and no QNX support claim exists for
it.

### What the board must measure (S9)

Run all four on the same image, with a board build from `main` after this PR:

1. `apps/performance-lab/image-probe.js`, with `image-probe.config.json` pointing at
   `scripts/image-probe-server.mjs` on the Mac. It prints the three stage times, the JS queue
   lateness and both clocks per attempt, and the server log gives the fetch share from the other
   end. Run it with the server `--delay-ms` at 0 and at 2000.
2. The gallery with the updated card. It now logs the monotonic time, the mount id and the `onLoad`
   count, so a second `onLoad` on one mount, or a wall clock step, shows in the log.
3. On the board shell, before and after the run: `date` and uptime, to see a wall clock step against
   the monotonic clock.
4. Name resolution and TCP connect time to `images.unsplash.com` from the board at the same moment,
   to compare with the load time.

## Review and skill record

See the PR description for the skill table and the review record.
