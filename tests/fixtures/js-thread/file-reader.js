globalThis.fileReaderResult = 'pending';
__turboModuleProxy('FileReaderModule').readAsText({}, 'utf8').then(
  () => { globalThis.fileReaderResult = 'resolved'; },
  (error) => { globalThis.fileReaderResult = 'rejected: ' + error.message; });
