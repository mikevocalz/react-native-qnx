// Reads the `window` object that prepare-native writes into native-app.json
// (PR #122 shape). The same rules as resolveQnxWindow in
// @rn-qnx/expo-qnx-config-plugins, cmake/RnqnxConsumerApp.cmake and
// rnqnx::window::parseWindowChromeConfig, so a bad descriptor fails here
// before CMake runs. See docs/adr/0016.

export const WINDOW_CONTROLS = Object.freeze(['macos', 'windows']);
export const TITLE_BAR_STYLES = Object.freeze(['default', 'hidden', 'none']);
export const MAX_WINDOW_EDGE = 16384;

/** @typedef {{controls: 'macos'|'windows', titleBarStyle: 'default'|'hidden'|'none', initialSize: {width: number, height: number} | null, legacy: boolean}} WindowDescriptor */

/** @param {unknown} value */
function isPlainObject(value) {
  return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

/**
 * @param {unknown} descriptor parsed native-app.json
 * @returns {WindowDescriptor}
 */
export function readWindowDescriptor(descriptor) {
  if (!isPlainObject(descriptor)) throw new Error('native-app.json must be an object.');
  const object = /** @type {Record<string, unknown>} */ (descriptor);
  if (!('window' in object)) {
    // Written before the field existed: keep the chromeless window.
    return { controls: 'macos', titleBarStyle: 'none', initialSize: null, legacy: true };
  }
  const window = object.window;
  if (!isPlainObject(window)) throw new Error('native-app.json window must be an object.');
  const input = /** @type {Record<string, unknown>} */ (window);
  for (const key of Object.keys(input))
    if (!['controls', 'titleBarStyle', 'initialSize'].includes(key))
      throw new Error(`native-app.json window.${key} is not a supported window option.`);
  const controls = input.controls ?? 'macos';
  if (typeof controls !== 'string' || !WINDOW_CONTROLS.includes(controls))
    throw new Error(
      `native-app.json window.controls must be one of: ${WINDOW_CONTROLS.join(', ')}.`
    );
  const titleBarStyle = input.titleBarStyle ?? 'default';
  if (typeof titleBarStyle !== 'string' || !TITLE_BAR_STYLES.includes(titleBarStyle))
    throw new Error(
      `native-app.json window.titleBarStyle must be one of: ${TITLE_BAR_STYLES.join(', ')}.`
    );
  let initialSize = null;
  if (input.initialSize !== undefined) {
    const size = input.initialSize;
    if (!isPlainObject(size))
      throw new Error('native-app.json window.initialSize must be {width, height}.');
    const keys = Object.keys(/** @type {object} */ (size));
    if (keys.length !== 2 || !keys.includes('width') || !keys.includes('height'))
      throw new Error('native-app.json window.initialSize must be {width, height}.');
    const { width, height } = /** @type {{width: unknown, height: unknown}} */ (size);
    for (const [label, edge] of /** @type {const} */ ([
      ['width', width],
      ['height', height],
    ]))
      if (
        !Number.isInteger(edge) ||
        /** @type {number} */ (edge) < 1 ||
        /** @type {number} */ (edge) > MAX_WINDOW_EDGE
      )
        throw new Error(
          `native-app.json window.initialSize.${label} must be an integer from 1 to ${MAX_WINDOW_EDGE}.`
        );
    initialSize = { width: /** @type {number} */ (width), height: /** @type {number} */ (height) };
  }
  return {
    controls: /** @type {'macos'|'windows'} */ (controls),
    titleBarStyle: /** @type {'default'|'hidden'|'none'} */ (titleBarStyle),
    initialSize,
    legacy: false,
  };
}

/**
 * CMake flags for the opt-in chrome. The chrome is compiled only when the
 * developer asks for it; a descriptor alone never turns it on. OFF is passed
 * explicitly because build-native reuses build/<platform>/native, whose CMake
 * cache would otherwise keep an earlier ON.
 * @param {WindowDescriptor} window
 * @param {boolean} enabled
 */
export function windowChromeCMakeFlags(window, enabled) {
  if (!enabled) return ['-DRNQNX_ENABLE_WINDOW_CHROME=OFF'];
  if (window.titleBarStyle === 'none')
    throw new Error(
      '--window-chrome has nothing to draw: the descriptor has no window or titleBarStyle "none".'
    );
  return ['-DRNQNX_ENABLE_WINDOW_CHROME=ON'];
}
