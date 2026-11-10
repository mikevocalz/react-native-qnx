import { createRequire } from 'node:module';
import { writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
const require = createRequire(import.meta.url);
const {
  generateExpoPlatformCMake,
  resolveExpoSearchResults,
} = require('../../../packages/expo-qnx-autolinking/src');
const platform = process.argv[3] || 'marathon';
const descriptors = resolveExpoSearchResults(
  {
    contract: {
      path: fileURLToPath(new URL('./provider', import.meta.url)),
      version: '1.0.0',
      config: {
        platforms: [platform],
        [platform]: {
          target: 'rnqnx_expo_test_provider',
          modules: ['ExpoAutolinkContract'],
          registrationFunction: 'rnqnxRegisterContractProvider',
        },
      },
    },
  },
  platform
);
writeFileSync(process.argv[2], generateExpoPlatformCMake(descriptors, platform));
