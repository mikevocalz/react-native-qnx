# Bundled React render probe

The previous QNX probe constructed a host/UIManager and evaluated `6 * 7`.
That could pass without a React component, Fabric commit, or mounted Qt item.
The replacement builds one production Metro bundle and uses the same C++ source
on host and QNX. It verifies:

1. The registered `RNQnxReactRenderProbe` component starts a real React surface.
2. A Fabric transaction produces one 160×100 Qt item with a 48×24 child at
   (12, 16), with the fixture's background colors.
3. A real React `useState` update produces another transaction, changes the
   child to 72×24 at (40, 16), and changes its background color.
4. Stopping the surface produces another transaction and removes the items.

Each asynchronous stage has a five-second deadline. JS exceptions, host errors,
mount failures, incorrect geometry, and missing/empty/throwing bundles fail the
process; only the completed sequence emits `RNQNX_REACT_RENDER_OK`.

## Evidence boundary

This is **geometry/property evidence**, not a pixel, scene-graph, on-glass,
QNX Screen, or physical-device result. The output explicitly labels
`platform=host` or `platform=qnx` according to the compiled platform and includes
`geometry-only`. `validate:qnx-react` checks source contracts only; the new
Metro CI step proves bundling only. Neither is runtime evidence.

The exact Qt/Hermes/RN host compile and runtime check is still pending for this
change until the separate current-line host-renderer build work succeeds.
A licensed QNX SDP 8.0.5 cross-build and execution on the actual Pi remain
separate pending gates. No target output is checked in or fabricated.

## Host run

Use Node/pnpm from `package.json`, CMake ≥3.31.9, and Qt 6.11.1 (Core, Gui, Qml,
Quick) with its development prerequisites. Hermes and RN dependencies are the
repository's existing pinned source builds.

```sh
pnpm install --frozen-lockfile
cmake -S . -B build/react-probe -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DRNQNX_BUILD_HERMES=ON -DRNQNX_FETCH_REACT_NATIVE_DEPS=ON \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.1/gcc_64
cmake --build build/react-probe --target rnqnx_react_render_host_probe
ctest --test-dir build/react-probe --output-on-failure \
  -R '^rnqnx_react_render_host_probe'
```

A host without Qt skips the optional graph on the current main branch; therefore
success of the basic two-test C++ smoke suite proves nothing about this probe.
The explicit build target above must exist, and CTest must list the render and
bad-bundle tests. The bad-bundle test checks nonzero exit, the matching error
marker, and absence of the success marker, rather than accepting crashes.

## Later QNX run

Enable the existing `RNQNX_BUILD_REACT_QNX` and `RNQNX_BUILD_HERMES_QNX` graph
using the licensed SDP and matching Qt target installation. Building
`rnqnx_react_qnx_probe` also builds `rnqnx-react-probe.bundle.js` with host Metro.
Deploy **both** the executable and the bundle, together with the required Qt,
Hermes and QPA runtime. The QNX binary intentionally has no baked-in host path:

```sh
./rnqnx_react_qnx_probe --bundle ./rnqnx-react-probe.bundle.js
```

Use the real target's configured QPA backend. Save the exact commit, bundle
SHA-256, command, exit code, stdout/stderr, SDP/QSTI identity and board identity.
A QNX geometry pass still does not replace an on-glass Qt Quick/pixel gate.
