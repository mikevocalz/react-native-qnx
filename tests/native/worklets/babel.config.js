const path = require('node:path');
if (!process.env.RNQNX_WORKLETS_DIR)
  throw new Error('Set RNQNX_WORKLETS_DIR to audited Worklets 0.13.1');
module.exports = {
  presets: [[require.resolve('babel-preset-expo'), { worklets: false, reanimated: false }]],
  plugins: [path.join(process.env.RNQNX_WORKLETS_DIR, 'plugin')],
};
