# ADR-0023: The Pi 400 stands in for the device-parity gate

## Status

Accepted, owner Mike (2026-11-23). Mike stated in the 2026-11-23 session that the connected Pi 400 is the board for every gate.
The Accepting pull request carries the matching `device-parity` gate text in `docs/releases/v1-readiness.json`.

## Context

The `device-parity` gate asks for a device disposition and tested limits for every component "on stock Pi 4 QSTI" and says Pi 400
results stay separate. The only board connected to the project is a Raspberry Pi 400 running QNX 8.0.0 with the stock desktop.
No stock Pi 4 QSTI board is available.

## Decision

Pi 400 results are the device results for `device-parity`. Every receipt names the board as Pi 400 and never calls it a stock
Pi 4. A gate flips only when each listed criterion has Pi 400 evidence. The gate text in `v1-readiness.json` changed in the same
pull request that Accepted this ADR, and in no other.

For `cold-boot`, the ADR allows the named release board to be the Pi 400, and nothing else. The Pi 400 keeps booting to the stock
QNX desktop: no autostart, no edit to `/system/etc/startup`, no hook that launches an app at boot, and no reboot or kill of
`drm-rpi4` or `screen` by an agent. The criterion still needs 20 consecutive supervised cold boots with recovery and logs, each
started by a person power-cycling the board and each followed by a by-hand launch. Those are the remaining criteria; the board
type is not one of them.

## Consequences

- `docs/component-support.json` already holds a `pi400` column; it becomes the disposition column for the gate. `pi4Qsti` stays
  `unverified` and is not read by the gate.
- 12 of 24 components were `unverified` on the Pi 400 on 2026-11-23 (VirtualizedList, SectionList, VirtualizedSectionList, Button,
  TouchableOpacity, TouchableHighlight, TouchableWithoutFeedback, ImageBackground, KeyboardAvoidingView, RefreshControl,
  SafeAreaView, StatusBar). The gate stays `pending` until each has a Pi 400 disposition and the keyboard and touch scope is proven.
- `cold-boot` needs 20 supervised cold boots on the Pi 400 with no boot hook (see Decision). Only the board name changed.

## Alternatives considered

Keep the stock Pi 4 requirement: no board exists, so the gate could never close.

## Related files

- `docs/releases/v1-readiness.json`
- `docs/component-support.json`
