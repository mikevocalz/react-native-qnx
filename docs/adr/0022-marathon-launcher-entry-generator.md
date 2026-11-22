# ADR-0022: Marathon launcher entry generator, and the scope lift for it

## Status

Accepted, 2026-11-21. Host tests only. The entry has not been installed on, or
read by, a Marathon device, so item 4e of the `app-identity` gate is met on the
host and stays pending on a device, and the gate stays `pending`.

## Context

The `app-identity` gate asks that the app's icon reach the launcher tile, title
bar, task switcher and `QGuiApplication::setWindowIcon`, and adds "(Marathon:
app manifest)". The [2026-11-21 host and QNX VM
receipt](../evidence/app-identity-host-vm-2026-11-21/README.md) listed item 4e,
the Marathon app manifest, as **missing**: no generator for consumer apps
existed, the only entries in the repository were hand-written, and writing one
was a new component outside the frozen Marathon scope.

The owner was asked and answered on 2026-11-21:

> "nothing missing, so yes to marathon"

## Decision

The Marathon freeze is lifted for one item: a generator that writes the
Marathon launcher entry for a consumer app, as `project marathon-entry`
(`packages/react-native-qnx-cli/src/marathon-entry.mjs`), with its test
(`tests/js/marathon-entry.test.mjs`), the docs that describe it, and this
record.

Not lifted, and unchanged: every other Marathon item, WebGPU, `docs/v2`,
Forge, new Expo ports, and pinch and rotation. The generator does not touch the
Marathon host, the compositor contract, `.marathon` packaging, permissions or
lifecycle.

### What the entry is

Marathon-Shell lists a native (non-QML) app on its home grid from a
freedesktop `.desktop` file. `read_app` in
`crates/marathon-shell-native/src/desktop.rs` reads it (patrickjquinn/Marathon-Shell
at `567f1faa8bfdf31611dd401134c293daa160221c`, the revision the label box in
[app identity](../getting-started/app-identity.md) is derived from; the file
differs from `1f845160`, the revision of the 2026-11-20 VM contracts, only in
`map_or_default` refactors). The scan paths are in `shell/main.cpp`:
`/usr/share/applications`, `/usr/local/share/applications`,
`~/.local/share/applications` (watched; a change rescans after 500 ms) and the
two Flatpak export directories.

The signed `.marathon` manifest is not an option: its schema requires a `.qml`
`entryPoint` and a `toolkit` of `keel` or `marathonui`, so it cannot start a
native executable ([UPSTREAM_RFC](../marathon/UPSTREAM_RFC.md), re-read at
`1f845160` on 2026-11-19). "App manifest" in the gate therefore means the
`.desktop` entry for a native app. This is a reading of the gate, recorded
here so a reviewer can disagree with it.

The generator writes `build/marathon/desktop/<appId>.desktop` from
`build/marathon/native-app.json`:

| Key                          | Value                                          | Why                                                                                                                                                     |
| ---------------------------- | ---------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| file name                    | `<appId>.desktop`                              | upstream takes the app id from the file stem                                                                                                            |
| `Type`                       | `Application`                                  | upstream drops any other type                                                                                                                           |
| `Name`                       | the tile label (`shortName`, else `expo.name`) | upstream shows `Name` verbatim in the grid; the width was already checked against the label box by prepare-native                                       |
| `Exec`                       | `--exec`, validated                            | upstream rewrites or drops some commands (see below)                                                                                                    |
| `Icon`                       | `<app-dir>/icons/icon-128.png`                 | an absolute file path is used as is; the QNX tile uses the same size                                                                                    |
| `Terminal`, `StartupWMClass` | `false`, the app id                            | both hand-written entries in the repository carry them; `StartupWMClass` is not read by `desktop.rs`, and that the host reports this id is not verified |
| `X-Purism-FormFactor`        | `Mobile;`                                      | the default "filter mobile friendly apps" setting hides an entry unless this, `X-KDE-FormFactors`, `Categories` or the Store's phone list says phone    |

`Exec` is refused when Marathon would change it: a leading `/` (upstream keeps
only the file name, so use a command on `PATH` or `sh /absolute/script`), field
codes, repeated whitespace, `--new-window` style flags, `flatpak run`,
`snap run`, `gapplication`, `sailfish-qml` (routed to the Keel host) and
X11-only tools. The characters allowed beyond that are a precaution, not a
finding: nothing in the source says what runs the command.

## Consequences

- A consumer app gets a launcher entry from one command, derived from the same
  descriptor, tile label and icon set that prepare-native already measured and
  hashed.
- The check is host-only. The generated file was run through upstream's own
  `read_app` and `fits_a_phone` code, copied unchanged and built against
  `freedesktop_entry_parser` 2.0.1, the version upstream pins. That shows the
  file is read as an app, shown, labelled with the tile label and found
  phone-ready. It does not show that a Marathon launcher draws the tile, that
  the icon looks right at the launcher's pixel ratio, or that tapping the tile
  starts the app.
- One `Icon` path means one size. At Keel pixel ratio 2 and above a 128 px file
  is scaled up; `--icon-size` picks another size from the set.
- The task switcher is untouched: nothing in this entry feeds it.

## Alternatives considered

- A `.marathon` manifest. Rejected: the schema accepts only a QML entry point.
- Reusing `project desktop-entry`. Rejected: it writes the QNX demolauncher
  tile and `desktop.conf` block, a different format with a different reader.
- Defaulting `--exec` to a launcher script. Rejected: no Marathon launcher
  script exists in the repository, and inventing one would be unverified code.

## Related files

- `packages/react-native-qnx-cli/src/marathon-entry.mjs`
- `tests/js/marathon-entry.test.mjs`
- `docs/getting-started/app-identity.md`
- `docs/evidence/app-identity-host-vm-2026-11-21/README.md`
- `docs/evidence/app-identity-host-vm-2026-11-21/marathon-entry/`
- `docs/marathon/UPSTREAM_RFC.md`

## Notes

Needed on a Marathon device to close the device half of 4e: install the entry
and the icon directory, confirm the tile appears with the custom icon and the
`Name` label, tap it and confirm the host starts, and read the launcher's real
pixel ratio and label box.
