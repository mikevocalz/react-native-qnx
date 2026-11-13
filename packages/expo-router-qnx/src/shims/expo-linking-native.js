// expo-linking's ExpoLinking native module for QNX. A desktop window has no
// launch URL and no URL events: getLinkingURL() is null, listeners never fire.
module.exports = {
  __esModule: true,
  default: {
    addListener() {
      return { remove() {} };
    },
    getLinkingURL() {
      return null;
    },
    clearInitialURL() {},
  },
};
