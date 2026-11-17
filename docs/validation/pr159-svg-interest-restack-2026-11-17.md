# PR 159 SVG subscription restack — November 17, 2026

The reviewed image branch now incorporates the actual published SVG commit
`6f40a8d5a26a987ebfe277b9e7d7a0f051d4d58c`. Its public tree is
`f544c4d475a7115dec05e0289fbffcd658896074`, with parent
`d3eacc75dcad761c992736223a68b53ff852203b`. The object was fetched from the
existing GitHub remote and its tree and parent were checked before merging.

The previous local head is `d4445cf45ea5498a116710bb79e8a1ad509e83bd`, whose
tree `03455c6af3891b91aa4a7e804055a777af57c5aa` matches published PR 159 head
`d50e34d99a504f48b690225ad28e8cb3cd8ca37a`. That published head completed all
15 workflows and 17 jobs successfully. The retained
[CI snapshot](../evidence/pr159-svg-interest-restack-2026-11-17/previous-head-ci.json)
and [source-tree proof](../evidence/pr159-svg-interest-restack-2026-11-17/previous-head-source-trees.json)
distinguish direct checkouts from GitHub's tree-identical synthetic merge.
Those results belong to the previous head; the new merged head requires its
own hosted checks.

## Merge scope and source identity

The SVG continuation retains the existing listener marker through native
codegen and QNX's React prop transport. Reporters keep their geometry tracking
and dispatch when React or an optional native-interest predicate requests the
event. Its original-source controls, transport discovery, lifetime mutation,
49/49 host graph and 149 pixel comparisons are documented in the
[SVG receipt](pr161-svg-layout-interest-2026-11-17.md).

Two native conflict resolutions preserve both reviewed additions: the adapter
header retains the existing modal include alongside the new standard includes,
and its constructor retains ModalHostView registration alongside guarded SVG
interest-state initialization. The handoff and readiness conflicts preserve
both dated workstreams and both evidence entries. The coordinator and Worklets
integration agent independently reviewed those resolutions.

Eighteen of the 23 imported source, generator and test files match the SVG
receipt byte-for-byte. Five retain existing PR 159 context: BaseViewConfig,
the adapter header/source and the QtReactHost header/source. The
[source inventory](../evidence/pr159-svg-interest-restack-2026-11-17/source-identities.json)
records every resulting hash, both parent identities, the build configuration
and the merged input tree `45ba17d4cb9c64405d64023eebe92d9fcace565d`.
All 105 imported SVG evidence files remain intact; all 104 files covered by
their published inventory match its hashes. Raw logs and patches retain their
original bytes.

## Affected integration checks

The existing Debug Hermes graph has Expo Image enabled and SVG disabled.
The ordinary rebuild compiles the changed adapter/host inputs and relinks
both actual Expo executables. It succeeds in 23.66 seconds. These tests use
direct Fabric scripts and the current Expo polyfill; the focused transport
test separately executes the real React view-config and prop-serialization
pipeline.

| Check                                | Result                 |
| ------------------------------------ | ---------------------- |
| `rnqnx_expo_image_module_host_test`  | Pass, 0.78 seconds     |
| `rnqnx_expo_image_fabric_host_test`  | Pass, 0.62 seconds     |
| Combined configured CTest invocation | 2/2 pass, 1.42 seconds |
| Real SVG layout transport regression | Pass                   |
| Pinned SVG codegen drift check       | Pass                   |
| Both TypeScript configurations       | Pass                   |

The [build record](../evidence/pr159-svg-interest-restack-2026-11-17/build.json),
[CTest record](../evidence/pr159-svg-interest-restack-2026-11-17/affected-image-tests.json),
[raw build](../evidence/pr159-svg-interest-restack-2026-11-17/build.log),
[CTest output](../evidence/pr159-svg-interest-restack-2026-11-17/affected-image-tests.log)
and [native output](../evidence/pr159-svg-interest-restack-2026-11-17/affected-image-native-output.log)
retain exact commands, configuration, timestamps and executable/object hashes.
All 23 recorded source hashes remain unchanged through execution.

The configured image tests retain their 120-second limits, software Qt rendering
and existing AddressSanitizer fixture/link settings, including disabled leak
detection. Production and upstream objects are not all sanitizer-instrumented.
The SVG-enabled graph has its separate prerequisite receipt; final hosted
renderer ON/OFF jobs must validate the complete merged branch.

Formatting, generated documentation and the scoped source/metadata diff checks
are recorded in the [evidence index](../evidence/pr159-svg-interest-restack-2026-11-17/README.md).
The previously documented local full-JavaScript Unix-socket restriction remains
separate from the previous head's successful hosted JavaScript suites.

## Review, remaining acceptance and rollback

DRI: Expo integration agent `finish_expo159`. Independent merge reviewers:
coordinator `root` and Worklets integration agent `qnx_readiness`. The SVG
implementation and its native/transport evidence received separate independent
reviews recorded in its receipt. These agent reviews retain their host scope.

Image worker, disk/memory epoch, uncached-pixel delivery and teardown behavior
remain as previously reviewed. `configureCache` retains its synchronous
contract. Broader router prop/RTL/resize conformance remains outside this
continuation. There is no new QNX or Marathon runtime execution, release asset
qualification or exact-device receipt. All eleven release gates are unchanged
and pending, with canonical gate-array SHA256
`ced2bf1a611f20f87eeccf058b7e484d98a0b1382aa165f18a69e07e36fe13b4`.
Keep the PR draft while its required acceptance evidence remains open.

Rollback can revert this SVG merge while retaining the previous image worker
commit chain and its evidence. If the optional Reanimated registry integration
has subsequently been stacked, coordinate its rollback with removal of the
SVG-interest setter. Rebuild the affected host targets and refresh application
bundles when changing platform configuration.
