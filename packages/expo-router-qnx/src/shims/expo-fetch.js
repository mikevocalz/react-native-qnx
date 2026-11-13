// expo/fetch for QNX. Expo's runtime installs `fetch` from a native
// ExpoFetchModule that this host does not have. Keep React Native's own
// fetch, which goes through the Networking module, so importing `expo` does
// not replace a working global with one that throws.
module.exports = { fetch: require('react-native/Libraries/Network/fetch').fetch };
