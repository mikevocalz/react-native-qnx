# Visual direction for the QNX runner

The runner should read as a small native application on the QNX desktop. The
application's icon and name carry its identity. The chrome quietly exposes
window actions; compatibility and connection states explain the next action.
The existing dark blue chrome is the starting palette, with light appearance
where Qt reports it. There is no new decorative brand or marketing surface.

## Five surface decisions

| Surface            | Direction                                                                                                                                                                       | Alternative rejected and reason                                                                                                       |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- |
| macOS title bar    | Preserve left round controls and centered icon/title group inside the reserved title rectangle. Keep glyphs visible on QNX, 44 px targets and keyboard focus.                   | Hover-only glyphs on QNX would make color carry the action and hide the action from touch users.                                      |
| Windows title bar  | Preserve left identity and trailing minimize/maximize/close cells. Keep close's red hover response, with a contrast-safe focus ring independent of hover.                       | Moving all controls to the left would erase the configured layout's meaning.                                                          |
| Launcher tile      | Keep the launcher's fixed cell and generated unmasked icon. One line of measured short-name text sits below it; focus outlines the whole tile.                                  | Per-name cell widths and multi-line labels would disturb the launcher grid.                                                           |
| Native preflight   | Use a left-aligned title, plain explanation, module rows, then build/copy and close actions. One error region marks the failed prerequisite; the app never evaluates behind it. | A toast is too transient for an app that cannot load. A full red wall gives module names and recovery instructions the same emphasis. |
| Development states | Use a compact connection banner while useful content exists. Use an owned panel for first-load connection failure or bundle error, retaining native chrome access.              | Replacing the app with a blank screen during every reconnect destroys context and makes the process appear dead.                      |

Use the proposed roles in [system-audit.md](system-audit.md): dark blue surface,
light primary text, cyan dark-focus ring, and error color confined to error
content. Use the existing bundled DejaVu Sans family in runner panels and
DejaVu Sans Mono for diagnostics; retain the launcher's measured Source Sans
identity contract. A new font family would add provisioning and fit uncertainty.

## Layout sketches

```text
macOS:   [close][min][max]      [icon App name]               |
Windows: [icon App name]                     [min][max][close]|
         ---------------------------------------------------
         Fabric content starts below native default-style bar

preflight:
         This runner needs a native build
         The app requires modules this runner cannot load.
         Module name                 Missing / Changed
         Module name                 Missing / Changed
         [Copy build command]        [Close app]

dev state with retained content:
         native title bar
         [Reconnecting to Metro…] [Retry connection]
         last usable content, with no incompatible update admitted
```

The sketches preserve the existing outer window contract. They do not prescribe
changing app width, height or orientation. Panels reflow within the actual
content region: at widths below 560 logical px, stack actions and module status
under each name; at larger widths keep rows. This is a proposed content breakpoint,
not a replacement window size.

## Review against the brief

The first draft considered a dashboard of module cards. A plain list better
supports package-name scanning and a keyboard copy workflow, so the direction
uses rows. It also considered a new display typeface; keeping the shipped fonts
reduces runtime and label-fit changes. The distinct feature is a named native
prerequisite with a truthful build action, rather than extra visual decoration.

Secondary-window planning follows the newer H7 request: one process and runtime,
additional named Fabric roots, shared modules, and per-root component state.
The existing singleton process and default single-window behavior remain in
force until a separately reviewed implementation changes that contract. No
second app process or duplicate owner tile is part of this design slice.
