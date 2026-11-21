// RNQNX_IMAGE_URI is set by the test before this runs.
globalThis.imageResult = 'pending';
nativeModuleProxy.ImageLoader.getSize(globalThis.RNQNX_IMAGE_URI).then(
  (size) => {
    globalThis.imageWidth = size[0];
    globalThis.imageHeight = size[1];
    globalThis.imageResult = 'resolved';
  },
  (error) => {
    globalThis.imageResult = 'rejected: ' + error;
  }
);
