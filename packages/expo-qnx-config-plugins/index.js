'use strict';

// Native app configuration for QNX and Marathon comes from this plugin only.
// It writes plain values into the Expo config (extra.rnqnx); prepare-native reads
// the resolved config through @expo/config and generates native-app.json. Nothing
// here edits generated native files, so running prebuild twice is a no-op.

const WINDOW_CONTROLS = Object.freeze(['macos', 'windows']);
const TITLE_BAR_STYLES = Object.freeze(['default', 'hidden', 'none']);
const UNAVAILABLE_CONTROLS = Object.freeze(['hide', 'disable']);
const IDENTITY_KEYS = Object.freeze(['appId', 'componentName', 'shortName', 'icon']);
const MAX_WINDOW_EDGE = 16384;

/** @param {unknown} value */
function isPlainObject(value) {
  return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

/**
 * Validate and fill defaults for extra.rnqnx.window. Throws on unknown keys or
 * values so a typo in app.config fails at prebuild instead of on the device.
 * @param {unknown} value
 */
function resolveQnxWindow(value) {
  // Only undefined means "not set". null is rejected everywhere, matching the
  // CMake and C++ readers of native-app.json.
  if (value === undefined) value = {};
  if (!isPlainObject(value)) throw new Error('extra.rnqnx.window must be an object.');
  const input = /** @type {Record<string, unknown>} */ (value);
  for (const key of Object.keys(input))
    if (!['controls', 'titleBarStyle', 'initialSize', 'unavailableControls'].includes(key))
      throw new Error(`extra.rnqnx.window.${key} is not a supported window option.`);
  const controls = input.controls === undefined ? 'macos' : input.controls;
  if (typeof controls !== 'string' || !WINDOW_CONTROLS.includes(controls))
    throw new Error(`extra.rnqnx.window.controls must be one of: ${WINDOW_CONTROLS.join(', ')}.`);
  const titleBarStyle = input.titleBarStyle === undefined ? 'default' : input.titleBarStyle;
  if (typeof titleBarStyle !== 'string' || !TITLE_BAR_STYLES.includes(titleBarStyle))
    throw new Error(
      `extra.rnqnx.window.titleBarStyle must be one of: ${TITLE_BAR_STYLES.join(', ')}.`
    );
  /** @type {{controls: string, titleBarStyle: string, initialSize?: {width: number, height: number}, unavailableControls?: string}} */
  const resolved = { controls, titleBarStyle };
  if (input.unavailableControls !== undefined) {
    const mode = input.unavailableControls;
    if (typeof mode !== 'string' || !UNAVAILABLE_CONTROLS.includes(mode))
      throw new Error(
        `extra.rnqnx.window.unavailableControls must be one of: ${UNAVAILABLE_CONTROLS.join(', ')}.`
      );
    resolved.unavailableControls = mode;
  }
  if (input.initialSize !== undefined) {
    const size = input.initialSize;
    if (!isPlainObject(size))
      throw new Error('extra.rnqnx.window.initialSize must be {width, height}.');
    const { width, height, ...rest } = /** @type {Record<string, unknown>} */ (size);
    if (Object.keys(rest).length)
      throw new Error('extra.rnqnx.window.initialSize accepts only width and height.');
    for (const [label, edge] of [
      ['width', width],
      ['height', height],
    ])
      if (
        !Number.isInteger(edge) ||
        /** @type {number} */ (edge) < 1 ||
        /** @type {number} */ (edge) > MAX_WINDOW_EDGE
      )
        throw new Error(
          `extra.rnqnx.window.initialSize.${label} must be an integer from 1 to ${MAX_WINDOW_EDGE}.`
        );
    resolved.initialSize = {
      width: /** @type {number} */ (width),
      height: /** @type {number} */ (height),
    };
  }
  return resolved;
}

/**
 * Expo config plugin. Plugin options win over values already in extra.rnqnx, so
 * the plugin entry in app.json / app.config.* is the one place to set them.
 */
function withQnxProject(config, options = {}) {
  if (!isPlainObject(options)) throw new Error('withQnxProject options must be an object.');
  const current = config.extra?.rnQnx || {};
  const rnqnx = { ...(config.extra?.rnqnx || {}) };
  for (const key of IDENTITY_KEYS) {
    if (options[key] === undefined) continue;
    if (typeof options[key] !== 'string' || !options[key].trim())
      throw new Error(`@rn-qnx/expo-qnx-config-plugins option ${key} must be a nonempty string.`);
    rnqnx[key] = options[key];
  }
  if (options.window === null || rnqnx.window === null)
    throw new Error('extra.rnqnx.window must be an object, not null.');
  if (options.window !== undefined || rnqnx.window !== undefined)
    rnqnx.window = resolveQnxWindow({ ...(rnqnx.window ?? {}), ...(options.window ?? {}) });
  const extra = {
    ...config.extra,
    rnQnx: {
      ...current,
      targetArch: options.targetArch || current.targetArch || 'aarch64',
      autolinkingJson:
        options.autolinkingJson ||
        current.autolinkingJson ||
        'build/generated/expo-qnx-modules.json',
      autolinkingCMake:
        options.autolinkingCMake ||
        current.autolinkingCMake ||
        'build/generated/expo-qnx-modules.cmake',
    },
  };
  if (Object.keys(rnqnx).length) extra.rnqnx = rnqnx;
  return { ...config, extra };
}

module.exports = withQnxProject;
module.exports.withQnxProject = withQnxProject;
module.exports.resolveQnxWindow = resolveQnxWindow;
module.exports.WINDOW_CONTROLS = WINDOW_CONTROLS;
module.exports.TITLE_BAR_STYLES = TITLE_BAR_STYLES;
