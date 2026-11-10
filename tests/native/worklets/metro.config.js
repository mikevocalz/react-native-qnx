const path = require('node:path');
const base =
  process.env.RNQNX_WORKLETS_PLATFORM === 'marathon'
    ? require('../../../apps/rn-playground/metro.public.marathon.config')
    : require('../../../apps/rn-playground/metro.public.config');
const worklets = process.env.RNQNX_WORKLETS_DIR;
if (!worklets) throw new Error('Set RNQNX_WORKLETS_DIR');
module.exports = {
  ...base,
  projectRoot: __dirname,
  watchFolders: [
    ...base.watchFolders,
    path.dirname(worklets),
    path.dirname(require('node:fs').realpathSync(path.resolve(__dirname, '../../../node_modules'))),
  ],
  resolver: {
    ...base.resolver,
    extraNodeModules: { ...base.resolver.extraNodeModules, 'react-native-worklets': worklets },
  },
};
