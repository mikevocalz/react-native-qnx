# Check an external project's metadata

From the source checkout providing the CLI, inspect a consumer project before
attempting its platform build. QNX is the default target:

```bash
pnpm qnx project doctor --project /path/to/app
pnpm qnx project doctor --project /path/to/app --platform marathon --json
```

The command checks the running Node version, project manifest shape, selected
entry/config file presence and a bounded list of installed package identities
against the reviewed platform graph. It reads package metadata without
importing package JavaScript, Metro configuration or application entry code.
It does not start a package manager, compiler, network request or device command.

The default entry is `index.js`; select another project-relative file with
`--entry src/main.js`. QNX uses `metro.config.js`; Marathon uses
`metro.marathon.config.js`. Project input symlinks and escaping paths are
rejected. Normal package-manager directory links are resolved for dependency
metadata, including pnpm's installed graph. Metadata reads are limited to
one MiB. The caller's filesystem is trusted; this is not a sandbox against
concurrent replacement of files.

Each result is `passed`, `failed` or `not-checked`. The Node minimum is enforced,
not overridden for a local test. A supplied `packageManager` declaration must
match the reviewed string; omission is reported as not checked. No installed
package-manager executable is probed. A hidden package.json export is a
metadata inspection failure, not a claim the dependency is absent.

Exit 0 means the inspected metadata matches. Exit 1 means at least one check
failed. Invalid command options or an unreadable project root return exit 2.
Use `--json` for the structured report. Correct failed package versions as a
matched platform graph; this command never installs or upgrades dependencies.
Its contract includes a reviewed source revision, not an assertion of npm latest.

A compatible report deliberately keeps `nativeBuildVerified` and
`runtimeVerified` false. It does not inspect QNX SDP, Qt/plugin/libc closure,
all third-party native modules, Nitro ABI, permissions, actual bundling,
deployment, Router, lifecycle, debugger attachment or Fast Refresh. Those
checks remain explicitly not checked, not green. No broad readiness claim
should be derived from this command.

CI exercises both fixture failures/no-execution behavior and reports from
actual installed workspace packages linked into an external source directory.
That is not an independently installed published package or a physical-device
receipt. No dependency version, boot configuration, native runtime or premium
product is changed. Revert the doctor command and its focused files to remove
this slice; consumer source and dependencies remain untouched.
