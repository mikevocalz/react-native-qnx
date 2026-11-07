globalThis.rejected = 0;
nativeRejected.catch((error) => {
  if (error instanceof Error && error.message.includes('native failure')) {
    rejected = 42;
  }
});
