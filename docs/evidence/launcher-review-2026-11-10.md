# QNX launcher failure recovery review, 2026-11-10

This follow-up fixes two host-reproduced failure paths on PR #103 base
`e1c7001ef559ed1078ec3dd90f501b01c0e4bd14`.

## Process cleanup

A timed-out SSH/local tool could terminate its immediate child while leaving
a descendant running. If that descendant ignored TERM and closed inherited
output pipes, the parent's `close` event canceled the pending KILL timer. The
command had already rejected, so a short-lived CLI could also exit before an
unreferenced cleanup timer ran.

The command now finishes only after killing the owned local process group and
closing its streams. A parent `close` cannot cancel cleanup, and output is no
longer accumulated after failure. The regression creates a TERM-ignoring
descendant with independent pipes and a heartbeat file. It failed on the
original implementation with `timed-out descendant kept running` and passed
after the fix. Remote SSH side effects still require inspection; killing a
local process group does not prove remote cancellation.

## Managed destination paths

The installer checked staged filenames but did not validate every existing
final destination. Replacing `icon.png` with a symlink to another directory
caused `mv` to follow the directory and still report `RNQNX_LAUNCHER_OK`.

Before replacement, the installer now rejects symlinks or non-files at each
managed destination, including the icon, launch script, app metadata, entry
fragment, and combined desktop configuration. Regression fixtures replace
each destination with a directory symlink and require refusal while retaining
the symlink and the unrelated directory contents. The original icon case
reproduced a false success before the destination checks were added.

## Validation and scope

Environment: Linux x86-64, Node 26.13.0, pnpm 12.8.1. Frozen offline installation
passed with unchanged dependency pins and lockfile. The focused launcher suite
passes **136 host checks** (previously 120), including both new reproductions,
install/update/coexistence/removal, interrupted-operation recovery, literal
argv handling, icon validation, and strict SSH planning. Typechecking passed.
Additional applicable checks and their actual outcomes are recorded in the
handoff, including any blocked full-suite run.

These are real host process/filesystem tests and inert generated-shell
fixtures. No QNX cross-build, device installation, physical icon click, target
refresh/update/removal, QNX VM, Qt UI, or Marathon shell ran in this follow-up.
The older Pi 400 backend registration remains historical prerequisite
evidence. The configured extra launch arguments and this changed cleanup
behavior have no new target execution receipt.

## Skills, ownership, and handoff

Repository `qnx-platform-validation` and `qnx-expo-phase-delivery` were read at
the inspected branch. Their applied decisions were to exercise process and
filesystem failure boundaries, retain the existing non-root registration
contract, and keep target acceptance separate from host simulations.
Callstack `writing-user-docs`, source
`59e9d17446a248b21d86f078fcd0bd2d36db1ca4`, was read and applied to the recovery
instructions. No new native API, Nitro binding, renderer, dependency, paid
component, or licensing change is introduced.

Implementation/test owner: `audit_base_stack`; independent code review: the
session integrator. Human QNX/Qt target acceptance remains unassigned. The next
target check is a named installed application with literal launch arguments,
visible icon selection, launch/foreground behavior, update/removal, and
confirmed user-data retention. No performance improvement is claimed.

Reverting this follow-up restores the old process/destination handling. To
remove an application registration, use the documented launcher removal
command; executable files and user data remain. No boot, OS launcher, Screen,
or system runtime files were changed by this host work.

## Follow-up source checks

The frozen lockfile SHA-256 is `b854ebe2e50a42b2e8e07124df065592d99d43b1d1a3a984b66d3835ad86c3e4`.
Documentation, dependency, architecture, and manifest validation and
`pnpm exec tsc --noEmit` all passed with exit 0. Generated records were updated
with `pnpm docs:sync`; release gates remain pending. `git diff --check` passed.
