(() => {
  const PlatformInfo = NitroModulesProxy.getHybridClass('QnxPlatformInfo');
  const platform = new PlatformInfo();
  return platform.platform + '|' + platform.architecture + '|' + platform.add(20, 22);
})();
