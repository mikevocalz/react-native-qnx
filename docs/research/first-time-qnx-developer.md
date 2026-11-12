# The first-time QNX developer

Who opens this repository for the first time, what they already have, and where
the published guides lost them. Based on walking the docs as written on
2026-11-12 ([clean-host simulation](../evidence/clean-host-sim-2026-11-12-b51b49c.md)).
No interviews were run; the profile below is a working assumption to test
against real users, not a finding.

## Who they are

A React Native developer who has shipped iOS or Android apps and now has to put
an app on a QNX board, usually a Raspberry Pi 4 or 5 running the QNX Everywhere
QSTI image, sometimes an automotive or industrial board from their employer.

They know: Node, a package manager, Metro, React, `npx`-style tooling, reading
a stack trace.

They usually do not know: QNX SDP licensing and QNX Software Center, what
`qnxsdp-env.sh` does, cross-compiling with `qcc`, Screen, Qt Quick, CMake
presets, or SSH deploys to a board without root.

## What they have installed

| Likely present                            | Likely missing or wrong                                                               |
| ----------------------------------------- | ------------------------------------------------------------------------------------- |
| macOS laptop, often Apple Silicon         | an x86-64 Linux host for QNX builds                                                   |
| Node from Homebrew or a version manager   | Node 26 specifically                                                                  |
| a global pnpm (often 9 or 10) or only npm | the pinned pnpm (12.18.0 since the main merge); Node 26 ships no Corepack to fetch it |
| Xcode command line tools, maybe CMake     | Ninja, CMake 3.31.9+, Qt 6.11.1+ host tools                                           |
| nothing QNX                               | a QNX license, SDP 8.0.5, the board's QSTI package                                    |

## Where the guides lost them

Ordered by where the walk stopped first. The walk ran when the pin was pnpm
12.8.1; the repository now pins 12.18.0.

1. **Installing dependencies.** The README said "use pnpm 12.8.1" and went
   straight to `pnpm install`. With Homebrew pnpm 10 in a fresh HOME, pnpm tried
   to switch itself to 12.8.1 and failed with `ENOEXEC`. A React Native developer
   has no reason to know Corepack is gone from Node 26.
2. **Checking the checkout.** `pnpm test:js` failed on macOS at base, on files
   unrelated to anything the developer touched, and stopped before running the
   rest. A first-timer reads that as "my setup is broken".
3. **Running the doctor without the SDP.** One missing step (sourcing the SDP)
   produced eight errors, two with an empty path, none naming the command to
   run.
4. **Learning the host requirement late.** The fact that QNX SDP 8.0 has no
   macOS host tools appeared in `build-and-deploy.md`, not in the README. On a
   Mac with the SDP sourced, the doctor told the developer to put
   `QNX_HOST/usr/bin` on `PATH`, which cannot help.
5. **Stale numbers.** `build-and-deploy.md` listed React Native 0.88.0-rc.3,
   Expo 58.0.0 and Nitro 0.37.1 while the lockfile pins 0.88.1, 58.0.22 and Nitro
   1.0.0-alpha. A careful reader stops to work out which is true.
6. **Two names for one command.** `./scripts/doctor.sh` in one guide,
   `pnpm qnx doctor` in another; the CLI help pointed at `rnqnx`, which is not
   on `PATH` in a checkout.

What worked without help: clone, `validate:dependencies`, `init`, the second
install, `bundle-qnx`, `typecheck` and `validate:docs`. The JavaScript half of
the path is short once pnpm 12.8.1 is installed: about 25 seconds of commands,
including the 9-second first install.

## What changed in response

See the "Fixes on the branch" table in the simulation receipt. In short: the
README installs the pinned pnpm explicitly and states the Linux build host up
front; the doctor names one fix per failure, collapses cascades into `SKIPPED`
lines, checks Qt host tools and reports "no target configured" as skipped; CLI
errors name the next command.

## Open questions for real users

- How many arrive with a Mac only, and would a documented container or remote
  Linux host keep them, or do they stop at "you need Ubuntu x86-64"?
- Do they start from the stock Pi QSTI guide or from the CLI page? The README
  links both in the same paragraph.
- Is `pnpm qnx doctor` discovered before the first failure, or only after?
