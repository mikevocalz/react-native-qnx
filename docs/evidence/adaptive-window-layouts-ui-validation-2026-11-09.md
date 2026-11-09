# Adaptive layouts — JavaScript and packaging validation, November 9, 2026

## Scope and source

This receipt records the JavaScript, TypeScript, starter-generation and documentation
checks for the adaptive gallery implementation. The checks were rerun after merging
prerequisite PR #104 at `3b414f185ec9b6c23eb7fbfdbc0afc9bfed18b94`, using merge
checkpoint `8597464` plus the subsequent reviewed integration fixes. The final
candidate revision and native execution results belong to the
[main adaptive-window receipt](adaptive-window-layouts-2026-11-09.md).

The implementation is React Native on the existing Fabric, Yoga, Hermes and Qt Quick
path. The earlier explanatory HTML preview is not an application dependency or the
implementation under test. The optional
[`@rn-qnx/react-native-qnx/adaptive` module](../../packages/react-native-qnx/adaptive/README.md)
has two consumers: the component gallery and the generated starter. It adds no
premium dependency or replacement renderer.

The host used **Node v26.0.0**, executable
`/workspace/scratch/508fcf9227b7/toolchain/node_modules/node/bin/node`, and **pnpm
12.8.1**. Dependency installation used the existing frozen lockfile. Native helper
tools and Qt 6.11.1 libraries were supplied through the identified host toolchain's
`PATH` and `LD_LIBRARY_PATH`; these are validation-environment settings, not values
embedded in the application or generated project.

## Focused checks

Each command below completed successfully. Logs are retained under
`build/adaptive-ui-validation/`; the main receipt identifies the final retained
artifacts. A passing fixture establishes only the scope stated in the last column.

| Check                        | Command                                                                                              | Scope                                                                                                                                                                                              |
| ---------------------------- | ---------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Adaptive helpers and shell   | `node tests/js/qnx-adaptive-layout.test.mjs`                                                         | Logical size-class boundaries, invalid measurements, fitting one/two/three-column grids, stable child ancestry and deterministic menu/focus transitions.                                           |
| Component gallery            | `node tests/js/qnx-component-conformance.test.mjs`                                                   | Public imports/assets, actual Zustand state transitions, retained route-layer structure, selection/value preservation, width-only and height-only geometry updates, and duplicate geometry no-ops. |
| Public entry                 | `node tests/js/qnx-public-entry.test.mjs`                                                            | Lazy standard RN exports, I18nManager path, unsupported-export errors, resolver opt-in and other-platform preservation.                                                                            |
| Renderer validation contract | `node tests/js/host-renderer-contract.test.mjs`                                                      | Required host-target inventory, fail-closed validation and retained prerequisite gallery/desktop controls. This validates the validator, not execution of every native target.                     |
| Editable-field state         | `node tests/js/text-input-state.test.mjs`                                                            | Per-instance state, controlled acknowledgements, forwarded refs/events and the secure-input gate.                                                                                                  |
| Compiled QML source          | `node tests/js/text-input-qml-source.test.mjs`                                                       | Exact styled OFF snapshot, both-mode border/fill/radius and Yoga content-inset declarations, and ON-only selection endpoint observers. Native paint and input remain separate tests.               |
| Project generation           | `node tests/js/qnx-project-create.test.mjs`                                                          | Actual local package archives, independent QNX and dual-target project output, application identity, integrity hashes, refusal cases and no-overwrite behavior.                                    |
| CLI initialization           | `node tests/js/qnx-cli.test.mjs`                                                                     | Initializer output/dependencies, symlink confinement, rejected options, dry-run behavior and propagation of a real shell failure. No device connection.                                            |
| Repository tooling types     | `node node_modules/typescript/bin/tsc --noEmit --pretty false`                                       | Existing tooling type contract.                                                                                                                                                                    |
| Public adaptive types        | `node node_modules/typescript/bin/tsc --noEmit --pretty false -p tests/types/adaptive.tsconfig.json` | Consumer imports and accepted/rejected public API arguments in an isolated React Native type environment.                                                                                          |
| Dependency policy            | `node scripts/validate-dependencies.mjs`                                                             | Pinned dependency and patch policy.                                                                                                                                                                |
| Architecture policy          | `node scripts/validate-architecture.mjs`                                                             | Existing renderer/platform architecture constraints.                                                                                                                                               |
| Package manifests            | `node scripts/validate-manifests.mjs`                                                                | Ten package manifests and seven scaffold-only directories.                                                                                                                                         |
| Documentation                | `node scripts/validate-docs.mjs`                                                                     | Generated content, local links, 22 component records and seven release gates; it does not mark the release gates passed.                                                                           |

