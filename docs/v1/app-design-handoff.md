# Template and conformance app handoff

A spec sheet for the three apps that put pixels on a QNX screen:
`template/qnx` (what `pnpm qnx init` copies), `apps/component-conformance` and
`apps/performance-lab`. It records the values in the source on 2026-11-11, names
them as tokens, and lists accessibility findings. No app code changed with this
page.

Everything here comes from reading the source and computing contrast on the
host. The apps were not run on a device for this page; the device audit is
**not run (needs device)**.

## Tokens

None of the three apps imports a shared token file. Each hardcodes its values.
The names below are proposals for a shared module so the apps stop drifting.

### Colour

| Proposed token   | Value     | Used in                         | Role                                  |
| ---------------- | --------- | ------------------------------- | ------------------------------------- |
| `surface.base`   | `#102033` | template, performance-lab       | Screen background                     |
| `surface.base`   | `#0c1929` | conformance                     | Screen background (differs)           |
| `surface.panel`  | `#142538` | conformance                     | List panel, unselected buttons        |
| `surface.active` | `#1b5b61` | conformance                     | Selected nav button, press button     |
| `surface.active` | `#215861` | conformance                     | Selected list row (differs)           |
| `text.primary`   | `#ffffff` | template, performance-lab title | Main text                             |
| `text.primary`   | `#e9f0f7` | conformance (`INK`)             | Main text                             |
| `text.primary`   | `#e3edf7` | performance-lab rows            | Row text                              |
| `text.onActive`  | `#f2ffff` | conformance                     | Text on a selected nav button         |
| `text.muted`     | `#9dafc2` | conformance (`MUTED`)           | Secondary text                        |
| `accent`         | `#79e8dc` | conformance (`ACCENT`)          | Eyebrow label, spinner, width readout |

Three roles have two or three values each. Pick one per role.

### Type

| Proposed token | Size / line height  | Where                                 |
| -------------- | ------------------- | ------------------------------------- |
| `type.display` | 32                  | Template app name                     |
| `type.title`   | 24 / 30             | Conformance screen title              |
| `type.heading` | 21 / 27             | Conformance section heading           |
| `type.body`    | 14 to 18            | Five sizes in use: 14, 15, 16, 17, 18 |
| `type.caption` | 12 / 18 and 13 / 19 | List subtitles, footer, eyebrow label |

All text uses the platform default family. The conformance `Label` sets line
height to size + 6. Template and performance-lab set no line height.

### Spacing and shape

Spacing values in use: 4, 8, 10, 12, 14, 16, 18, 24. A 4-point scale
(4, 8, 12, 16, 24) covers all but 10, 14 and 18. Corner radius is 10 on
controls and 12 on panels. Row height is 48 in performance-lab (`ROW_HEIGHT`)
and 58 in the conformance list.

## Components

| Component       | App             | Size                            | States in source                    | Accessibility in source                     |
| --------------- | --------------- | ------------------------------- | ----------------------------------- | ------------------------------------------- |
| Nav button      | conformance     | min height 42, padding 16 × 10  | selected / unselected (colour only) | `accessibilityRole="button"`, label = title |
| List row        | conformance     | height 58                       | selected / unselected (colour only) | role `button`, label = row title            |
| Text field      | conformance     | min height 52, font 17          | default                             | label "Editable text"                       |
| Switch          | conformance     | platform default                | on / off                            | no label; adjacent text is not associated   |
| Press button    | conformance     | padding 18 × 14 (about 48 tall) | default                             | role `button`, label "Increase press count" |
| Spinner         | conformance     | `small`                         | always animating                    | none                                        |
| Adaptive panels | conformance     | side by side at width ≥ 760     | wide / narrow                       | none needed                                 |
| List row        | performance-lab | height 48, padding 12           | none                                | none                                        |
| Title text      | template        | font 32, centred                | none                                | text only                                   |

No component has hover, focus, pressed or disabled styling. On QNX the input
path is synthetic or keyboard, so a visible focus state matters more than hover.

