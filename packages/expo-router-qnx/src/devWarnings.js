// One development warning per option name. The router options below have no
// QNX behavior to give; the warning names the option so a value that does
// nothing is not mistaken for one that works. Silent in release builds.
const warned = new Set();

/**
 * @param {string} name The option or prop name.
 * @param {string} reason Why the platform cannot honor it.
 */
function warnUnsupportedOnce(name, reason) {
  if (typeof __DEV__ === 'undefined' || !__DEV__ || warned.has(name)) return;
  warned.add(name);
  console.warn(`[expo-router-qnx] "${name}" has no effect on QNX: ${reason}`);
}

/**
 * Warns for each key of `table` that `values` sets to something other than
 * undefined (and, when the table entry has `when`, that passes the check).
 * @param {Record<string, any> | undefined} values
 * @param {Record<string, { reason: string, when?: (value: any) => boolean }>} table
 */
function warnUnsupportedOptions(values, table) {
  if (!values) return;
  for (const name of Object.keys(table)) {
    const value = values[name];
    if (value === undefined) continue;
    const { reason, when } = table[name];
    if (!when || when(value)) warnUnsupportedOnce(name, reason);
  }
}

/** For tests: forget what was warned. */
function resetWarnings() {
  warned.clear();
}

module.exports = { warnUnsupportedOnce, warnUnsupportedOptions, resetWarnings };
