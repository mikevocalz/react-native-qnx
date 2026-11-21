globalThis.fileReaderResult = 'pending';
nativeModuleProxy.FileReaderModule.readAsText({}, 'utf8').then(
  () => {
    globalThis.fileReaderResult = 'resolved';
  },
  (error) => {
    globalThis.fileReaderResult = 'rejected: ' + error.message;
  }
);
