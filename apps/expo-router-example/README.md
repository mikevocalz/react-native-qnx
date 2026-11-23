# Expo Router example

The expo-router example app for QNX: a left drawer (Home, Settings, About) around three tabs (Home, Explore, Controls). `pnpm qnx init expo-router-example` created the app shell, and the routes come from `template/qnx-router`, the same starter that `project create` writes for a QNX-only project. `apps/component-gallery` is the larger Router app; this one stays at the starter's size.

```text
app/_layout.js         Drawer (navigation only)
app/(tabs)/_layout.js  Tabs: bottom bar under 700 px, left rail from 700 px
app-ctx.js             require.context of ./app (replaces expo-router/_ctx)
index.js               registers RNQnxPlayground, the surface the host starts
app.json               name, scheme and extra.rnqnx identity and window settings
```

From the repository root:

```bash
pnpm install --frozen-lockfile
pnpm qnx bundle-qnx --app expo-router-example
```

Host checks (macOS or Linux, no QNX SDP): `tests/js/expo-router-example.test.mjs` bundles the app for `qnx`, and `pnpm validate:expo-sdk58-support` checks the support list.

Not shown by anything in this directory: the app running on the Pi 400, the QNX VM or a release board, and Fast Refresh on a device. The `expo` gate keeps those as pending.
