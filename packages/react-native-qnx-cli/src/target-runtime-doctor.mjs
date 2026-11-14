// Offline QNX target observations. No report field becomes code, a command,
// a host filesystem lookup or a claim that this process inspected a device.
import { constants } from 'node:fs';
import { lstat, open } from 'node:fs/promises';
import path from 'node:path';

const MAX_BYTES = 64 * 1024;
const ENV_KEYS = [
  'LD_LIBRARY_PATH',
  'QT_PLUGIN_PATH',
  'QML2_IMPORT_PATH',
  'QT_QPA_FONTDIR',
  'QT_QPA_QNX_DISPLAY_CONFIG',
  'QQNX_PHYSICAL_SCREEN_SIZE',
];

/** @typedef {{id: string, status: 'passed'|'failed'|'not-checked', message: string}} TargetCheck */
/** @typedef {{schema: string, capturedAt: string, target: {board: string, image: string, sdp: string},
 * services: {screen: boolean, slogger2: boolean}, tmp: boolean, random: boolean,
 * ipv6: boolean, fontconfig: boolean, directories: string[], files: string[],
 * environment: Record<string, string>, resolvedDirectories: {libraries: string, plugins: string, qml: string, fonts: string[]}, display: {widthMm: number, heightMm: number,
 * configurationVerified: boolean}}} Observation */
/** @typedef {{schema: string, platform: 'qnx', evidenceTier: 'offline-observation',
 * capturedAt: string, target: Observation['target'], reportedPrerequisitesSatisfied: boolean,
 * observationOrigin: 'user-supplied', collectionVerified: false, runtimeVerified: false, checks: TargetCheck[], debugEnvironment?: Record<string, string>}} TargetReport */
/** @typedef {{ok: true, report: TargetReport} | {ok: false,
 * step: 'platform'|'read-report'|'parse-report'|'validate-report', message: string}} TargetResult */

/** @param {unknown} value @param {string[]} keys */
function shape(value, keys) {
  return (
    value !== null &&
    typeof value === 'object' &&
    !Array.isArray(value) &&
    Object.keys(value).length === keys.length &&
    keys.every((key) => Object.hasOwn(value, key))
  );
}
/** @param {unknown} value @param {number} max */
function text(value, max) {
  return typeof value === 'string' && value.length <= max && !/[\x00-\x1f\x7f]/.test(value);
}
/** @param {unknown} value */
function absolutePath(value) {
  return (
    typeof value === 'string' &&
    text(value, 1024) &&
    value.startsWith('/') &&
    value !== '/' &&
    !value.includes('\\') &&
    path.posix.normalize(value) === value &&
    !value.endsWith('/')
  );
}
/** @param {unknown} value */
function pathList(value) {
  return (
    Array.isArray(value) &&
    value.length <= 128 &&
    value.every(absolutePath) &&
    new Set(value).size === value.length
  );
}
/** @param {unknown} input @returns {input is Observation} */
function validObservation(input) {
  if (
    !shape(input, [
      'schema',
      'capturedAt',
      'target',
      'services',
      'tmp',
      'random',
      'ipv6',
      'fontconfig',
      'directories',
      'files',
      'environment',
      'resolvedDirectories',
      'display',
    ])
  )
    return false;
  const value = /** @type {Observation} */ (input);
  if (value.schema !== 'rnqnx-qnx-runtime-observation/v1') return false;
  if (
    !text(value.capturedAt, 30) ||
    !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/.test(value.capturedAt) ||
    !Number.isFinite(Date.parse(value.capturedAt)) ||
    new Date(value.capturedAt).toISOString() !== value.capturedAt.replace('Z', '.000Z')
  )
    return false;
  if (
    !shape(value.target, ['board', 'image', 'sdp']) ||
    !Object.values(value.target).every((v) => text(v, 160) && v.trim())
  )
    return false;
  if (
    !shape(value.services, ['screen', 'slogger2']) ||
    !Object.values(value.services).every((v) => typeof v === 'boolean')
  )
    return false;
  if (![value.tmp, value.random, value.ipv6, value.fontconfig].every((v) => typeof v === 'boolean'))
    return false;
  if (
    !pathList(value.directories) ||
    !pathList(value.files) ||
    value.files.some((file) => value.directories.includes(file))
  )
    return false;
  if (
    !shape(value.environment, ENV_KEYS) ||
    !Object.values(value.environment).every((v) => text(v, 4096))
  )
    return false;
  if (
    !shape(value.resolvedDirectories, ['libraries', 'plugins', 'qml', 'fonts']) ||
    ![
      value.resolvedDirectories.libraries,
      value.resolvedDirectories.plugins,
      value.resolvedDirectories.qml,
    ].every(absolutePath) ||
    !pathList(value.resolvedDirectories.fonts)
  )
    return false;
  return (
    shape(value.display, ['widthMm', 'heightMm', 'configurationVerified']) &&
    [value.display.widthMm, value.display.heightMm].every(
      (v) => Number.isFinite(v) && v >= 0 && v <= 10000
    ) &&
    typeof value.display.configurationVerified === 'boolean'
  );
}

