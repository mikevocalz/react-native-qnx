# DECISIONS.md

## Decision log

Major architectural decisions are recorded as ADRs in `docs/adr/`. This file captures smaller or emerging decisions that do not yet warrant a full ADR.

### 2026-07-19 – Repository structure follows React Native out-of-tree platform conventions

- **Decision:** Adopt a monorepo with `apps/`, `packages/`, `native/`, `cmake/`, `template/`, `scripts/`, `tests/`, and `docs/`.
- **Reason:** Aligns with `react-native-linux` structural inspiration and Expo platform support expectations, while keeping QNX-specific code in clearly owned directories.
- **Alternatives considered:** Separate repos for runtime, Expo, and examples; rejected because cross-cutting changes (toolchain, Fabric, Nitro) require atomic commits.
- **Consequences:** Larger repository, but simpler coordinated releases.
- **Related files:** Repository root layout.
- **Should become an ADR:** Yes, once Milestone 0 exits.

### 2026-08-14 – React Native's own modules stay C++; Nitro is for application APIs

- **Decision:** modules that React Native's own JavaScript requires by name —
  `ExceptionsManager`, `SourceCode`, `DeviceInfo`, and any that follow — are
  implemented in C++ against React Native's generated `…CxxSpec` classes.
  Everything an application calls is a Nitro HybridObject.
- **Made by:** the project owner, asked directly after the third such module was
  written.
- **Why the question arose:** the specification says modules use Nitro and never
  TurboModules, and three TurboModules now exist. That is worth being precise
  about rather than leaving to a reader's inference.
- **The constraint that forces it:** `TurboModuleRegistry.getEnforcing('X')`
  reads `global.__turboModuleProxy` or `global.nativeModuleProxy`, installed by
  `TurboModuleBinding::install`. A Nitro HybridObject is reached through
  `HybridObjectRegistry` and is invisible to that lookup. The calls are inside
  React Native's own JavaScript — `Dimensions.js:126`,
  `ScrollView.js` via `Dimensions` — which this project does not own. So
  something must answer that registry for React Native's JavaScript to run at
  all.
- **Alternative considered and rejected:** wrapping Nitro HybridObjects behind
  the same TurboModule provider. It produces identical JavaScript-visible
  behaviour, adds a layer, and defines each method shape twice, since React
  Native's generated `CxxSpec` already defines them exactly.
- **Also rejected:** forking React Native's JavaScript so it asks Nitro.
  Permanent maintenance cost for no functional gain.
- **Consequences:** the count of C++ modules will grow as more React Native
  components are supported, and that is expected rather than drift. Nothing
  application-facing has been written in either system yet, so no Nitro work is
  displaced. ADR-0004 stands; this records where its boundary falls.

### 2026-08-13 – Compatible-current Nitro pair overrides the specification's historical lock

- **Decision:** Keep `react-native-nitro-modules` and `nitrogen` pinned exactly
  at 0.36.5 rather than downgrading to the specification's 0.36.3 pair.
- **Reason:** The user separately required the most current packages. Official
  npm registry metadata identifies 0.36.5 as latest for both packages;
  Nitrogen 0.36.5 requires Nitro `^0.36.5`. The installed 0.36.5/0.36.5 sources
  and lockfile already pass the repository source-contract validation.
- **Alternatives considered:** Exact 0.36.3/0.36.3 is internally coherent but
  stale; mixed versions create unnecessary generated/runtime skew.
- **Consequences:** The specification is unchanged, while ADR-0008 and the
  executable dependency validator are authoritative for the active line. A
  future bump still requires registry, generated-output, and source-seam audit.
- **Should become an ADR:** Already covered by ADR-0008.
