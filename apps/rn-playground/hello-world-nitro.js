/** Run the existing synchronous QNX Nitro v1 proof before mounting the app. */
const { NitroModules } = require('nitro-core');
const QnxPlatformInfo = NitroModules.getHybridClass('QnxPlatformInfo');
const info = new QnxPlatformInfo();
if (info.platform !== 'qnx' || info.architecture !== 'aarch64' || info.add(20, 22) !== 42) {
  throw new Error('QNX Nitro v1 native binding verification failed');
}
console.log(
  `RNQNX_HELLO_NITRO_V1_OK platform=${info.platform} architecture=${info.architecture} add=42`
);
require('./hello-world');