/** Inspect bounded JSON observations; no target or local environment is consulted.
 * @param {string} source @param {{platform?: string, verbose?: boolean}} [options]
 * @returns {TargetResult} */
export function inspectTargetRuntimeText(source, options = {}) {
  if ((options.platform ?? 'qnx') !== 'qnx')
    return {
      ok: false,
      step: 'platform',
      message: 'Target runtime observations are QNX-only.',
    };
  if (typeof source !== 'string' || Buffer.byteLength(source) > MAX_BYTES)
    return {
      ok: false,
      step: 'parse-report',
      message: 'Use a JSON observation of at most 64 KiB.',
    };
  let parsed;
  try {
    parsed = JSON.parse(source);
  } catch {
    return {
      ok: false,
      step: 'parse-report',
      message: 'The target observation is not valid JSON.',
    };
  }
  if (!validObservation(parsed))
    return {
      ok: false,
      step: 'validate-report',
      message:
        'The target observation does not match rnqnx-qnx-runtime-observation/v1. See docs/getting-started/qnx-target-runtime-doctor.md.',
    };
  const value = parsed;
  /** @type {TargetCheck[]} */
  const checks = [];
  /** @param {string} id @param {boolean} good @param {string} pass @param {string} fail */
  const check = (id, good, pass, fail) =>
    checks.push({
      id,
      status: good ? 'passed' : 'failed',
      message: good ? pass : fail,
    });
  check(
    'screen',
    value.services.screen,
    'Screen was reported running.',
    'Start and verify Screen using the image runbook before launching Qt.'
  );
  check(
    'slogger2',
    value.services.slogger2,
    'slogger2 was reported running.',
    'Verify slogger2 and /dev/slog2 on the target.'
  );
  check(
    'tmp',
    value.tmp,
    '/tmp was reported usable by the application user.',
    'Provide an accessible /tmp directory for the application and slog2.'
  );
  check(
    'random',
    value.random,
    '/dev/random was reported functional.',
    'Verify the target random service and /dev/random.'
  );
  check(
    'ipv6',
    value.ipv6,
    'The target network stack was reported IPv6-capable.',
    'Verify IPv6 support, including when the application uses IPv4.'
  );
  for (const [key, resolved] of [
    ['LD_LIBRARY_PATH', value.resolvedDirectories.libraries],
    ['QT_PLUGIN_PATH', value.resolvedDirectories.plugins],
    ['QML2_IMPORT_PATH', value.resolvedDirectories.qml],
  ]) {
    check(
      key,
      value.directories.includes(resolved),
      `${key}: the resolved runtime directory was reported present, whether selected by environment or Qt defaults.`,
      `${key}: report the actual resolved runtime directory and verify that it exists on the target.`
    );
  }
  const fontDirs = value.resolvedDirectories.fonts;
  const resolvedFonts =
    fontDirs.length > 0 &&
    fontDirs.every((dir) => value.directories.includes(dir)) &&
    value.files.some(
      (file) => /\.(ttf|otf|ttc)$/i.test(file) && fontDirs.includes(path.posix.dirname(file))
    );
  check(
    'fonts',
    value.fontconfig || resolvedFonts,
    'Fontconfig or a font file in a resolved font directory was reported.',
    'Without fontconfig, report a real font file and its resolved directory (QT_QPA_FONTDIR, Qt lib/fonts or registered application fonts).'
  );
  const config = value.environment.QT_QPA_QNX_DISPLAY_CONFIG;
  check(
    'display-configuration',
    value.display.configurationVerified &&
      (!config || (absolutePath(config) && value.files.includes(config))),
    'The display count/order was reported checked, including the configured file when set.',
    'Check display count/order and QT_QPA_QNX_DISPLAY_CONFIG; an explicit path must be a reported file.'
  );
  const measured = value.display.widthMm > 0 && value.display.heightMm > 0;
  const fallback = value.environment.QQNX_PHYSICAL_SCREEN_SIZE;
  const dimensions = /^\d+(?:\.\d+)?,\d+(?:\.\d+)?$/.test(fallback)
    ? fallback.split(',').map(Number)
    : [];
  const validFallback = dimensions.length === 2 && dimensions.every((v) => v > 0 && v <= 10000);
  check(
    'physical-screen-size',
    fallback ? validFallback : measured,
    fallback
      ? 'A positive millimetre fallback was reported; confirm it matches the actual display.'
      : 'Screen reported positive physical dimensions.',
    'For a zero/unknown Screen dimension, set QQNX_PHYSICAL_SCREEN_SIZE to the actual width,height in millimetres; any configured fallback must be valid.'
  );
  return {
    ok: true,
    report: {
      schema: 'rnqnx-qnx-runtime-diagnostics/v1',
      platform: 'qnx',
      evidenceTier: 'offline-observation',
      observationOrigin: 'user-supplied',
      collectionVerified: false,
      capturedAt: value.capturedAt,
      target: value.target,
      reportedPrerequisitesSatisfied: checks.every((item) => item.status === 'passed'),
      runtimeVerified: false,
      checks,
      ...(options.verbose
        ? {
            debugEnvironment: {
              QT_DEBUG_PLUGINS: '1',
              LD_DEBUG: '1',
              QT_LOGGING_TO_CONSOLE: '1',
            },
          }
        : {}),
    },
  };
}

