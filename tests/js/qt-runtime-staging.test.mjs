import test from 'node:test';
import assert from 'node:assert/strict';
import {
  mkdtemp,
  mkdir,
  writeFile,
  readFile,
  rm,
  symlink,
  lstat,
  readlink,
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { createHash } from 'node:crypto';
import {
  validateCandidate,
  inspectElf,
  relativePath,
  canonicalJson,
  LIMITS,
} from '../../scripts/lib/qt-runtime-manifest.mjs';
import { stageQtRuntime, main } from '../../scripts/stage-qt-runtime.mjs';

const sha = (/** @type {any} */ bytes) => createHash('sha256').update(bytes).digest('hex');
const ABI = {
  os: 'qnx',
  class: 64,
  endian: 'little',
  machine: 183,
  osabi: 0,
  abiVersion: 0,
  flags: 0,
};
const ROOTS = ['QtQuick', 'QtQuick.Shapes'];

// Structural ELF fixtures, not compiled QNX binaries or evidence that Qt runs.
/** @param {{needed?: string[], soname?: string, rpath?: string, interpreter?: string, machine?: number}} [options] */
function elf({ needed = ['libc.so.3'], soname, rpath, interpreter, machine = 183 } = {}) {
  const bytes = Buffer.alloc(4096);
  Buffer.from([127, 69, 76, 70, 2, 1, 1, 0]).copy(bytes);
  bytes.writeUInt16LE(3, 16);
  bytes.writeUInt16LE(machine, 18);
  bytes.writeUInt32LE(1, 20);
  bytes.writeBigUInt64LE(64n, 32);
  bytes.writeUInt16LE(64, 52);
  bytes.writeUInt16LE(56, 54);
  bytes.writeUInt16LE(interpreter ? 3 : 2, 56);
  const segment = (
    /** @type {any} */ index,
    /** @type {any} */ type,
    /** @type {any} */ offset,
    /** @type {any} */ size
  ) => {
    const at = 64 + index * 56;
    bytes.writeUInt32LE(type, at);
    bytes.writeBigUInt64LE(BigInt(offset), at + 8);
    bytes.writeBigUInt64LE(BigInt(0x1000 + offset), at + 16);
    bytes.writeBigUInt64LE(BigInt(size), at + 32);
    bytes.writeBigUInt64LE(BigInt(size), at + 40);
  };
  segment(0, 1, 0, bytes.length);
  let strings = '\0';
  const add = (/** @type {any} */ value) => {
    const offset = Buffer.byteLength(strings);
    strings += `${value}\0`;
    return offset;
  };
  const dynamic = needed.map((name) => [1, add(name)]);
  if (soname) dynamic.push([14, add(soname)]);
  if (rpath !== undefined) dynamic.push([29, add(rpath)]);
  dynamic.push([5, 0x1600], [10, Buffer.byteLength(strings)], [0, 0]);
  segment(1, 2, 0x200, dynamic.length * 16);
  for (let i = 0; i < dynamic.length; i++) {
    bytes.writeBigUInt64LE(BigInt(dynamic[i][0]), 0x200 + i * 16);
    bytes.writeBigUInt64LE(BigInt(dynamic[i][1]), 0x208 + i * 16);
  }
  Buffer.from(strings).copy(bytes, 0x600);
  if (interpreter) {
    segment(2, 3, 0x180, interpreter.length + 1);
    Buffer.from(`${interpreter}\0`).copy(bytes, 0x180);
  }
  return bytes;
}

async function fixture(/** @type {any} */ t, profile = 'baseline') {
  const root = await mkdtemp(path.join(tmpdir(), 'qt-runtime-contract-'));
  t.after(() => rm(root, { recursive: true, force: true }));
  const prefix = path.join(root, 'qt'),
    materials = path.join(root, 'input');
  await mkdir(prefix);
  await mkdir(materials);
  const app = path.join(root, 'app'),
    bundle = path.join(root, 'app.hbc');
  const manifest = path.join(materials, 'candidate.json');
  const appBytes = elf({
    needed: ['libQt6Core.so.6', 'libQt6Quick.so.6', 'libhermes.so'],
    interpreter: '/usr/lib/ldqnx-64.so.2',
    rpath: '$ORIGIN/../lib',
  });
  await writeFile(app, appBytes);
  await writeFile(bundle, 'fixture HBC; not executable');
  /** @type {any} */
  const m = {
    schemaVersion: 1,
    profile,
    repository: {
      commit: '3b414f185ec9b6c23eb7fbfdbc0afc9bfed18b94',
      sourceMaterial: 'app-source',
    },
    app: {
      sha256: sha(appBytes),
      bundleSha256: sha('fixture HBC; not executable'),
      hermesSha256: sha('fixture Hermes source'),
      buildMaterial: 'app-build',
    },
    abi: { ...ABI },
    qt: {
      channel: 'public-source',
      version: '6.12.0',
      hostVersion: '6.12.0',
      targetVersion: '6.12.0',
      sourceUrl:
        'https://download.qt.io/official_releases/qt/6.12/6.12.0/single/qt-everywhere-src-6.12.0.tar.xz',
      sourceSha256: sha('fixture Qt source; not an archive'),
      sourceMaterial: 'qt-source',
      modificationsMaterial: 'changes',
      recipeMaterial: 'recipe',
      configurationMaterial: 'config',
      toolchainMaterial: 'toolchain',
      compiler: 'fixture QCC identity',
      sysrootSha256: sha('fixture sysroot inventory'),
      qpaPlugin: 'plugins/platforms/libqqnx.so',
    },
    materials: [],
    components: [],
    files: [],
    qmlModules: [],
    qmlRoots: [...ROOTS],
    inlineSources: ['inline'],
    dynamicPlugins: ['plugins/platforms/libqqnx.so'],
    systemDependencies: [
      { name: 'libc.so.3', sha256: sha('fixture libc identity'), material: 'system' },
      {
        name: '/usr/lib/ldqnx-64.so.2',
        sha256: sha('fixture loader identity'),
        material: 'system',
      },
    ],
    replacement: {
      mechanism: 'shared-library',
      instructionsMaterial: 'replacement',
      installationMaterial: 'installation',
    },
  };
  const material = async (
    /** @type {any} */ id,
    /** @type {any} */ role,
    /** @type {any} */ content
  ) => {
    const file = `${id}.txt`,
      existing = m.materials.find((/** @type {any} */ r) => r.id === id);
    await writeFile(path.join(materials, file), content);
    const entry = { id, path: file, sha256: sha(content), role };
    if (existing) Object.assign(existing, entry);
    else m.materials.push(entry);
  };
  for (const [id, role, content] of [
    ['app-source', 'source', 'fixture repository source archive'],
    ['qt-source', 'source', 'fixture Qt source; not an archive'],
    ['changes', 'modifications', 'fixture no changes'],
    ['recipe', 'build-recipe', 'fixture recipe'],
    ['config', 'configuration', 'fixture options'],
    ['toolchain', 'toolchain', 'fixture compiler inventory'],
    ['qt-license', 'license', 'fixture LGPL text'],
    ['notice', 'notice', 'fixture module and QPA notices'],
    ['replacement', 'replacement', 'fixture shared-library replacement instructions'],
    ['installation', 'installation', 'fixture installation information'],
    [
      'inline',
      'inline-source',
      'const auto qml = R"(import QtQuick; import QtQuick.Shapes; Item {})";',
    ],
    ['system', 'system', 'fixture separately provided target-system identities'],
  ])
    await material(id, role, content);
  m.components.push({
    id: 'qt',
    version: '6.12.0',
    channel: 'public-source',
    sourceMaterial: 'qt-source',
    buildMaterial: 'qt-build',
    licenseMaterials: ['qt-license'],
    noticeMaterial: 'notice',
    terms: 'LGPL-3.0-only',
  });
  await material('hermes-source', 'source', 'fixture Hermes source');
  await material('hermes-license', 'license', 'fixture Hermes license');
  m.components.push({
    id: 'hermes',
    version: 'fixture',
    channel: 'public-source',
    sourceMaterial: 'hermes-source',
    buildMaterial: 'hermes-build',
    licenseMaterials: ['hermes-license'],
    noticeMaterial: 'notice',
    terms: 'MIT',
  });
  const file = async (
    /** @type {any} */ name,
    /** @type {any} */ kind,
    /** @type {any} */ data,
    component = 'qt'
  ) => {
    const destination = path.join(prefix, name);
    await mkdir(path.dirname(destination), { recursive: true });
    await writeFile(destination, data);
    const entry = { path: name, kind, sha256: sha(data), component };
    const existing = m.files.find((/** @type {any} */ r) => r.path === name);
    if (existing) Object.assign(existing, entry);
    else m.files.push(entry);
  };
  const module = async (/** @type {any} */ uri, body = '', qml = '') => {
    const directory = `qml/${uri.replaceAll('.', '/')}`;
    m.qmlModules.push({ uri, path: directory, dynamicImports: [] });
    await file(`${directory}/qmldir`, 'qmldir', `module ${uri}\n${body}\n`);
    if (qml) await file(`${directory}/Fixture.qml`, 'qml', qml);
  };
  for (const name of ['Core', 'Quick', 'Qml', 'Network'])
    await file(
      `lib/libQt6${name}.so.6`,
      'elf',
      elf({
        soname: `libQt6${name}.so.6`,
        needed: name === 'Quick' ? ['libQt6Qml.so.6', 'libQt6Network.so.6'] : ['libc.so.3'],
      })
    );
  await file('lib/libhermes.so', 'elf', elf({ soname: 'libhermes.so' }), 'hermes');
  await file(
    'plugins/platforms/libqqnx.so',
    'elf',
    elf({ needed: ['libQt6Core.so.6', 'libscreen.so.1'] })
  );
  m.systemDependencies.push({
    name: 'libscreen.so.1',
    sha256: sha('fixture Screen'),
    material: 'system',
  });
  await module('QtQml');
  await module('QtQuick', 'depends QtQml auto\nplugin qtquick2plugin', 'import QtQml\nItem {}');
  await file('qml/QtQuick/libqtquick2plugin.so', 'elf', elf({ needed: ['libQt6Quick.so.6'] }));
  await module('QtQuick.Shapes', 'depends QtQuick auto\nplugin qmlshapesplugin');
  await file(
    'qml/QtQuick/Shapes/libqmlshapesplugin.so',
    'elf',
    elf({ needed: ['libQt6Quick.so.6'] })
  );
  if (profile === 'desktop') {
    m.qmlRoots.push('QtQuick.Controls', 'QtQuick.Controls.Basic', 'QtQuick.Templates');
    await module('QtQuick.Templates', 'depends QtQuick auto\nplugin templatesplugin');
    await file(
      'qml/QtQuick/Templates/libtemplatesplugin.so',
      'elf',
      elf({ needed: ['libQt6Quick.so.6'] })
    );
    await module('QtQuick.Controls', 'depends QtQuick.Controls.Basic auto');
    await module(
      'QtQuick.Controls.Basic',
      'depends QtQuick.Templates auto\nplugin basicplugin\nSwitch 6.0 Switch.qml'
    );
    await file(
      'qml/QtQuick/Controls/Basic/libbasicplugin.so',
      'elf',
      elf({ needed: ['libQt6Quick.so.6'] })
    );
    await file(
      'qml/QtQuick/Controls/Basic/Switch.qml',
      'qml',
      'import QtQuick\nimport QtQuick.Templates as T\nT.Switch {}'
    );
  }
  const build = {
    qtChannel: 'public-source',
    repositoryCommit: m.repository.commit,
    appSha256: m.app.sha256,
    bundleSha256: m.app.bundleSha256,
    hermesSha256: m.app.hermesSha256,
    qtSourceSha256: m.qt.sourceSha256,
    qtVersion: m.qt.version,
    compiler: m.qt.compiler,
    sysrootSha256: m.qt.sysrootSha256,
    abi: { ...m.abi },
  };
  /** @type {Record<string, any>} */
  const componentBuilds = Object.fromEntries(
    m.components.map((/** @type {any} */ c) => [
      c.id,
      {
        channel: 'public-source',
        sourceSha256: m.materials.find((/** @type {any} */ r) => r.id === c.sourceMaterial).sha256,
        version: c.version,
        qtVersion: m.qt.version,
        compiler: m.qt.compiler,
        sysrootSha256: m.qt.sysrootSha256,
        abi: { ...m.abi },
        files: [],
      },
    ])
  );
  const save = async () => {
    for (const c of m.components) {
      componentBuilds[c.id].files = m.files
        .filter((/** @type {any} */ r) => r.component === c.id)
        .map((/** @type {any} */ r) => ({ path: r.path, sha256: r.sha256 }));
      await material(c.buildMaterial, 'component-build', canonicalJson(componentBuilds[c.id]));
    }
    await material('app-build', 'app-build', canonicalJson(build));
    await writeFile(manifest, canonicalJson(m));
  };
  await save();
  return {
    root,
    prefix,
    materials,
    m,
    build,
    componentBuilds,
    file,
    module,
    material,
    save,
    options: { manifest, qtPrefix: prefix, app, bundle, output: path.join(root, 'staged') },
  };
}

test('baseline stages real ELF-derived transitive closure without desktop-only modules', async (t) => {
  const f = await fixture(t);
  const receipt = await stageQtRuntime(f.options);
  assert.equal(receipt.execution, 'NOT RUN');
  assert.equal(receipt.releaseAuthorized, false);
  assert.deepEqual(receipt.qmlClosure, ['QtQml', 'QtQuick', 'QtQuick.Shapes']);
  assert.ok(
    receipt.dependencies.some(
      (r) => r.file === 'lib/libQt6Quick.so.6' && r.needed === 'libQt6Network.so.6'
    )
  );
  assert.ok(
    receipt.dependencies.some(
      (r) => r.needed === 'libscreen.so.1' && r.provider === 'target-system'
    )
  );
  assert.equal(
    await readFile(path.join(f.options.output, 'bin/qt.conf'), 'utf8'),
    '[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\nQmlImports=qml\n'
  );
  const onDisk = JSON.parse(
    await readFile(path.join(f.options.output, 'candidate-receipt.json'), 'utf8')
  );
  assert.deepEqual(receipt, onDisk);
  for (const record of receipt.files) {
    assert.equal(sha(await readFile(path.join(f.options.output, record.path))), record.sha256);
    assert.equal(
      (await lstat(path.join(f.options.output, record.path))).mode & 0o777,
      parseInt(record.mode, 8)
    );
  }
});

test('desktop recursively requires Basic, Templates and their implementation plugins', async (t) => {
  const f = await fixture(t, 'desktop');
  const result = await validateCandidate(f.options);
  assert.ok(result.qmlClosure.includes('QtQuick.Controls.Basic'));
  assert.ok(result.qmlClosure.includes('QtQuick.Templates'));
});

test('baseline omits installed but unselected desktop modules without copying their prefix', async (t) => {
  const f = await fixture(t);
  const extra = path.join(f.prefix, 'qml/QtQuick/Controls');
  await mkdir(extra);
  await writeFile(path.join(extra, 'qmldir'), 'module QtQuick.Controls\n');
  await writeFile(path.join(extra, 'Unused.qml'), 'import QtQuick.Controls.Basic\nItem {}');
  const receipt = await stageQtRuntime(f.options);
  assert.ok(!receipt.files.some((r) => r.path.includes('/Controls/')));
  await assert.rejects(lstat(path.join(f.options.output, 'qml/QtQuick/Controls')), {
    code: 'ENOENT',
  });
});

test('staged contents and receipt are deterministic in separate new destinations', async (t) => {
  const f = await fixture(t);
  await stageQtRuntime(f.options);
  const second = { ...f.options, output: path.join(f.root, 'second') };
  await stageQtRuntime(second);
  assert.deepEqual(
    await readFile(path.join(f.options.output, 'candidate-receipt.json')),
    await readFile(path.join(second.output, 'candidate-receipt.json'))
  );
});

test('confined library alias is retained and hashed as link text', async (t) => {
  const f = await fixture(t);
  const old = f.m.files.find((/** @type {any} */ r) => r.path === 'lib/libQt6Core.so.6');
  const bytes = await readFile(path.join(f.prefix, old.path));
  await rm(path.join(f.prefix, old.path));
  f.m.files = f.m.files.filter((/** @type {any} */ r) => r !== old);
  await f.file('lib/libQt6Core.so.6.12.0', 'elf', bytes);
  await symlink('libQt6Core.so.6.12.0', path.join(f.prefix, 'lib/libQt6Core.so.6'));
  f.m.files.push({
    path: 'lib/libQt6Core.so.6',
    kind: 'symlink',
    target: 'libQt6Core.so.6.12.0',
    sha256: sha('libQt6Core.so.6.12.0'),
    component: 'qt',
  });
  await f.save();
  await stageQtRuntime(f.options);
  assert.equal(
    await readlink(path.join(f.options.output, 'lib/libQt6Core.so.6')),
    'libQt6Core.so.6.12.0'
  );
});

/** @type {[string, (f: any) => unknown, RegExp][]} */
const mutations = [
  [
    'evaluation Qt channel',
    (/** @type {any} */ f) => {
      f.m.qt.channel = 'evaluation';
    },
    /public-source/,
  ],
  [
    'evaluation build receipt',
    (/** @type {any} */ f) => {
      f.build.qtChannel = 'evaluation';
    },
    /app-build/,
  ],
  [
    'evaluation component receipt',
    (/** @type {any} */ f) => {
      f.m.components[0].channel = 'evaluation';
    },
    /public-source/,
  ],
  [
    'evaluation component build receipt',
    (/** @type {any} */ f) => {
      f.componentBuilds.qt.channel = 'evaluation';
    },
    /component-build/,
  ],
  [
    'component build source mismatch',
    (/** @type {any} */ f) => {
      f.componentBuilds.qt.sourceSha256 = '0'.repeat(64);
    },
    /component-build/,
  ],
  [
    'private source URL',
    (/** @type {any} */ f) => {
      f.m.qt.sourceUrl = 'https://account.qt.io/private/qt.tar.xz';
    },
    /source origin/,
  ],
  [
    'host/target Qt mismatch',
    (/** @type {any} */ f) => {
      f.m.qt.hostVersion = '6.11.1';
    },
    /version mismatch/,
  ],
  [
    'missing Shapes module',
    (/** @type {any} */ f) => {
      f.m.qmlModules = f.m.qmlModules.filter((/** @type {any} */ r) => r.uri !== 'QtQuick.Shapes');
    },
    /missing QML/,
  ],
  [
    'missing inline Shapes root',
    (/** @type {any} */ f) => {
      f.m.qmlRoots.pop();
    },
    /profile QML roots/,
  ],
  [
    'missing ELF dependency',
    async (/** @type {any} */ f) => {
      f.m.files = f.m.files.filter((/** @type {any} */ r) => r.path !== 'lib/libQt6Network.so.6');
    },
    /missing ELF dependency/,
  ],
  [
    'missing QPA',
    (/** @type {any} */ f) => {
      f.m.dynamicPlugins = [];
    },
    /dynamic plugins|QPA/,
  ],
  [
    'missing QML implementation plugin',
    (/** @type {any} */ f) => {
      f.m.files = f.m.files.filter(
        (/** @type {any} */ r) => r.path !== 'qml/QtQuick/Shapes/libqmlshapesplugin.so'
      );
    },
    /unlisted|implementation plugin/,
  ],
  [
    'wrong runtime ELF machine',
    async (/** @type {any} */ f) => {
      await f.file('lib/libQt6Qml.so.6', 'elf', elf({ machine: 62 }));
    },
    /ELF ABI/,
  ],
  [
    'unhashed material tampering',
    async (/** @type {any} */ f) => {
      await writeFile(path.join(f.materials, 'qt-license.txt'), 'tampered');
    },
    /hash mismatch/,
  ],
  [
    'unhashed runtime tampering',
    async (/** @type {any} */ f) => {
      await writeFile(path.join(f.prefix, 'lib/libQt6Qml.so.6'), elf({ machine: 62 }));
    },
    /hash mismatch/,
  ],
  [
    'missing replacement instructions',
    (/** @type {any} */ f) => {
      f.m.materials = f.m.materials.filter((/** @type {any} */ r) => r.id !== 'replacement');
    },
    /replacement material/,
  ],
  [
    'missing module notices',
    (/** @type {any} */ f) => {
      f.m.components[0].noticeMaterial = 'absent';
    },
    /notice material/,
  ],
  [
    'wrong HBC pairing',
    (/** @type {any} */ f) => {
      f.build.bundleSha256 = '0'.repeat(64);
    },
    /app-build/,
  ],
  [
    'wrong compiler pairing',
    (/** @type {any} */ f) => {
      f.build.compiler = 'other compiler';
    },
    /app-build/,
  ],
  [
    'wrong sysroot pairing',
    (/** @type {any} */ f) => {
      f.build.sysrootSha256 = '0'.repeat(64);
    },
    /app-build/,
  ],
  [
    'undeclared target system dependency',
    (/** @type {any} */ f) => {
      f.m.systemDependencies = f.m.systemDependencies.filter(
        (/** @type {any} */ r) => r.name !== 'libscreen.so.1'
      );
    },
    /missing ELF/,
  ],
  [
    'Qt as target-system dependency',
    (/** @type {any} */ f) => {
      f.m.systemDependencies.push({
        name: 'libQt6Core.so.6',
        sha256: sha('x'),
        material: 'system',
      });
    },
    /Qt\/Hermes/,
  ],
  [
    'absolute build-prefix RPATH',
    async (/** @type {any} */ f) => {
      await f.file(
        'lib/libQt6Qml.so.6',
        'elf',
        elf({ soname: 'libQt6Qml.so.6', rpath: '/private/evaluation/lib' })
      );
    },
    /unsafe ELF runtime path/,
  ],
  [
    'escaping RPATH',
    async (/** @type {any} */ f) => {
      await f.file(
        'lib/libQt6Qml.so.6',
        'elf',
        elf({ soname: 'libQt6Qml.so.6', rpath: '$ORIGIN/../../escape' })
      );
    },
    /escaping ELF/,
  ],
  [
    'empty RPATH entry',
    async (/** @type {any} */ f) => {
      await f.file(
        'lib/libQt6Qml.so.6',
        'elf',
        elf({ soname: 'libQt6Qml.so.6', rpath: '$ORIGIN:' })
      );
    },
    /unsafe ELF/,
  ],
  [
    'ambiguous SONAME',
    async (/** @type {any} */ f) => {
      await f.file('lib/libDuplicate.so', 'elf', elf({ soname: 'libQt6Core.so.6' }));
    },
    /ambiguous ELF/,
  ],
  [
    'missing physical SONAME alias',
    async (/** @type {any} */ f) => {
      await f.file('lib/libOther.so', 'elf', elf({ soname: 'libMissing.so.6' }));
    },
    /physical SONAME/,
  ],
  [
    'escaping material path',
    (/** @type {any} */ f) => {
      f.m.materials[0].path = '../outside';
    },
    /unsafe relative/,
  ],
  [
    'escaping runtime path',
    (/** @type {any} */ f) => {
      f.m.files[0].path = '../../outside';
    },
    /unsafe relative/,
  ],
  [
    'SDK payload',
    (/** @type {any} */ f) => {
      f.m.files[0].path = 'lib/QtCore.a';
    },
    /non-runtime/,
  ],
  [
    'GPL-only selected module',
    (/** @type {any} */ f) => {
      f.m.components[0].terms = 'GPL-3.0-only';
    },
    /selected component terms/,
  ],
  [
    'non-ELF disguised library',
    async (/** @type {any} */ f) => {
      await f.file('lib/libQt6Qml.so.6', 'elf', 'not ELF');
    },
    /ELF file kind/,
  ],
  [
    'ELF disguised as asset',
    (/** @type {any} */ f) => {
      f.m.files[0].kind = 'asset';
    },
    /lib contains only/,
  ],
  [
    'unlisted QML content',
    async (/** @type {any} */ f) => {
      await writeFile(path.join(f.prefix, 'qml/QtQuick/Hidden.qml'), 'import Hidden.Module');
    },
    /unlisted/,
  ],
  [
    'relative QML import escape',
    async (/** @type {any} */ f) => {
      await f.file('qml/QtQuick/Fixture.qml', 'qml', 'import "../../../secret"\nItem {}');
    },
    /unsafe relative|missing local/,
  ],
  [
    'recursive QML import missing',
    async (/** @type {any} */ f) => {
      await f.file('qml/QtQuick/Fixture.qml', 'qml', 'import Missing.Module\nItem {}');
    },
    /missing QML closure/,
  ],
  [
    'inline-only additional import missing',
    async (/** @type {any} */ f) => {
      await f.material('inline', 'inline-source', 'R"(import QtQuick; import Missing.Inline;)"');
    },
    /missing QML closure/,
  ],
  [
    'symlinked material',
    async (/** @type {any} */ f) => {
      await rm(path.join(f.materials, 'qt-license.txt'));
      await symlink('../app', path.join(f.materials, 'qt-license.txt'));
    },
    /symlinked input/,
  ],
  [
    'symlinked prefix directory',
    async (/** @type {any} */ f) => {
      await rm(path.join(f.prefix, 'lib'), { recursive: true });
      await symlink(f.root, path.join(f.prefix, 'lib'));
    },
    /symlinked input/,
  ],
  [
    'unknown field',
    (/** @type {any} */ f) => {
      f.m.releaseAuthorized = true;
    },
    /unknown field/,
  ],
  [
    'too many entries',
    (/** @type {any} */ f) => {
      f.m.qmlRoots = Array(LIMITS.entries + 1).fill('QtQuick');
    },
    /bounded/,
  ],
];
for (const [name, mutate, error] of mutations)
  test(`rejects ${name} before creating output`, async (t) => {
    const f = await fixture(t);
    await mutate(f);
    await f.save();
    await assert.rejects(stageQtRuntime(f.options), error);
    await assert.rejects(lstat(f.options.output), { code: 'ENOENT' });
  });

for (const missing of ['QtQuick.Controls.Basic', 'QtQuick.Templates'])
  test(`desktop rejects missing ${missing}`, async (t) => {
    const f = await fixture(t, 'desktop');
    f.m.qmlModules = f.m.qmlModules.filter((/** @type {any} */ r) => r.uri !== missing);
    await f.save();
    await assert.rejects(validateCandidate(f.options), /missing QML/);
  });

test('desktop rejects missing Basic implementation plugin', async (t) => {
  const f = await fixture(t, 'desktop');
  f.m.files = f.m.files.filter(
    (/** @type {any} */ r) => r.path !== 'qml/QtQuick/Controls/Basic/libbasicplugin.so'
  );
  await f.save();
  await assert.rejects(validateCandidate(f.options), /unlisted|implementation plugin/);
});

test('baseline does not accept desktop modules or QtQuick.Layouts', async (t) => {
  const f = await fixture(t, 'desktop');
  f.m.profile = 'baseline';
  await f.save();
  await assert.rejects(validateCandidate(f.options), /desktop-only/);
  f.m.profile = 'desktop';
  await f.module('QtQuick.Layouts');
  await f.save();
  await assert.rejects(validateCandidate(f.options), /Yoga owns layout/);
});

test('output cannot overwrite owner data or be created inside input roots', async (t) => {
  const f = await fixture(t);
  await mkdir(f.options.output);
  await writeFile(path.join(f.options.output, 'owner.txt'), 'preserve');
  await assert.rejects(stageQtRuntime(f.options), { code: 'EEXIST' });
  assert.equal(await readFile(path.join(f.options.output, 'owner.txt'), 'utf8'), 'preserve');
  await assert.rejects(
    stageQtRuntime({ ...f.options, output: path.join(f.prefix, 'candidate') }),
    /outside/
  );
  await assert.rejects(
    stageQtRuntime({ ...f.options, output: path.join(f.materials, 'candidate') }),
    /outside/
  );
});

test('symlink escapes and chains are rejected', async (t) => {
  const f = await fixture(t);
  const entry = {
    path: 'lib/libAlias.so',
    kind: 'symlink',
    component: 'qt',
    target: '../../app',
    sha256: sha('../../app'),
  };
  f.m.files.push(entry);
  await symlink(entry.target, path.join(f.prefix, entry.path));
  await f.save();
  await assert.rejects(validateCandidate(f.options), /symlink target/);
  entry.target = 'libSecond.so';
  entry.sha256 = sha(entry.target);
  await rm(path.join(f.prefix, entry.path));
  await symlink(entry.target, path.join(f.prefix, entry.path));
  await symlink('libQt6Core.so.6', path.join(f.prefix, 'lib/libSecond.so'));
  await f.save();
  await assert.rejects(validateCandidate(f.options), /symlinked input/);
});

test('ELF parser rejects truncation, bad tables, missing dynamic segment and class mismatch', () => {
  assert.throws(() => inspectElf(Buffer.alloc(16), ABI), /ELF/);
  const badClass = elf();
  badClass[4] = 1;
  assert.throws(() => inspectElf(badClass, ABI), /class/);
  const overflow = elf();
  overflow.writeBigUInt64LE(0xffffffffffffffffn, 32);
  assert.throws(() => inspectElf(overflow, ABI), /overflow/);
  const range = elf();
  range.writeBigUInt64LE(50000n, 32);
  assert.throws(() => inspectElf(range, ABI), /bounds/);
  const noDynamic = elf();
  noDynamic.writeUInt32LE(1, 64 + 56);
  assert.throws(() => inspectElf(noDynamic, ABI), /dynamic/);
  const noNul = elf();
  noNul.fill(1, 0x200, 0x200 + 5 * 16);
  assert.throws(() => inspectElf(noNul, ABI), /overflow|unterminated/);
});

test('bounded manifest and confined relative paths fail closed', async (t) => {
  const f = await fixture(t);
  await writeFile(f.options.manifest, ' '.repeat(LIMITS.manifestBytes + 1));
  await assert.rejects(validateCandidate(f.options), /file size/);
  for (const name of ['/tmp/x', '../x', 'a/../x', 'a//x', 'a\\x', 'a/./x', 'C:/x', 'a/\0x']) {
    assert.throws(() => relativePath(name), /path/);
  }
});

test('CLI rejects missing and duplicate options; root execution is refused', async (t) => {
  await assert.rejects(main([]), /usage/);
  await assert.rejects(
    main(['--manifest', 'x', '--manifest', 'x', '--app', 'x', '--bundle', 'x', '--output', 'x']),
    /duplicate/
  );
  t.mock.method(/** @type {any} */ (process), 'getuid', () => 0);
  await assert.rejects(stageQtRuntime({}), /root execution/);
});
