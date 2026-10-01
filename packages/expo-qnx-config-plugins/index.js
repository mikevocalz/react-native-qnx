'use strict';

function withQnxProject(config, options = {}) {
  const current = config.extra?.rnQnx || {};
  return {
    ...config,
    extra: {
      ...config.extra,
      rnQnx: {
        ...current,
        targetArch: options.targetArch || current.targetArch || 'aarch64',
        autolinkingJson:
          options.autolinkingJson ||
          current.autolinkingJson ||
          'build/generated/expo-qnx-modules.json',
        autolinkingCMake:
          options.autolinkingCMake ||
          current.autolinkingCMake ||
          'build/generated/expo-qnx-modules.cmake',
      },
    },
  };
}

module.exports = withQnxProject;
module.exports.withQnxProject = withQnxProject;
