(() => {
  const platform = expo.modules.ExpoQnxPlatform;
  return platform.platform + '|' + platform.architecture;
})();
