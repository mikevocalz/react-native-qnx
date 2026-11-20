# @rn-qnx/metro-config-marathon

Metro configuration for the `marathon` platform. It is opt-in and leaves the
QNX configuration unchanged.

```js
const { getMarathonMetroConfig } = require('@rn-qnx/metro-config-marathon');
module.exports = getMarathonMetroConfig(__dirname, {/* project overrides */});
```

Build with `metro build index.js --config metro.marathon.config.js --platform marathon`.

For a React Native `Libraries/` file, resolution order is the Marathon override
in `@rn-qnx/react-native-marathon`, then the QNX override, then React Native's
own file. Relative imports of dev-only modules that exist only as `.ios.js` or
`.android.js` resolve to the shared `src/private` copies, and the Expo
polyfill entry resolves to the shared polyfill.
