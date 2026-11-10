import { constants } from 'node:fs';
import { open, lstat, realpath, readlink, opendir } from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';

export const LIMITS = Object.freeze({
  manifestBytes: 2 * 1024 * 1024,
  textBytes: 2 * 1024 * 1024,
  elfBytes: 128 * 1024 * 1024,
  fileBytes: 2 * 1024 ** 3,
  totalBytes: 8 * 1024 ** 3,
  entries: 8192,
  depth: 24,
});
const HASH = /^[a-f0-9]{64}$/;
const ID = /^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$/;
const URI = /^[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)*$/;
const ROLES = [
  'source',
  'modifications',
  'build-recipe',
  'configuration',
  'toolchain',
  'license',
  'notice',
  'replacement',
  'installation',
  'app-build',
  'component-build',
  'inline-source',
  'system',
];

function requireThat(/** @type {any} */ condition, /** @type {any} */ message) {
  if (!condition) throw new Error(message);
}
function object(/** @type {any} */ value, /** @type {any} */ keys, /** @type {any} */ label) {
  requireThat(
    value && !Array.isArray(value) && typeof value === 'object',
    `${label}: expected object`
  );
  requireThat(
    Object.keys(value).every((key) => keys.includes(key)),
    `${label}: unknown field`
  );
  requireThat(
    keys.every((/** @type {any} */ key) => Object.hasOwn(value, key)),
    `${label}: missing field`
  );
}
function list(/** @type {any} */ value, /** @type {any} */ label, nonempty = true) {
  requireThat(
    Array.isArray(value) && value.length <= LIMITS.entries && (!nonempty || value.length > 0),
    `${label}: expected bounded ${nonempty ? 'nonempty ' : ''}array`
  );
  return value;
}
function string(
  /** @type {any} */ value,
  /** @type {any} */ label,
  /** @type {RegExp | undefined} */ pattern = undefined
) {
  requireThat(
    typeof value === 'string' &&
      value.length > 0 &&
      value.length <= 4096 &&
      !/[\x00-\x1f\x7f]/.test(value) &&
      (!pattern || pattern.test(value)),
    `${label}: invalid string`
  );
  return value;
}
function hash(/** @type {any} */ value, /** @type {any} */ label) {
  return string(value, label, HASH);
}
function unique(/** @type {any} */ items, /** @type {any} */ key, /** @type {any} */ label) {
  const map = new Map();
  for (const item of items) {
    const name = key(item);
    requireThat(!map.has(name), `${label}: duplicate ${name}`);
    map.set(name, item);
  }
  return map;
}

export function relativePath(/** @type {any} */ value) {
  string(value, 'relative path');
  requireThat(
    value.length <= 512 &&
      /^[A-Za-z0-9_./+-]+$/.test(value) &&
      value
        .split('/')
        .every((/** @type {any} */ part) => part !== '' && part !== '.' && part !== '..') &&
      !value.startsWith('/') &&
      value.split('/').length <= LIMITS.depth,
    `unsafe relative path: ${value}`
  );
  return value;
}

function boundedTree(/** @type {any} */ value) {
  /** @type {[any, number][]} */
  const pending = [[value, 0]];
  let count = 0;
  while (pending.length) {
    const [item, depth] = /** @type {[any, number]} */ (pending.pop());
    requireThat(++count <= 100000 && depth <= LIMITS.depth, 'manifest structure limit exceeded');
    if (typeof item === 'string')
      requireThat(item.length <= 65536, 'manifest string limit exceeded');
    if (item && typeof item === 'object') {
      for (const [key, child] of Object.entries(item)) {
        requireThat(
          !['__proto__', 'prototype', 'constructor'].includes(key),
          'unsafe manifest key'
        );
        pending.push([child, depth + 1]);
      }
    }
  }
}

export function canonicalJson(/** @type {any} */ value) {
  /** @type {(item: any) => any} */
  const sort = (/** @type {any} */ item) => {
    if (Array.isArray(item)) return item.map(sort);
    if (item && typeof item === 'object')
      return Object.fromEntries(
        Object.keys(item)
          .sort()
          .map((key) => [key, sort(item[key])])
      );
    return item;
  };
  return `${JSON.stringify(sort(value), null, 2)}\n`;
}

// Roots are canonicalized once; no symlinked ancestors or special files are admitted.
export async function confinedFile(/** @type {any} */ root, /** @type {any} */ relative) {
  relativePath(relative);
  let current = root;
  const parts = relative.split('/');
  for (let i = 0; i < parts.length; i++) {
    current = path.join(current, parts[i]);
    const stat = await lstat(current);
    requireThat(
      !stat.isSymbolicLink() && (i === parts.length - 1 ? stat.isFile() : stat.isDirectory()),
      `non-regular or symlinked input: ${relative}`
    );
  }
  return current;
}

