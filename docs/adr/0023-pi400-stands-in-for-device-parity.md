# ADR-0023: The Pi 400 stands in for the device-parity gate

## Status

Proposed. The decision is the owner's (Mike, 2026-11-23). It reached this branch as a message from the coordinating agent, not
as a signed change by the owner, so it is not Accepted until the owner approves this pull request. Until then
`docs/releases/v1-readiness.json` keeps the gate text it has.

## Context

The `device-parity` gate asks for a device disposition and tested limits for every component "on stock Pi 4 QSTI" and says Pi 400
results stay separate. The only board connected to the project is a Raspberry Pi 400 running QNX 8.0.0 with the stock desktop.
No stock Pi 4 QSTI board is available.

## Decision

Pi 400 results are the device results for `device-parity`. Every receipt names the board as Pi 400 and never calls it a stock
Pi 4. A gate flips only when each listed criterion has Pi 400 evidence. The gate text in `v1-readiness.json` changes in the same
pull request that Accepts this ADR, and in no other.

## Consequences

- `docs/component-support.json` already holds a `pi400` column; it becomes the disposition column for the gate. `pi4Qsti` stays
  `unverified` and is not read by the gate.
- 12 of 24 components are `unverified` on the Pi 400 on 2026-11-23 (VirtualizedList, SectionList, VirtualizedSectionList, Button,
  TouchableOpacity, TouchableHighlight, TouchableWithoutFeedback, ImageBackground, KeyboardAvoidingView, RefreshControl,
  SafeAreaView, StatusBar), so the gate stays `pending` after this ADR too.
- `cold-boot` is unaffected: it needs 20 supervised cold boots, and the Pi 400 must keep booting to the stock QNX desktop, with no
  boot hook.

## Alternatives considered

Keep the stock Pi 4 requirement: no board exists, so the gate could never close.

## Related files

- `docs/releases/v1-readiness.json`
- `docs/component-support.json`
