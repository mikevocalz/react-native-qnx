# QNX component gallery

The tabbed component gallery (Components, Documents, Sections, Controls,
Adaptive, Limits) that ran on the Pi 400 on 2026-11-10, taken from `083c4bd`
on `devin/1794282758-v1-integration`. It uses the public `react-native`
entry through `@rn-qnx/metro-config-qnx/public-entry`. The leaner fixture in
`apps/component-conformance` is unchanged.

```bash
node packages/react-native-qnx-cli/src/bin.mjs bundle-qnx --app component-gallery \
  --out build/gallery/app.bundle.js
"$RNQNX_HOST_HERMESC" -O -emit-binary -out build/gallery/app.hbc build/gallery/app.bundle.js
```

Stage `assets/icon.png` as `assets/assets/icon.png` next to the bundle; the
Image card loads it from there.

The app opens as a 1280 x 800 window (`extra.rnqnx.window.initialSize`) with
the macOS-style title bar. The Components page lays its cards out in one, two
or three columns from the measured window width. Building and running it on
the Pi 400 is in [the Pi 400 desktop guide](../../docs/getting-started/pi400-desktop-app.md).