export async function readBounded(/** @type {any} */ file, /** @type {any} */ maximum) {
  const handle = await open(file, constants.O_RDONLY | constants.O_NOFOLLOW);
  try {
    const stat = await handle.stat();
    requireThat(stat.isFile() && stat.size <= maximum, `file size/type limit: ${file}`);
    const data = Buffer.alloc(stat.size);
    let position = 0;
    while (position < data.length) {
      const { bytesRead } = await handle.read(data, position, data.length - position, position);
      requireThat(bytesRead > 0, `input changed while reading: ${file}`);
      position += bytesRead;
    }
    requireThat((await handle.stat()).size === stat.size, `input changed while reading: ${file}`);
    return data;
  } finally {
    await handle.close();
  }
}

export async function fileDigest(/** @type {any} */ file) {
  const handle = await open(file, constants.O_RDONLY | constants.O_NOFOLLOW);
  try {
    const stat = await handle.stat();
    requireThat(stat.isFile() && stat.size <= LIMITS.fileBytes, `file size/type limit: ${file}`);
    const digest = createHash('sha256');
    const buffer = Buffer.alloc(1024 * 1024);
    let size = 0;
    while (true) {
      const { bytesRead } = await handle.read(buffer, 0, buffer.length, null);
      if (!bytesRead) break;
      size += bytesRead;
      requireThat(
        size <= LIMITS.fileBytes && size <= stat.size,
        `input grew while hashing: ${file}`
      );
      digest.update(buffer.subarray(0, bytesRead));
    }
    requireThat(size === stat.size, `input changed while hashing: ${file}`);
    return { sha256: digest.digest('hex'), size };
  } finally {
    await handle.close();
  }
}

// Read program headers, not an executable's output or a caller-supplied dependency list.
export function inspectElf(/** @type {any} */ data, /** @type {any} */ abi) {
  requireThat(
    data.length >= 64 && data.subarray(0, 4).equals(Buffer.from([127, 69, 76, 70])),
    'invalid ELF magic/header'
  );
  requireThat(
    data[4] === 2 && data[5] === 1 && data[6] === 1,
    'unsupported ELF class/endian/version'
  );
  const integer = (/** @type {any} */ offset) => {
    const value = data.readBigUInt64LE(offset);
    requireThat(value <= BigInt(Number.MAX_SAFE_INTEGER), 'ELF integer overflow');
    return Number(value);
  };
  const range = (/** @type {any} */ offset, /** @type {any} */ size) => {
    requireThat(
      Number.isSafeInteger(offset + size) &&
        offset >= 0 &&
        size >= 0 &&
        offset + size <= data.length,
      'ELF range out of bounds'
    );
  };
  requireThat(
    data.readUInt16LE(18) === abi.machine &&
      data[7] === abi.osabi &&
      data[8] === abi.abiVersion &&
      data.readUInt32LE(48) === abi.flags,
    'ELF ABI mismatch'
  );
  const type = data.readUInt16LE(16);
  requireThat(
    [2, 3].includes(type) && data.readUInt32LE(20) === 1 && data.readUInt16LE(52) === 64,
    'unsupported ELF type/header'
  );
  const offset = integer(32),
    entrySize = data.readUInt16LE(54),
    count = data.readUInt16LE(56);
  requireThat(entrySize === 56 && count > 0 && count <= 1024, 'ELF program-header limit');
  range(offset, entrySize * count);
  const segments = [];
  for (let i = 0; i < count; i++) {
    const at = offset + i * entrySize;
    const segment = {
      type: data.readUInt32LE(at),
      offset: integer(at + 8),
      address: integer(at + 16),
      size: integer(at + 32),
    };
    range(segment.offset, segment.size);
    requireThat(
      Number.isSafeInteger(segment.address + segment.size),
      'ELF virtual-address overflow'
    );
    segments.push(segment);
  }
  const dynamics = segments.filter((s) => s.type === 2);
  requireThat(
    dynamics.length === 1 && dynamics[0].size <= LIMITS.textBytes && dynamics[0].size % 16 === 0,
    'ELF needs one bounded dynamic segment (shared-library replacement only)'
  );
  /** @type {any[]} */
  const entries = [];
  let terminated = false;
  for (let at = dynamics[0].offset; at < dynamics[0].offset + dynamics[0].size; at += 16) {
    const tag = integer(at),
      value = integer(at + 8);
    if (tag === 0) {
      terminated = true;
      break;
    }
    entries.push({ tag, value });
  }
  requireThat(terminated, 'unterminated ELF dynamic table');
  const one = (/** @type {any} */ tag) => {
    const matches = entries.filter((e) => e.tag === tag);
    requireThat(matches.length <= 1, `ambiguous ELF dynamic tag ${tag}`);
    return matches[0]?.value;
  };
  const address = one(5),
    size = one(10);
  requireThat(
    address !== undefined && size > 0 && size <= LIMITS.textBytes,
    'missing/bounded ELF string table'
  );
  const loads = segments.filter(
    (s) => s.type === 1 && address >= s.address && address + size <= s.address + s.size
  );
  requireThat(loads.length === 1, 'ambiguous/unmapped ELF string table');
  const start = loads[0].offset + address - loads[0].address;
  range(start, size);
  const cString = (/** @type {any} */ at, /** @type {any} */ end) => {
    const nul = data.indexOf(0, at);
    requireThat(nul >= at && nul < end && nul - at <= 4096, 'unterminated/oversized ELF string');
    return data.toString('utf8', at, nul);
  };
  const dynString = (/** @type {any} */ index) => {
    requireThat(index < size, 'ELF string offset out of bounds');
    return cString(start + index, start + size);
  };
  const needed = entries.filter((e) => e.tag === 1).map((e) => dynString(e.value));
  const soname = one(14) === undefined ? null : dynString(one(14));
  for (const name of [...needed, ...(soname ? [soname] : [])])
    string(name, 'ELF library name', /^[A-Za-z0-9_+.-]+\.so(?:\.[A-Za-z0-9_.-]+)?$/);
  requireThat(new Set(needed).size === needed.length, 'duplicate ELF dependency');
  // Filters/auditors introduce implicit loading beyond DT_NEEDED; do not guess their closure.
  requireThat(
    !entries.some((e) => [0x7fffffff, 0x7ffffffd, 0x6ffffefb, 0x6ffffefc].includes(e.tag)),
    'unsupported ELF filter/auditor'
  );
  const interpreters = segments.filter((s) => s.type === 3);
  requireThat(interpreters.length <= 1, 'ambiguous ELF interpreter');
  const interpreter = interpreters.length
    ? cString(interpreters[0].offset, interpreters[0].offset + interpreters[0].size)
    : null;
  const searchPaths = [15, 29].flatMap((tag) =>
    one(tag) === undefined ? [] : dynString(one(tag)).split(':')
  );
  return { type, needed, soname, interpreter, searchPaths };
}

