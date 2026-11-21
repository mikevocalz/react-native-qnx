// The gallery is an expo-router app: ./app holds the routes (a drawer around a
// tabs group). This file only registers the root; the route modules live in
// app-ctx.js so Fast Refresh can swap a route without reloading the app.
const { AppRegistry, LogBox } = require('react-native');
const { App } = require('expo-router/build/qualified-entry');

// scripts/vm-proof/build-bundles.sh replaces this marker in the plain-JS bundle
// with one Advanced card id to make a single-card app for screenshots. The
// marker stays untouched in every other build, which runs the full gallery.
const PROOF_CARD = '__RNQNX_PROOF_CARD__';

// A host built before app.json gained `scheme` embeds the older config, and
// expo-linking warns about it; the warning goes away with the next native build.
LogBox.ignoreLogs(['Linking requires a build-time setting']);

if (PROOF_CARD.startsWith('__')) {
  AppRegistry.registerComponent('RNQnxPlayground', () => App);
} else {
  AppRegistry.registerComponent('RNQnxPlayground', () =>
    require('./proof/ProofRoot').makeProofRoot(PROOF_CARD)
  );
}
// The SVG cards alone, for the device screenshot check; see ./svg/standalone.js.
AppRegistry.registerComponent('RNQnxSvgGallery', () => require('./svg/standalone').SvgGalleryApp);