## Accessibility findings (WCAG 2.1 AA, source review)

### Contrast

| Pair                                       | Ratio   | Needed       | Result |
| ------------------------------------------ | ------- | ------------ | ------ |
| `#ffffff` on `#102033` (template)          | 16.45:1 | 4.5:1        | Pass   |
| `#e3edf7` on `#102033` (perf rows)         | 13.88:1 | 4.5:1        | Pass   |
| `#e9f0f7` on `#142538` (panel text)        | 13.52:1 | 4.5:1        | Pass   |
| `#9dafc2` on `#142538` (muted on panel)    | 6.91:1  | 4.5:1        | Pass   |
| `#9dafc2` on `#0c1929` (muted on base)     | 7.87:1  | 4.5:1        | Pass   |
| `#f2ffff` on `#1b5b61` (selected nav)      | 7.56:1  | 4.5:1        | Pass   |
| `#e9f0f7` on `#215861` (selected row)      | 6.94:1  | 4.5:1        | Pass   |
| `#79e8dc` on `#0c1929` (accent)            | 12.13:1 | 4.5:1        | Pass   |
| `#1b5b61` selected vs `#142538` unselected | 2.01:1  | 3:1 (1.4.11) | Fail   |

### Open findings

| #   | Finding                                                                                   | WCAG          | Severity | Fix                                                                                                  |
| --- | ----------------------------------------------------------------------------------------- | ------------- | -------- | ---------------------------------------------------------------------------------------------------- |
| 1   | Selected nav buttons and list rows differ from unselected ones by colour alone, at 2.01:1 | 1.4.1, 1.4.11 | Major    | Add a border or check mark on the selected item, and raise the fill contrast to 3:1                  |
| 2   | Selection is not exposed to assistive technology                                          | 4.1.2         | Major    | Set `accessibilityState={{ selected }}` on nav buttons and rows                                      |
| 3   | The Switch has no accessible name; "Switch ON/OFF" text beside it is a separate element   | 4.1.2, 3.3.2  | Major    | Give the Switch an `accessibilityLabel` and drop the state from the visible label                    |
| 4   | No visible focus state on any control                                                     | 2.4.7         | Major    | Add a focus ring style once keyboard focus reaches Pressable on QNX                                  |
| 5   | Nav buttons are 42 tall                                                                   | 2.5.5 (AAA)   | Minor    | Raise min height to 48 to match the press button and the window-chrome gate's "finger-sized" targets |
| 6   | Text field label "Editable text" does not say what to type                                | 2.4.6         | Minor    | Name the field by its content, for example "Sample text"                                             |
| 7   | 12-point captions for list subtitles and the footer                                       | none at AA    | Minor    | Use 13 or larger on a 1920×1080 panel viewed at arm's length                                         |

Items 1 to 4 need an app code change and a device check. They belong with the
`device-parity` and `window-chrome` gates; this page does not fix them.

### Not checked

- Screen reader output on QNX. The [View accessibility receipt](../evidence/accessibility-view-qnx-2026-11-07.md)
  covers Qt semantic queries, not a screen reader.
- Keyboard order across the conformance pages. Needs a device with a keyboard.
- Large-text behavior at increased font scale.

## Copy

| Where                 | Now                                      | Suggested                                    | Why                                       |
| --------------------- | ---------------------------------------- | -------------------------------------------- | ----------------------------------------- |
| Template title        | `__APP_NAME__ on React Native QNX`       | keep                                         | Confirms the app name reached the screen  |
| Conformance eyebrow   | `RNQNX / CONFORMANCE LAB`                | `Conformance lab`                            | All caps reads slower and adds no meaning |
| Switch label          | `Switch ON` / `Switch OFF`               | `Sample setting` plus the switch's own state | State belongs to the control              |
| Performance-lab title | `RNQNX performance workload / list-1000` | keep                                         | Matches the workload name in receipts     |