function validateManifest(/** @type {any} */ m) {
  boundedTree(m);
  object(
    m,
    [
      'schemaVersion',
      'profile',
      'repository',
      'app',
      'abi',
      'qt',
      'materials',
      'components',
      'files',
      'qmlModules',
      'qmlRoots',
      'inlineSources',
      'dynamicPlugins',
      'systemDependencies',
      'replacement',
    ],
    'manifest'
  );
  requireThat(
    m.schemaVersion === 1 && ['baseline', 'desktop'].includes(m.profile),
    'invalid schema/profile'
  );
  object(m.repository, ['commit', 'sourceMaterial'], 'repository');
  string(m.repository.commit, 'repository commit', /^[a-f0-9]{40}$/);
  object(m.app, ['sha256', 'bundleSha256', 'hermesSha256', 'buildMaterial'], 'app');
  for (const key of ['sha256', 'bundleSha256', 'hermesSha256']) hash(m.app[key], `app.${key}`);
  object(m.abi, ['os', 'class', 'endian', 'machine', 'osabi', 'abiVersion', 'flags'], 'abi');
  requireThat(
    m.abi.os === 'qnx' &&
      m.abi.class === 64 &&
      m.abi.endian === 'little' &&
      m.abi.machine === 183 &&
      [m.abi.osabi, m.abi.abiVersion].every((n) => Number.isInteger(n) && n >= 0 && n <= 255) &&
      Number.isInteger(m.abi.flags) &&
      m.abi.flags >= 0 &&
      m.abi.flags <= 0xffffffff,
    'unsupported target ABI'
  );
  object(
    m.qt,
    [
      'channel',
      'version',
      'sourceUrl',
      'sourceSha256',
      'sourceMaterial',
      'modificationsMaterial',
      'recipeMaterial',
      'configurationMaterial',
      'toolchainMaterial',
      'hostVersion',
      'targetVersion',
      'compiler',
      'sysrootSha256',
      'qpaPlugin',
    ],
    'qt'
  );
  requireThat(
    m.qt.channel === 'public-source',
    'Qt requires public-source provenance; evaluation receipts rejected'
  );
  string(m.qt.version, 'Qt version', /^6\.\d+\.\d+$/);
  requireThat(
    m.qt.hostVersion === m.qt.version && m.qt.targetVersion === m.qt.version,
    'host/target Qt version mismatch'
  );
  string(m.qt.compiler, 'compiler identity');
  hash(m.qt.sysrootSha256, 'sysroot identity');
  hash(m.qt.sourceSha256, 'Qt source archive hash');
  const url = new URL(string(m.qt.sourceUrl, 'Qt public source URL'));
  requireThat(
    url.protocol === 'https:' &&
      !url.username &&
      !url.password &&
      !url.search &&
      !url.hash &&
      ((url.hostname === 'download.qt.io' && url.pathname.startsWith('/official_releases/qt/')) ||
        (url.hostname === 'code.qt.io' && url.pathname.startsWith('/cgit/qt/')) ||
        (url.hostname === 'github.com' && url.pathname.startsWith('/qt/'))),
    'unapproved Qt public source origin'
  );
  requireThat(
    !/eval|commercial|enterprise/i.test(url.pathname),
    'evaluation/commercial source receipt rejected'
  );
  object(
    m.replacement,
    ['mechanism', 'instructionsMaterial', 'installationMaterial'],
    'replacement'
  );
  requireThat(
    m.replacement.mechanism === 'shared-library',
    'only shared-library replacement candidates are supported'
  );
  const materials = unique(
    list(m.materials, 'materials'),
    (/** @type {any} */ r) => string(r.id, 'material id', ID),
    'materials'
  );
  for (const r of materials.values()) {
    object(r, ['id', 'path', 'sha256', 'role'], 'material');
    relativePath(r.path);
    hash(r.sha256, 'material hash');
    requireThat(ROLES.includes(r.role), `unsupported material role: ${r.role}`);
  }
  unique(m.materials, (/** @type {any} */ r) => r.path, 'material paths');
  const reference = (/** @type {any} */ id, /** @type {any} */ role) => {
    requireThat(materials.get(id)?.role === role, `missing ${role} material: ${id}`);
    return materials.get(id);
  };
  reference(m.repository.sourceMaterial, 'source');
  reference(m.app.buildMaterial, 'app-build');
  requireThat(
    reference(m.qt.sourceMaterial, 'source').sha256 === m.qt.sourceSha256,
    'Qt source identity mismatch'
  );
  for (const [key, role] of [
    ['modificationsMaterial', 'modifications'],
    ['recipeMaterial', 'build-recipe'],
    ['configurationMaterial', 'configuration'],
    ['toolchainMaterial', 'toolchain'],
  ])
    reference(m.qt[key], role);
  reference(m.replacement.instructionsMaterial, 'replacement');
  reference(m.replacement.installationMaterial, 'installation');
  for (const id of list(m.inlineSources, 'inline sources')) reference(id, 'inline-source');
  const components = unique(
    list(m.components, 'components'),
    (/** @type {any} */ r) => string(r.id, 'component id', ID),
    'components'
  );
  for (const c of components.values()) {
    object(
      c,
      [
        'id',
        'version',
        'channel',
        'sourceMaterial',
        'buildMaterial',
        'licenseMaterials',
        'noticeMaterial',
        'terms',
      ],
      'component'
    );
    string(c.version, 'component version');
    requireThat(
      [
        'LGPL-3.0-only',
        'LGPL-3.0-or-later',
        'MIT',
        'BSD-2-Clause',
        'BSD-3-Clause',
        'Apache-2.0',
        'Zlib',
        'FTL',
        'OFL-1.1',
        'Unicode-3.0',
      ].includes(c.terms),
      `unsupported selected component terms: ${c.id}`
    );
    requireThat(
      c.channel === 'public-source',
      `component requires public-source provenance: ${c.id}`
    );
    reference(c.sourceMaterial, 'source');
    reference(c.noticeMaterial, 'notice');
    reference(c.buildMaterial, 'component-build');
    for (const id of list(c.licenseMaterials, 'license materials')) reference(id, 'license');
  }
  const files = unique(
    list(m.files, 'runtime files'),
    (/** @type {any} */ f) => relativePath(f.path),
    'runtime files'
  );
  unique(m.files, (/** @type {any} */ f) => f.path.toLowerCase(), 'case-folded runtime paths');
  for (const f of files.values()) {
    requireThat(
      ['elf', 'qml', 'qmldir', 'asset', 'symlink'].includes(f.kind),
      'unsupported runtime file kind'
    );
    object(
      f,
      f.kind === 'symlink'
        ? ['path', 'kind', 'sha256', 'component', 'target']
        : ['path', 'kind', 'sha256', 'component'],
      'runtime file'
    );
    hash(f.sha256, 'runtime hash');
    requireThat(components.has(f.component), `missing component: ${f.component}`);
    requireThat(
      /^(lib|plugins|qml|fonts)\//.test(f.path) && !/\.(a|h|hpp|prl|cmake)$/i.test(f.path),
      'non-runtime payload rejected'
    );
    if (f.kind === 'elf')
      requireThat(
        /\.so(?:\.[\w.-]+)?$/.test(f.path),
        'runtime ELF must be a shared library/plugin'
      );
    if (f.path.startsWith('lib/'))
      requireThat(
        f.path.split('/').length === 2 && ['elf', 'symlink'].includes(f.kind),
        'lib contains only flat shared libraries/aliases'
      );
    if (f.kind === 'symlink') {
      string(f.target, 'library symlink target', /^[A-Za-z0-9_.+-]+$/);
      requireThat(
        f.path.startsWith('lib/') && f.target !== '.' && f.target !== '..',
        'only confined library symlinks allowed'
      );
    }
  }
  for (const f of files.values()) {
    for (
      let parent = path.posix.dirname(f.path);
      parent !== '.';
      parent = path.posix.dirname(parent)
    ) {
      requireThat(!files.has(parent), `file/directory path collision: ${parent}`);
    }
  }
  const modules = unique(
    list(m.qmlModules, 'QML modules'),
    (/** @type {any} */ r) => string(r.uri, 'QML URI', URI),
    'QML modules'
  );
  unique(m.qmlModules, (/** @type {any} */ r) => r.path, 'QML module paths');
  for (const r of modules.values()) {
    object(r, ['uri', 'path', 'dynamicImports'], 'QML module');
    relativePath(r.path);
    requireThat(
      r.path === `qml/${r.uri.replaceAll('.', '/')}` &&
        files.get(`${r.path}/qmldir`)?.kind === 'qmldir',
      `missing QML qmldir: ${r.uri}`
    );
    for (const uri of list(r.dynamicImports, 'dynamic imports', false)) string(uri, 'QML URI', URI);
  }
  for (const uri of list(m.qmlRoots, 'QML roots')) string(uri, 'QML root', URI);
  const required = [
    'QtQuick',
    'QtQuick.Shapes',
    ...(m.profile === 'desktop'
      ? ['QtQuick.Controls', 'QtQuick.Controls.Basic', 'QtQuick.Templates']
      : []),
  ];
  requireThat(
    required.every((uri) => m.qmlRoots.includes(uri)),
    'missing profile QML roots (including inline Shapes)'
  );
  requireThat(
    ![...modules.keys()].some(
      (uri) => uri === 'QtQuick.Layouts' || uri.startsWith('QtQuick.Layouts.')
    ),
    'QtQuick.Layouts is excluded; Yoga owns layout'
  );
  if (m.profile === 'baseline')
    requireThat(
      ![...modules.keys()].some((uri) => /^QtQuick\.(Controls|Templates)(\.|$)/.test(uri)),
      'desktop-only QML module in baseline profile'
    );
  for (const p of list(m.dynamicPlugins, 'dynamic plugins'))
    requireThat(
      files.get(p)?.kind === 'elf' && p.startsWith('plugins/'),
      `missing dynamic plugin: ${p}`
    );
  requireThat(
    m.qt.qpaPlugin.startsWith('plugins/platforms/') && m.dynamicPlugins.includes(m.qt.qpaPlugin),
    'missing QPA plugin'
  );
  const system = unique(
    list(m.systemDependencies, 'system dependencies'),
    (/** @type {any} */ r) => r.name,
    'system dependencies'
  );
  for (const r of system.values()) {
    object(r, ['name', 'sha256', 'material'], 'system dependency');
    hash(r.sha256, 'system dependency hash');
    string(
      r.name,
      'system dependency',
      /^(?:\/[A-Za-z0-9_./+-]+|[A-Za-z0-9_+.-]+\.so(?:\.[A-Za-z0-9_.-]+)?)$/
    );
    requireThat(
      !r.name.includes('..') && !/Qt6|hermes/i.test(r.name),
      'Qt/Hermes cannot be target-system dependencies'
    );
    reference(r.material, 'system');
  }
  return { files, modules, materials, reference, system };
}

