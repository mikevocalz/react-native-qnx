# PR 159 SVG subscription restack evidence

Read the [validation receipt](../../validation/pr159-svg-interest-restack-2026-11-17.md)
for source identities, scope, review and remaining acceptance.

| Evidence                                                                                                                                                               | Scope                                                                                                                                         |
| ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| [Source identities](source-identities.json)                                                                                                                            | Actual published SVG parent, previous local/public image identity, merged input tree, all 23 affected source hashes and existing graph flags. |
| [Previous-head CI](previous-head-ci.json), [tree proof](previous-head-source-trees.json)                                                                               | Published d50e34d completed 15 workflows/17 jobs; direct and synthetic-merge checkouts have the same recorded tree.                           |
| [Native build](build.log), [result](build.json)                                                                                                                        | Rebuild and relink both actual Expo executables against the merged headers and sources.                                                       |
| [Affected CTests](affected-image-tests.log), [native output](affected-image-native-output.log), [JUnit](affected-image-tests.xml), [result](affected-image-tests.json) | 2/2 configured image tests pass with source and artifact identities. SVG is disabled in this graph.                                           |
| [Real transport](transport.log), [result](transport.json)                                                                                                              | Pinned platform filtering, generated metadata and React create/diff listener lifecycle.                                                       |
| [Codegen](codegen.log), [result](codegen.json)                                                                                                                         | Pinned generator output remains reproducible.                                                                                                 |
| [TypeScript](typecheck.log), [result](typecheck.json)                                                                                                                  | Both existing TypeScript configurations pass.                                                                                                 |
| [Documentation sync](docs-sync.log), [result](docs-sync.json)                                                                                                          | Standard generator updates the project index from canonical evidence metadata.                                                                |
| [Documentation check](docs.log), [result](docs.json)                                                                                                                   | Repository documentation and dependency-baseline checks.                                                                                      |
| [Formatting check](format.log), [result](format.json)                                                                                                                  | Full repository formatting after the receipt and metadata update.                                                                             |
| [Final integrity](final-integrity.json)                                                                                                                                | Source hashes, eleven unchanged gate entries, imported evidence and scoped diff check.                                                        |

The [imported SVG evidence](../pr161-svg-layout-interest-2026-11-17/README.md)
retains its 105 published files and separate ON graph, original-source controls,
intermediate transport failure and lifetime mutation. This restack adds only
the affected integration and light checks recorded above. Raw logs and patches
are preserved without whitespace normalization.
