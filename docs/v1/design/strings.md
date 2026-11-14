# Runner and window strings

These are the selected English strings for implementation. They are not yet
wired into a translation or native resource system. Limits are design budgets
for static English text; rendered fit, wrapping and localization tests remain
required. Dynamic identifiers must never be silently shortened in diagnostics.

| Key                      | Text                                                                                                         | Maximum static characters | Behavior                                                                        |
| ------------------------ | ------------------------------------------------------------------------------------------------------------ | ------------------------- | ------------------------------------------------------------------------------- |
| `window.close`           | Close window                                                                                                 | 24                        | Accessible name for close in both layouts.                                      |
| `window.minimize`        | Minimize window                                                                                              | 24                        | Accessible name; expose only a verified target action.                          |
| `window.maximize`        | Maximize window                                                                                              | 24                        | Changes to Restore window when maximized.                                       |
| `window.restore`         | Restore window                                                                                               | 24                        | Same action identity through the maximize state transition.                     |
| `window.keepOpen`        | Keep window open                                                                                             | 24                        | Non-destructive close-guard action.                                             |
| `window.closeConfirm`    | Close this window?                                                                                           | 40                        | Only when an application guard requests confirmation.                           |
| `window.closeBody`       | Unsaved changes in this window will be lost.                                                                 | 96                        | Use only when the app actually reports unsaved changes; never infer it.         |
| `preflight.title`        | This runner needs a native build                                                                             | 48                        | Primary heading for missing or changed native modules.                          |
| `preflight.body`         | The app requires native modules this runner cannot load. Build the app with its current native dependencies. | 160                       | Full text wraps; display before app-bundle evaluation.                          |
| `preflight.module`       | Module                                                                                                       | 24                        | Column/row label.                                                               |
| `preflight.missing`      | Missing                                                                                                      | 24                        | Module absent from runner.                                                      |
| `preflight.changed`      | Changed native code                                                                                          | 32                        | Signature differs even if package versions match.                               |
| `preflight.invalidTitle` | Runner details could not be checked                                                                          | 48                        | Missing, malformed or unsupported runtime metadata.                             |
| `preflight.invalidBody`  | Use a runner built for this app, or rebuild it from the current project.                                     | 128                       | Does not imply a network retry can repair native metadata.                      |
| `preflight.copyBuild`    | Copy build command                                                                                           | 32                        | Copies the exact validated command; show only with a working clipboard backend. |
| `preflight.buildHeading` | Build this app on your development host                                                                      | 64                        | Display command as selectable text when clipboard is unavailable.               |
| `preflight.close`        | Close app                                                                                                    | 24                        | Closes the owned app, without stopping Metro.                                   |
| `preflight.copied`       | Build command copied                                                                                         | 40                        | Show only after successful copy.                                                |
| `dev.connecting`         | Connecting to Metro…                                                                                         | 40                        | First connection, before bundle is available.                                   |
| `dev.loading`            | Loading app…                                                                                                 | 40                        | Bundle request/evaluation phase; no percentage without measured progress.       |
| `dev.reconnecting`       | Reconnecting to Metro…                                                                                       | 40                        | Retain usable app state where allowed.                                          |
| `dev.connectionTitle`    | Could not connect to Metro                                                                                   | 48                        | Network/connection failure.                                                     |
| `dev.connectionBody`     | Start Expo on your development host and check that the device can reach it.                                  | 128                       | Name the actual packager location in a separate selectable detail row.          |
| `dev.retry`              | Retry connection                                                                                             | 32                        | Attempts connection again; no rebuild implication.                              |
| `dev.bundleTitle`        | The app could not load                                                                                       | 48                        | Bundle build or evaluation failure; keep actual error below it.                 |
| `dev.bundleBody`         | Fix the error on your development host, then reload the app.                                                 | 128                       | Does not claim code changes were saved or applied.                              |
| `dev.reload`             | Reload app                                                                                                   | 32                        | Resets the JS app according to existing reload behavior.                        |
| `dev.details`            | Error details                                                                                                | 32                        | Accessible label for bounded, selectable diagnostic content.                    |
| `dev.closeDetails`       | Close details                                                                                                | 32                        | Dismisses details only; cannot bypass incompatible-runner preflight.            |

For connection instructions, the selected wording names Expo because B requires
Expo to own the dev session. Until B lands, existing documentation must continue
to identify the direct Metro command truthfully. This strings table specifies
the integrated target behavior and does not announce that B is implemented.

## Identity and truncation

`expo.name` is the full title and About name. `extra.rnqnx.shortName`, when set,
is the tile label; otherwise the tile uses `expo.name`. There is no universal
character maximum for a tile. Preparation must shape the proposed label in the
target launcher's named font/size and reject measured overflow, reporting the
available width and measured width. The developer chooses a shorter name.

After successful fit validation, runtime text remains single-line with elision
as a defensive rendering behavior. Elision must not excuse a failed preparation
measurement, change tile dimensions, or remove the full accessible name.
Title-bar text may elide to keep controls reachable; its accessible name remains
the full title. Diagnostics wrap long module names and provide the complete
identifier through selectable/copyable details.

## Localization

Keep module/package names, hashes and commands untranslated. Translate status
labels separately from identifiers. Reserve expansion space and test RTL reading
order without reversing configured physical window-control positions. Avoid
concatenating translated fragments. Count user-visible graphemes for budgets,
then verify actual shaping; UTF-16 length is not a rendered-fit measurement.

Rejected alternatives: “Something went wrong” conceals the failed step;
“Update required” could mean a JS refresh, package download or native rebuild;
“OK” obscures whether the window closes, a retry starts or details disappear.
