# Ordered Work Queue

## Active — G0-A toolchain truth

- [x] Obtain access to a supported x86-64 QNX SDP 8.0 development host with a
      deployed QNX Everywhere license (Windows host with SDP 8.0 at
      `C:\Users\Owner\qnx800`).
- [x] Capture the installed SDP/package identity and complete `qcc -V`/`q++ -V`
      output without license or account data (recorded in `artifacts/g0/`).
- [x] Audit the installed compiler target against `gcc_ntoaarch64le`.
- [x] Cross-build the concepts/ranges/designated-initializer/`std::jthread`
      probe with the `qnx-pi4` preset; `std::jthread` is unsupported in this SDP's
      libc++, so the probe falls back to `std::thread` and reports the gap.
- [ ] Run that exact binary on the physical Pi 4 and record its output.

## Blocked behind G0-A — G0-D device loop

- [ ] Record the current QSTI image package/build and SDP compatibility.
- [ ] Document a named SSH user and key setup; do not assume `root` access.
- [ ] Deploy, launch, and collect stdout plus `slog2info` through the checked-in
      device script.
- [ ] Measure a rebuilt hello-world round trip below 30 seconds.
- [ ] Repeat the smoke path on the licensed QEMU QSTI environment.

## Blocked behind G0-D — Qt decisions

- [ ] Resolve the Qt-for-QNX commercial-license wording against the proposed
      LGPLv3 source-build posture before downloading or distributing Qt for QNX.
- [ ] Select an exact Qt 6 LTS tag only after it builds with the installed SDP.
- [ ] Verify the pinned QNX QPA and mkspec/toolchain source paths.
- [ ] Complete on-glass G0-B evidence and then replace ADR-0001.
- [ ] Complete the G0-C item-instantiation source audit and Pi benchmark.

## Governance inputs required

- [ ] Supply verified GitHub users/team slugs for CODEOWNERS.
- [ ] Supply the permanent npm organization replacing provisional `@rn-qnx`.
- [x] Resolve the specification's Nitro conflict: retain the registry-current,
      mutually coherent 0.36.5/0.36.5 pair under ADR-0008.
- [ ] Define actual package manifests/release units before enabling
      release-please or per-package changelogs.
