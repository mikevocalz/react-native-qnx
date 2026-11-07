# React Pressable probe fixtures

The probe is behind the existing default-OFF
`RNQNX_ENABLE_INPUT_LIFECYCLE` option. Its production Metro fixture imports real
React Native Pressable. The raw Fabric payload probe remains separate.

By default, host and QNX builds generate the Metro bundle. A cross-build
environment without Node may consume a bundle generated on the development
host from the same patched, frozen dependency graph:

```sh
pnpm install --frozen-lockfile
cd apps/rn-playground
../../node_modules/.bin/metro build pressable-probe.js --platform qnx --dev false --out /absolute/path/pressable.bundle.js
```

Copy that bundle into the cross-build environment, then add these options to
the existing full QNX graph configuration:

```sh
-DRNQNX_ENABLE_INPUT_LIFECYCLE=ON
-DRNQNX_INPUT_TEST_HERMESC=/absolute/path/to/pinned-host-hermesc
-DRNQNX_PRESSABLE_TEST_BUNDLE=/absolute/path/pressable.bundle.js
```

`RNQNX_PRESSABLE_TEST_BUNDLE` is a QNX-only cached file path, empty by default.
A missing file or directory is rejected during configuration. An explicit
existing file bypasses Metro generation only; the pinned host Hermes compiler
still produces `pressable-probe.hbc`, which is a dependency of
`rnqnx_pressable_probe`. Replacing the input bundle rebuilds its bytecode. Host
builds retain their normal Metro path.

Record the source commit, lockfile hash, bundle hash, compiler identity and
deployed bytecode hash with target results. Supplying a file is not proof of
its provenance or of a target run. The probe tests synthetic Qt input and React
callbacks; it does not establish physical HID, RNGH or Worklets support.
