# ADR-0012: Quick Controls style for the QNX component library

## Status

Proposed 2026-08-14. Evidence is from the Qt 6.11.1 Homebrew installation and
Qt's Quick Controls documentation. Acceptance on the Pi requires a component
gallery that renders correctly with RN style props authoritative.

## Context

Quick Controls ships with several built-in styles (Basic, Fusion, Imagine,
Material, Universal, and platform-specific styles). It also has a labs-level
`Qt.labs.stylekit` (Technology Preview). The style determines how complex
controls such as `Switch`, `Button`, `Slider`, and `TextField` look and feel.

The current view layer does not use Quick Controls. `<Switch>`, `<Button>`,
`<ActivityIndicator>`, and `<TextInput>` are implemented as plain `QtQuick` items
or `FocusScope` wrappers with direct `Rectangle`/`Text` drawing. The style
question therefore affects future components and any move to use `Button`,
`Switch`, `Slider`, or `TextField` from Quick Controls.

## Decision

1. The default style for any Quick Controls use is **`Basic`**. It is the
   smallest, most predictable style and is the one least likely to override
   React Native style props with platform-specific chrome.
2. `Fusion`, `Imagine`, `Material`, and `Universal` may be exposed later as
   opt-in themes, but only if a component gallery on the Pi proves that RN
   `style` props still control color, font, border, and geometry. If a style
   overrides those props, it is a bug.
3. `Qt.labs.stylekit` is **not used** until it leaves Technology Preview; the
   project will not put a stable public API on a TP module.
4. Theming is not a substitute for RN style props. Quick Controls styles are an
   implementation detail; the public API remains React Native's style system.

## Consequences

- Components built with Quick Controls will look simple and uniform across QNX
  targets.
- No macOS-specific platform style leaks in; `Basic` does not vary by desktop
  environment.
- A later Material/Fusion opt-in requires its own gallery and ADR revision.

## Alternatives considered

- **Use `Material` by default.** Rejected: Material's design assumptions
  (elevation, ink, density) can silently override RN `style` values and the
  palette does not match QNX automotive/industrial expectations by default.
- **Use `Fusion` by default.** Rejected: Fusion is more complete than `Basic`
  but adds extra non-RN styling that still needs to be proven on the Pi. Kept as
  a future opt-in.
- **Adopt `Qt.labs.stylekit` immediately.** Rejected: it is a Technology
  Preview with no source or binary compatibility promise.
- **Build a custom style from scratch.** Rejected: `Basic` plus RN style props
  is the smallest thing that can work; a custom style is unnecessary until a
  product requires it.

## Installed evidence (Qt 6.11.1, macOS host)

The `qt` Homebrew keg contains the following style frameworks under
`/opt/homebrew/Cellar/qt/6.11.1/Frameworks`:

- `QtQuickControls2Basic.framework`
- `QtQuickControls2BasicStyleImpl.framework`
- `QtQuickControls2Fusion.framework`
- `QtQuickControls2FusionStyleImpl.framework`
- `QtQuickControls2Imagine.framework`
- `QtQuickControls2ImagineStyleImpl.framework`
- `QtQuickControls2Material.framework`
- `QtQuickControls2MaterialStyleImpl.framework`
- `QtQuickControls2Universal.framework`
- `QtQuickControls2UniversalStyleImpl.framework`
- `QtLabsStyleKit.framework` and `QtLabsStyleKitImpl.framework` (Technology
  Preview)

The `Basic` style implementation is present and available in the current
installation. `QtQuickControls2` (the templates) and the platform-specific
implementations (`IOS`, `MacOS`, `FluentWinUI3`) are also present but are not
intended for the QNX target.

## Related files

- `docs/adr/0011-qt-module-licensing.md`
- `docs/adr/0001-qt-quick-backend.md`
- `vnext/probes/qtquick/main.cpp`
- `vnext/src/views/qt/QtQuickItemFactory.cpp`

## Notes

- This ADR does not change any current component. It becomes relevant the first
  time a component imports `QtQuick.Controls`.
- The acceptance criteria in Prompt 04 (a themed gallery on the Pi) remains
  unproven on the macOS host. This record is a design decision only; the Pi
  gallery is the Tier C gate.
