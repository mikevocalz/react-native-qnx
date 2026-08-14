# ADR-0002: Qt Quick built-in-item instantiation

## Status

Proposed; blocked by G0-B source/build proof and G0-C Pi benchmarking.

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
new decision. This document authorizes no implementation yet.

## Related files

- Platform specification §4 G0-C
- `docs/adr/0001-qt-quick-backend.md`
- `docs/toolchain.md`
