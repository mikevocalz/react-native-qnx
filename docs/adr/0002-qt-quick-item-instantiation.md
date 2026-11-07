# ADR-0002: Qt Quick built-in-item instantiation

## Status

Provisionally accepted on host evidence (2026-08-14). The pinned Qt 6.12.0
private evaluation now has scoped Pi 400 Hello World proof, recorded in
[ADR-0001](0001-qt-quick-backend.md) and the
[device receipt](../evidence/pi400-hello-world-2026-11-07.txt). G0-C's 1,000-item
Pi benchmark and the remaining source/ownership acceptance items below stay open.

## Context

Fabric needs to create, update, and destroy native component items. The specification
identifies a source question that cannot be answered from class names alone:
Qt Quick's built-in visual types may require QML type registration or private
implementation classes even though the public host surface includes
`QQuickItem`, `QQuickWindow`, and scene-graph nodes.

## Candidate decision

The default candidate is one cached `QQmlComponent` per React Native component
type, compiled once from an inline type declaration. The mounting layer would
instantiate it and mutate the resulting public object through verified
properties/metaobjects. QML would be only the type source, not the authored UI
or layout authority.

A custom `QQuickItem`/scene-graph implementation is the fallback for a component
where the cached-component path cannot meet measured control or performance
requirements. Text must not choose either path until the pinned source shows
which public text/scene-graph facilities exist.

## Acceptance evidence required before acceptance

1. Exact Qt source tag selected by G0-B.
2. Pinned `qtdeclarative` file/symbol citations for registration, component
   creation, ownership, property mutation, and destruction.
3. Confirmation that the selected path includes no unapproved private headers.
4. Pi benchmark results for creating, updating, and destroying 1,000 items.
5. Correct ownership/thread-affinity behavior under the actual Qt event loop.

If the candidate requires a private header or a behavioral Qt patch, stop for a
new decision.

## Host evidence — 2026-08-14

Under ADR-0010, the cached-component path is implemented and measured against
installed Qt 6.11.1 on macOS. `rnqnx_views_qt` uses only public headers; no
private Qt header is included, satisfying evidence item 3 for this Qt version.

`rnqnx_qtquick_host_probe`, 1,000 `Rectangle`-backed items, offscreen QPA,
Apple Silicon, Debug build with strict warnings:

| Phase                                   | Time      | Per item |
| --------------------------------------- | --------- | -------- |
| Create + parent + set absolute geometry | 12.856 ms | 12.9 µs  |
| 10 update passes (x + opacity per item) | 0.456 ms  | 46 ns    |
| Destroy                                 | 0.516 ms  | 0.5 µs   |

The update path — the one Fabric drives per frame — is the cheapest by three
orders of magnitude, which is the property that matters for mounting. The probe
also asserts that destroying items leaves no residual children on the content
item and that an unregistered type fails instead of returning a bare item.

This settles the shape of the strategy. It does not settle it for the target:
the exact Qt tag is now recorded by ADR-0001; evidence items 2, 4, and 5 remain open, and the Pi is slower than this host
by an unmeasured factor.

## Related files

- Platform specification §4 G0-C
- `vnext/include/react-native-qnx/views/QtQuickItemFactory.h`
- `vnext/src/views/qt/QtQuickItemFactory.cpp`
- `vnext/probes/qtquick/main.cpp`
- `docs/adr/0010-macos-qt-development-host.md`
- `docs/adr/0001-qt-quick-backend.md`
- `docs/toolchain.md`