function importedUris(/** @type {any} */ source) {
  const uncommented = source.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, '');
  return [
    ...uncommented.matchAll(
      /\b(?:import|depends)\s+([A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)*)/g
    ),
  ]
    .map((match) => match[1])
    .filter((uri) => uri !== 'auto');
}

async function validateQml(
  /** @type {any} */ m,
  /** @type {any} */ model,
  /** @type {any} */ prefix,
  /** @type {any} */ materialRoot
) {
  const requireModule = (/** @type {any} */ uri) =>
    requireThat(model.modules.has(uri), `missing QML closure module: ${uri}`);
  /** @type {any[]} */
  const pending = [];
  let edges = 0;
  const enqueue = (/** @type {any} */ uris) => {
    edges += uris.length;
    requireThat(edges <= 65536, 'QML import edge limit exceeded');
    pending.push(...uris);
  };
  enqueue(m.qmlRoots);
  for (const id of m.inlineSources) {
    const file = model.materials.get(id);
    enqueue(
      importedUris(
        (await readBounded(await confinedFile(materialRoot, file.path), LIMITS.textBytes)).toString(
          'utf8'
        )
      )
    );
  }
  const seen = new Set();
  while (pending.length) {
    const uri = pending.pop();
    requireModule(uri);
    if (seen.has(uri)) continue;
    seen.add(uri);
    const module = model.modules.get(uri);
    enqueue(module.dynamicImports);
    const root = await confinedDirectory(prefix, module.path);
    let entries = 0;
    const visit = async (
      /** @type {any} */ directory,
      /** @type {any} */ relative,
      /** @type {any} */ depth
    ) => {
      requireThat(depth <= LIMITS.depth, 'QML directory depth exceeded');
      const iterator = await opendir(directory);
      for await (const child of iterator) {
        requireThat(++entries <= LIMITS.entries, 'QML directory entry limit exceeded');
        const name = `${relative}/${child.name}`;
        if (child.isDirectory()) {
          // Installed sibling modules are not part of this selected module's inventory.
          let nestedModule = false;
          const nestedQmldir = path.join(directory, child.name, 'qmldir');
          try {
            const metadata = (await readBounded(nestedQmldir, LIMITS.textBytes)).toString('utf8');
            const declaration = /^module\s+(\S+)\s*$/m.exec(metadata);
            nestedModule =
              !!declaration &&
              URI.test(declaration[1]) &&
              name === `qml/${declaration[1].replaceAll('.', '/')}`;
          } catch (error) {
            if (/** @type {any} */ (error).code !== 'ENOENT') throw error;
          }
          if (!nestedModule) await visit(path.join(directory, child.name), name, depth + 1);
        } else {
          requireThat(
            child.isFile() && model.files.has(name),
            `unlisted/non-regular QML module file: ${name}`
          );
        }
      }
    };
    await visit(root, module.path, 0);
    const qmldir = (
      await readBounded(await confinedFile(prefix, `${module.path}/qmldir`), LIMITS.textBytes)
    ).toString('utf8');
    requireThat(
      new RegExp(`^module\\s+${uri.replaceAll('.', '\\.')}\\s*$`, 'm').test(qmldir),
      `qmldir URI mismatch: ${uri}`
    );
    enqueue(importedUris(qmldir));
    for (const line of qmldir.split(/\r?\n/).map((r) => r.replace(/#.*/, '').trim())) {
      const plugin = /^(?:optional\s+)?plugin\s+(\S+)(?:\s+(\S+))?$/.exec(line);
      if (plugin) {
        const directory = plugin[2] ? `${module.path}/${relativePath(plugin[2])}` : module.path;
        const pluginPath = `${directory}/lib${string(plugin[1], 'QML plugin name', ID)}.so`;
        requireThat(
          model.files.get(pluginPath)?.kind === 'elf',
          `missing QML implementation plugin: ${pluginPath}`
        );
      }
      const type = /(?:^|\s)([^\s]+\.(?:qml|js))$/.exec(line);
      if (type)
        requireThat(
          model.files.has(`${module.path}/${relativePath(type[1])}`),
          `missing qmldir type: ${type[1]}`
        );
    }
    for (const f of model.files.values()) {
      if (!f.path.startsWith(`${module.path}/`) || !/\.(qml|js|mjs)$/.test(f.path)) continue;
      const owner = [...model.modules.values()]
        .filter((r) => f.path.startsWith(`${r.path}/`))
        .sort((a, b) => b.path.length - a.path.length)[0];
      if (owner.uri !== uri) continue;
      const source = (
        await readBounded(await confinedFile(prefix, f.path), LIMITS.textBytes)
      ).toString('utf8');
      enqueue(importedUris(source));
      for (const match of source.matchAll(/(?:\bimport|\.import)\s+["']([^"']+)["']/g)) {
        const local = match[1];
        requireThat(!/[:\\]/.test(local) && !local.startsWith('/'), 'external QML import rejected');
        const resolved = path.posix.normalize(path.posix.join(path.posix.dirname(f.path), local));
        relativePath(resolved);
        requireThat(
          resolved.startsWith('qml/') &&
            (model.files.has(resolved) ||
              [...model.files.keys()].some((p) => p.startsWith(`${resolved}/`))),
          `missing local QML import: ${local}`
        );
      }
    }
  }
  requireThat(
    seen.size === model.modules.size,
    'unreachable QML module: declare actual dynamic imports'
  );
  return [...seen].sort();
}

async function confinedDirectory(/** @type {any} */ root, /** @type {any} */ relative) {
  const marker = await confinedFile(root, `${relative}/qmldir`);
  return path.dirname(marker);
}

export async function validateCandidate(/** @type {any} */ { manifest, qtPrefix, app, bundle }) {
  const manifestFile = await realpath(manifest);
  const materialRoot = path.dirname(manifestFile),
    prefix = await realpath(qtPrefix);
  const bytes = await readBounded(manifestFile, LIMITS.manifestBytes);
  const m = JSON.parse(bytes.toString('utf8'));
  const model = validateManifest(m);
  const inputs = [];
  let totalBytes = 0;
  const verify = async (
    /** @type {any} */ source,
    /** @type {any} */ expected,
    /** @type {any} */ destination,
    kind = 'file'
  ) => {
    const digest = await fileDigest(source);
    requireThat(digest.sha256 === expected, `hash mismatch: ${destination}`);
    totalBytes += digest.size;
    requireThat(totalBytes <= LIMITS.totalBytes, 'candidate total-byte limit exceeded');
    const record = { source, path: destination, sha256: digest.sha256, size: digest.size, kind };
    inputs.push(record);
    return record;
  };
  await verify(app, m.app.sha256, 'bin/app', 'app');
  await verify(bundle, m.app.bundleSha256, 'app.hbc', 'bundle');
  for (const r of model.materials.values())
    await verify(await confinedFile(materialRoot, r.path), r.sha256, `materials/${r.path}`);
  const elves = new Map();
  const appElf = inspectElf(await readBounded(app, LIMITS.elfBytes), m.abi);
  requireThat(
    appElf.needed.some((n) => /^libQt6Core\.so/.test(n)) &&
      appElf.needed.some((n) => /^libQt6Quick\.so/.test(n)),
    'app must dynamically link Qt Core and Quick'
  );
  elves.set('bin/app', appElf);
  for (const f of model.files.values()) {
    if (f.kind === 'symlink') {
      const parent = path.posix.dirname(f.path);
      await confinedFile(prefix, `${parent}/${f.target}`);
      requireThat(
        (await lstat(path.join(prefix, f.path))).isSymbolicLink() &&
          (await readlink(path.join(prefix, f.path))) === f.target,
        `library symlink mismatch: ${f.path}`
      );
      requireThat(
        model.files.get(`${parent}/${f.target}`)?.kind === 'elf',
        `unlisted symlink target: ${f.path}`
      );
      requireThat(
        createHash('sha256').update(f.target).digest('hex') === f.sha256,
        'symlink hash mismatch'
      );
      inputs.push({
        path: f.path,
        target: f.target,
        kind: 'symlink',
        sha256: f.sha256,
        size: Buffer.byteLength(f.target),
      });
      continue;
    }
    const source = await confinedFile(prefix, f.path);
    await verify(source, f.sha256, f.path);
    const head = await open(source, constants.O_RDONLY | constants.O_NOFOLLOW);
    let magic;
    try {
      magic = Buffer.alloc(4);
      await head.read(magic, 0, 4, 0);
    } finally {
      await head.close();
    }
    const isElf = magic.equals(Buffer.from([127, 69, 76, 70]));
    requireThat(isElf === (f.kind === 'elf'), `ELF file kind mismatch: ${f.path}`);
    if (isElf) {
      const elf = inspectElf(await readBounded(source, LIMITS.elfBytes), m.abi);
      requireThat(
        elf.type === 3 && !elf.interpreter,
        `runtime file is not a shared library: ${f.path}`
      );
      elves.set(f.path, elf);
    }
  }
  const providers = new Map();
  for (const file of elves.keys()) if (file !== 'bin/app') providers.set(file, file);
  for (const f of model.files.values())
    if (f.kind === 'symlink') providers.set(f.path, `${path.posix.dirname(f.path)}/${f.target}`);
  const sonames = new Map();
  for (const [file, elf] of elves)
    if (file.startsWith('lib/') && elf.soname) {
      requireThat(!sonames.has(elf.soname), `ambiguous ELF SONAME: ${elf.soname}`);
      sonames.set(elf.soname, file);
      requireThat(
        providers.get(`lib/${elf.soname}`) === file,
        `missing physical SONAME alias: ${elf.soname}`
      );
    }
  const dependencies = [];
  for (const [file, elf] of elves) {
    const search = ['lib'];
    for (const entry of elf.searchPaths) {
      requireThat(
        /^\$(?:ORIGIN|\{ORIGIN\})(?:\/[-A-Za-z0-9_./+]+)?$/.test(entry),
        `unsafe ELF runtime path: ${file}: ${entry}`
      );
      const resolved = path.posix.normalize(
        path.posix.join(path.posix.dirname(file), entry.replace(/^\$(?:ORIGIN|\{ORIGIN\})\/?/, ''))
      );
      requireThat(
        resolved === '.' ||
          (!resolved.startsWith('../') && resolved !== '..' && !resolved.startsWith('/')),
        `escaping ELF runtime path: ${file}`
      );
      search.push(resolved);
    }
    if (elf.interpreter)
      requireThat(
        model.system.has(elf.interpreter),
        `undeclared target interpreter: ${elf.interpreter}`
      );
    for (const name of elf.needed) {
      const matches = new Set(
        search.map((directory) => providers.get(path.posix.join(directory, name))).filter(Boolean)
      );
      requireThat(
        matches.size <= 1 && !(matches.size && model.system.has(name)),
        `ambiguous ELF provider: ${name}`
      );
      requireThat(
        matches.size || model.system.has(name),
        `missing ELF dependency: ${file} -> ${name}`
      );
      dependencies.push({
        file,
        needed: name,
        provider: matches.size ? [...matches][0] : 'target-system',
      });
    }
  }
  const qmlClosure = await validateQml(m, model, prefix, materialRoot);
  for (const component of m.components) {
    const material = model.reference(component.buildMaterial, 'component-build');
    const build = JSON.parse(
      (
        await readBounded(await confinedFile(materialRoot, material.path), LIMITS.textBytes)
      ).toString('utf8')
    );
    boundedTree(build);
    object(
      build,
      [
        'channel',
        'sourceSha256',
        'version',
        'qtVersion',
        'compiler',
        'sysrootSha256',
        'abi',
        'files',
      ],
      'component-build'
    );
    const expected = m.files
      .filter((/** @type {any} */ f) => f.component === component.id)
      .map((/** @type {any} */ f) => ({ path: f.path, sha256: f.sha256 }))
      .sort((/** @type {any} */ a, /** @type {any} */ b) =>
        a.path < b.path ? -1 : a.path > b.path ? 1 : 0
      );
    const actual = list(build.files, 'component build files')
      .map((/** @type {any} */ f) => {
        object(f, ['path', 'sha256'], 'component build file');
        relativePath(f.path);
        hash(f.sha256, 'component build file');
        return f;
      })
      .sort((/** @type {any} */ a, /** @type {any} */ b) =>
        a.path < b.path ? -1 : a.path > b.path ? 1 : 0
      );
    requireThat(
      build.channel === 'public-source' &&
        build.sourceSha256 === model.materials.get(component.sourceMaterial).sha256 &&
        build.version === component.version &&
        build.qtVersion === m.qt.version &&
        build.compiler === m.qt.compiler &&
        build.sysrootSha256 === m.qt.sysrootSha256 &&
        canonicalJson(build.abi) === canonicalJson(m.abi) &&
        canonicalJson(actual) === canonicalJson(expected),
      `component-build provenance/ABI/file identity mismatch: ${component.id}`
    );
  }
  const build = JSON.parse(
    (
      await readBounded(
        await confinedFile(materialRoot, model.reference(m.app.buildMaterial, 'app-build').path),
        LIMITS.textBytes
      )
    ).toString('utf8')
  );
  boundedTree(build);
  requireThat(
    build.qtChannel === 'public-source' &&
      build.repositoryCommit === m.repository.commit &&
      build.appSha256 === m.app.sha256 &&
      build.bundleSha256 === m.app.bundleSha256 &&
      build.hermesSha256 === m.app.hermesSha256 &&
      build.qtSourceSha256 === m.qt.sourceSha256 &&
      build.qtVersion === m.qt.version &&
      build.compiler === m.qt.compiler &&
      build.sysrootSha256 === m.qt.sysrootSha256 &&
      canonicalJson(build.abi) === canonicalJson(m.abi),
    'app-build ABI/source/bundle identity mismatch'
  );
  return {
    manifest: m,
    inputs: inputs.sort((a, b) => a.path.localeCompare(b.path, 'en')),
    manifestSha256: createHash('sha256').update(bytes).digest('hex'),
    dependencies,
    qmlClosure,
    prefix,
    materialRoot,
  };
}
