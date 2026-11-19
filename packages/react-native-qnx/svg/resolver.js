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
 * - Files of this directory (the overlay, the extractors, the Fabric specs)
 *   name package modules as `react-native-svg/src/...`. They may sit where
 *   that package is not reachable (pnpm installs it beside the app, not
 *   beside this package), so such a request is resolved from the package's own
 *   root, learned from the first package file Metro resolved. These files only
 *   load after one has been, since the overlay replaces files the package
 *   imports.
 *
 * The overlay replaces whole files, so it is tied to one package version and
 * one installed copy of it: another version, or a second copy, throws.
 */

const fs = require('node:fs');
const path = require('node:path');

const OVERLAY_ROOT = path.join(__dirname, 'overlay');
const EXPECTED_VERSION = '15.16.0';
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
 * @param {{
 *   platforms?: readonly string[],
 *   overlayRoot?: string,
 *   packageVersion?: (root: string) => string | null,
 * }} [options] `packageVersion` reads the installed version, for tests that
 *   resolve paths which are not on disk.
 * @returns {(context: any, moduleName: string, platform: string | null) => any}
 */
function createSvgOverlayResolveRequest(fallback, options = {}) {
  const { platforms = ['qnx'], overlayRoot = OVERLAY_ROOT, packageVersion = readVersion } = options;
  const overlay = scanOverlay(overlayRoot);
  const overlayPrefix = overlayRoot + path.sep;
  const ownPrefix = path.dirname(overlayRoot) + path.sep;
  const packagePrefix = `${PACKAGE_NAME}/`;
  /** @type {string | null} */
  let packageRoot = null;

  /** @param {string} root */
  const learn = (root) => {
    if (packageRoot === root) return;
    if (packageRoot !== null) {
      throw new Error(
        `Two copies of ${PACKAGE_NAME} are installed (${packageRoot} and ${root}). The QNX overlay supports one.`
      );
    }
    const version = packageVersion(root);
    if (version !== EXPECTED_VERSION) {
      throw new Error(
        `The QNX ${PACKAGE_NAME} overlay replaces whole files of ${EXPECTED_VERSION}, but ${root} is ${version ?? 'unreadable'}. Re-audit packages/react-native-qnx/svg before upgrading.`
      );
    }
    packageRoot = root;
  };

  return function resolveRequest(context, moduleName, platform) {
    const origin = context.originModulePath;
    const fromOverlay = typeof origin === 'string' && origin.startsWith(overlayPrefix);
    const fromOwn = typeof origin === 'string' && origin.startsWith(ownPrefix);

    // One of this directory's files asking for the package by its public name.
    if (
      fromOwn &&
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
    learn(match[1]);
    if (fromOverlay) return resolution;
    const key = match[2].replace(/\\/g, '/').replace(SOURCE_EXTENSIONS, '');
    const replacement = overlay.get(key);
    return replacement ? { type: 'sourceFile', filePath: replacement } : resolution;
  };
}

/** @param {string} root */
function readVersion(root) {
  try {
    return JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8')).version ?? null;
  } catch {
    return null;
  }
}

module.exports = { createSvgOverlayResolveRequest, scanOverlay, OVERLAY_ROOT };
