# **APP_NAME**

From the repository root, install workspace links with
`pnpm install --no-frozen-lockfile`, then build this app's production bundle:

```bash
pnpm qnx bundle-qnx --app __APP_NAME__
```

The bundle is `build/__APP_NAME__.bundle.js`. The existing native playground
runner starts the `RNQnxPlayground` surface, so keep that registration name in
`index.js`. The app uses no component state; use Zustand when adding state.

This is a source-workspace template. Native compilation, Qt provisioning,
runtime dependencies and QNX deployment are separate steps in
[the CLI guide](../../docs/getting-started/qnx-cli.md). Creating the app does
not establish device compatibility or alter the native window size.