/** Read only one bounded regular file; never follow a report symlink or run code.
 * @param {string} file @param {{platform?: string, verbose?: boolean}} [options]
 * @returns {Promise<TargetResult>} */
export async function inspectTargetRuntimeFile(file, options = {}) {
  if ((options.platform ?? 'qnx') !== 'qnx')
    return {
      ok: false,
      step: 'platform',
      message: 'Target runtime observations are QNX-only.',
    };
  try {
    const info = await lstat(file);
    if (!info.isFile() || info.size > MAX_BYTES)
      return {
        ok: false,
        step: 'read-report',
        message: 'Use a regular report file of at most 64 KiB; symlinks are refused.',
      };
    const handle = await open(
      file,
      constants.O_RDONLY | constants.O_NOFOLLOW | constants.O_NONBLOCK
    );
    try {
      const opened = await handle.stat();
      if (!opened.isFile() || opened.size > MAX_BYTES)
        return {
          ok: false,
          step: 'read-report',
          message: 'The opened report is not a bounded regular file.',
        };
      const bytes = Buffer.alloc(MAX_BYTES + 1);
      let length = 0;
      while (length < bytes.length) {
        const read = await handle.read(bytes, length, bytes.length - length, length);
        if (!read.bytesRead) break;
        length += read.bytesRead;
      }
      if (length > MAX_BYTES)
        return {
          ok: false,
          step: 'read-report',
          message: 'The report exceeds 64 KiB.',
        };
      return inspectTargetRuntimeText(bytes.subarray(0, length).toString('utf8'), options);
    } finally {
      await handle.close();
    }
  } catch {
    return {
      ok: false,
      step: 'read-report',
      message: 'Cannot read a bounded regular target observation file.',
    };
  }
}
