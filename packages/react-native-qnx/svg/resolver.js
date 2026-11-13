'use strict';

/**
 * Metro resolution for the react-native-svg overlay.
 *
 * react-native-svg 15.16.0 does not send every SVG feature to its host, and its
 * files cannot be edited in node_modules. The overlay directory mirrors the
 * package's `src/` tree: `overlay/elements/filters/FeTurbulence.tsx` replaces
 * `react-native-svg/src/elements/filters/FeTurbulence.tsx`. This resolver
 * swaps a resolved package file for its overlay twin, on the platforms it is
 * given and no others.
 *
 * Two rules keep the overlay usable:
 *
 * - A module inside the overlay is never redirected, so an overlay file can
 *   import the original it replaces (`extractGradient` wraps the package's
 *   function) without resolving back to itself.
 * - An overlay file names package modules as `react-native-svg/src/...`. The
 *   overlay may sit where that package is not reachable (pnpm installs it
 *   beside the app, not beside this package), so such a request is resolved
 *   from the package's own root, learned from the first package file Metro
 *   resolved. The overlay only loads after one has been, since it replaces
 *   files the package imports.
 */

const fs = require('node:fs');
const path = require('node:path');

const OVERLAY_ROOT = path.join(__dirname, 'overlay');
const PACKAGE_NAME = 'react-native-svg';
const SOURCE_EXTENSIONS = /\.(?:tsx?|jsx?)$/;
// `<root>/react-native-svg/src/<relative>`, with either path separator.
const PACKAGE_SOURCE = /^(.*[\\/]react-native-svg)[\\/]src[\\/](.+)$/;

/**
 * Overlay files keyed by the package-relative path they replace, with
 * forward slashes and no extension: `elements/filters/Filter`.
 * @param {string} directory
 * @returns {Map<string, string>}
 */
function scanOverlay(directory) {
  /** @type {Map<string, string>} */
  const found = new Map();
  const walk = (/** @type {string} */ dir) => {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      // Editor and archive litter such as macOS `._name` files.
      if (entry.name.startsWith('.')) continue;
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        walk(full);
      } else if (SOURCE_EXTENSIONS.test(entry.name)) {
        const relative = path.relative(directory, full).split(path.sep).join('/');
        found.set(relative.replace(SOURCE_EXTENSIONS, ''), full);
      }
    }
  };
  walk(directory);
  return found;
}

/**
 * @param {(context: any, moduleName: string, platform: string | null) => any} fallback
 *   The resolver this one wraps. It runs first, so the overlay is decided by
 *   where the package module resolved, which makes relative imports work.
 * @param {{ platforms?: readonly string[], overlayRoot?: string }} [options]
 * @returns {(context: any, moduleName: string, platform: string | null) => any}
 */
function createSvgOverlayResolveRequest(fallback, options = {}) {
  const { platforms = ['qnx'], overlayRoot = OVERLAY_ROOT } = options;
  const overlay = scanOverlay(overlayRoot);
  const overlayPrefix = overlayRoot + path.sep;
  const packagePrefix = `${PACKAGE_NAME}/`;
  /** @type {string | null} */
  let packageRoot = null;

  return function resolveRequest(context, moduleName, platform) {
    const origin = context.originModulePath;
    const fromOverlay = typeof origin === 'string' && origin.startsWith(overlayPrefix);

    // An overlay file asking for the package by its public name.
    if (
      fromOverlay &&
      packageRoot !== null &&
      platform !== null &&
      platforms.includes(platform) &&
      moduleName.startsWith(packagePrefix)
    ) {
      const absolute = path.join(packageRoot, moduleName.slice(packagePrefix.length));
      return fallback(context, absolute, platform);
    }

    const resolution = fallback(context, moduleName, platform);
    if (platform === null || !platforms.includes(platform) || resolution?.type !== 'sourceFile') {
      return resolution;
    }
    const match = PACKAGE_SOURCE.exec(resolution.filePath);
    if (!match) return resolution;
    packageRoot = match[1];
    if (fromOverlay) return resolution;
    const key = match[2].replace(/\\/g, '/').replace(SOURCE_EXTENSIONS, '');
    const replacement = overlay.get(key);
    return replacement ? { type: 'sourceFile', filePath: replacement } : resolution;
  };
}

module.exports = { createSvgOverlayResolveRequest, scanOverlay, OVERLAY_ROOT };
