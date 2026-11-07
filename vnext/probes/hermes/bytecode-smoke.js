// Compile with the pinned host hermesc; run on the QNX lean VM.
(() => {
  if (1 + 1 !== 2) throw new Error('arithmetic');
  class Counter {
    constructor(n) {
      this.n = n;
    }
    add(...xs) {
      return xs.reduce((a, b) => a + b, this.n);
    }
  }
  const { n } = new Counter(10);
  if (new Counter(n).add(1, 2, 3) !== 16) throw new Error('es6');
  let seen = 0;
  const bump = async () => {
    seen = 7;
  };
  bump();
  if (seen !== 7) throw new Error('async-arrow');
  if (doubler(21) !== 42) throw new Error('host-function');
  return 42;
})();
