// App-side API of @rn-qnx/expo-router-qnx, imported from routes. The Metro
// helper (node-only) is in ../metro.js.
const { qnxTabsScreenOptions } = require('./QnxTabs');
const { setSwipeExclusionLeft } = require('./swipeExclusion');

module.exports = {
  qnxTabsScreenOptions,
  setSwipeExclusionLeft,
  // Metro config and Node tooling can import the adapter without loading React
  // or Expo Router's native app environment. Entry details load only on use.
  get QnxRouterProvider() {
    return require('./RouterEntry').QnxRouterProvider;
  },
};