The adaptive type fixture lives only in `tests/types/adaptive.ts` with its dedicated
configuration. The obsolete `tests/js/qnx-adaptive-types.ts` is absent. Keeping the
React Native consumer fixture isolated avoids mixing its DOM-like globals with the
Node tooling environment while preserving both type checks.

Changed JavaScript, TypeScript, JSON and Markdown files were checked with the
repository's pinned Prettier. Formatting was confined to changed files. The original
font license bytes were preserved; they were not reformatted.

## Full JavaScript suite

**51 of 52 discovered files passed; one file was environment-blocked.** The
official runner exited 1 at `marathon-development-launcher.test.mjs` because the
host denied its local Unix-socket `listen()` with `EPERM`. Fourteen files had
completed before that failure. All 37 subsequent files completed successfully
in the continuation, which exited 0. This is not a fully passing official suite.

The official `node scripts/run-js-tests.mjs` runner discovers and imports test files
in sorted order. It stops at the first error. After the observed environment-only
launcher failure, the remaining files were imported in the same sorted order in a
separate process. No assertion was weakened, no test was edited to bypass the
restriction, and the blocked file is not counted as a pass. The continuation
validates its boundary against the official runner log before executing anything.

The retained logs are `build/adaptive-ui-validation/full-js.log` and
`build/adaptive-ui-validation/remaining-js.log`; `summary.json` records the exact
discovered files, passes, blocker, executable/version and exit codes. The first
attempt exposed unformatted new documentation rows, which were corrected before
the recorded rerun. That development failure remains in
`full-js-before-format.log` and is not counted as a passing run.

## What the state and generator checks establish

The adaptive JavaScript fixture uses deterministic React hook/ref substitutes to
exercise render/effect transitions. Its focus checks establish that the intended
imperative command is issued after the overlay transition, including the
wide-to-compact native-blur ordering. They do not prove Qt keyboard delivery or
physical focus behavior. The gallery fixture uses actual vanilla Zustand stores
with a React subscription substitute, so it checks application state and React
element structure rather than a rendered native tree.

The route checks require stable keys and parent ancestry, equal content geometry,
transparent inactive layers, and `pointerEvents="none"` for inactive routes. They
reject using `display: none` to retain pages because Fabric removes hidden native
subtrees. Actual caret, native item identity, scroll offset, window resizing and
menu/dialog keyboard behavior are covered by the distinct native gallery evidence
in the main receipt. Neither JavaScript object identity nor a saved input string
alone is presented as proof of native selection or scroll retention.

The generator tests inspect real local archives and generated source. They check
that the reusable adaptive module is packaged and that starter output adopts the
same shell/grid with per-instance application state. The public-entry Metro tests
in the wider suite exercise the actual locked Metro bundler. Bundling or archive
inspection alone does not prove native compilation, rendering or deployment.

## Limits

These results are Linux host evidence. They establish no new QNX cross-build,
physical device execution, Marathon/Wayland session, operating-system screen-reader
support, IME qualification, real cross-display movement or system font-preference
provider. Logical 360-unit fixtures are layout inputs; they do not override the
desktop host's separate minimum window-size policy. There is no FPS, latency,
startup, memory or battery improvement claim.

The implementation requires a matching rebuilt native executable and JavaScript
bundle/assets. See the main receipt for native OFF/ON results and the remaining
physical-target merge gate.
