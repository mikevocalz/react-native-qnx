# PR 159 host review evidence, 2026-11-14

These are raw Linux host outputs for the follow-up based on `d862625`.
The [validation receipt](../../validation/pr159-review-followup-2026-11-14.md)
records commands, toolchain, dependency identity, behavior, limitations and
rollback. Native UI includes the integrated modal fix and both direct and
memory-prefetched GIF animation cases. The SVG HTTP run instruments project
code with ASan/UBSan; Qt libraries are uninstrumented and leak checking is
disabled by the existing target. `full-js-blocked.txt` records the unrelated
Unix-socket restriction. These files contain no QNX or physical-device result.

The final frozen code source is `252b128d0bbb93357dd87f840ea81d31a08c5ef1`;
`tested-source.json` records source-tree identities and SHA-256 file checks.
`image-sanitizers/` contains the fresh 73-entry image ASan/UBSan run.
`release-moc-refresh/` records the corrected 126-entry Release run after a
stale generated MOC was found to omit one test; the original 125-entry logs
remain intact with that limitation explained in the receipt.

`full-host/` preserves the successful Hermes dependency/Metro steps and the
failed GCC full-host attempt. The subsequent
`full-host-clang/` directory contains the corrected candidate/control/restored
59-case host evidence, including the failed intermediate attempts. The
full-host source is b6bf7eb, recorded in `full-host-tested-source.json`; its
image checks remain scoped separately to 252b128.

The newly archived `full-host/`, `full-host-clang/`, `image-sanitizers/` and
`release-moc-refresh/` log copies have trailing horizontal whitespace
trimmed for repository formatting. No diagnostic, test line or result was
removed; original command outputs remain in the recorded build directories.

The archived CMake cache also omits its final empty line.
