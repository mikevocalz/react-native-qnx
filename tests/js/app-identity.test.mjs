import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import {
  chmod,
  copyFile,
  lstat,
  mkdir,
  mkdtemp,
  readFile,
  realpath,
  rm,
  symlink,
  writeFile,
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { crc32 } from 'node:zlib';
import {
  DEFAULT_ICON,
  ICON_SIZES,
  LAUNCHER_PROFILES,
  iconSetting,
  measureTileLabel,
  readPngHeader,
  resolveTileLabel,
  validateIconSource,
  verifyPreparedIdentity,
} from '../../packages/react-native-qnx-cli/src/app-identity.mjs';
import { inspectConsumer } from '../../packages/react-native-qnx-cli/src/doctor.mjs';
import { prepareNativeMain } from '../../packages/react-native-qnx-cli/src/prepare-native.mjs';
import { identityToolForTests, standInIdentityTool } from './helpers/identity-tool.mjs';

const root = process.cwd();
const templateIcon = path.join(root, 'template/qnx/assets/icon.png');
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-app-identity-'));
const { tool, real } = await identityToolForTests();
let checks = 0;
/** @param {() => unknown} fn @param {RegExp} pattern */
function throwsWith(fn, pattern) {
  assert.throws(fn, pattern);
  checks++;
}
/** @param {() => Promise<unknown>} fn @param {RegExp} pattern */
async function rejectsWith(fn, pattern) {
  await assert.rejects(fn, pattern);
  checks++;
}
/** A PNG signature plus IHDR only: enough for header validation.
 * @param {number} width @param {number} height */
function pngHeaderOnly(width, height) {
  const ihdr = Buffer.alloc(17);
  ihdr.write('IHDR', 0, 'latin1');
  ihdr.writeUInt32BE(width, 4);
  ihdr.writeUInt32BE(height, 8);
  ihdr[12] = 8;
  ihdr[13] = 6;
  const length = Buffer.alloc(4);
  length.writeUInt32BE(13);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(ihdr));
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    length,
    ihdr,
    crc,
  ]);
}
/** @param {string} project @param {Record<string, unknown>} expo */
async function writeApp(project, expo) {
  await writeFile(path.join(project, 'app.json'), JSON.stringify({ expo }));
  // @expo/config resolves the config from the project's package.json.
  await writeFile(
    path.join(project, 'package.json'),
    '{"name":"identity-fixture","version":"1.0.0"}'
  );
}

try {
  // PNG header parsing and the icon source contract.
  const header = await readPngHeader(templateIcon);
  assert.deepEqual(header, {
    width: 1024,
    height: 1024,
    bitDepth: 8,
    colorType: 6,
    interlaced: false,
  });
  checks++;
  const project = path.join(temporary, 'app');
  await mkdir(path.join(project, 'assets'), { recursive: true });
  /** @type {Array<[string, Buffer, RegExp]>} */
  const cases = [
    ['small.png', pngHeaderOnly(512, 512), /512x512.*square PNG of at least 1024x1024/],
    ['wide.png', pngHeaderOnly(2048, 1024), /2048x1024/],
    ['fake.png', Buffer.from('\xff\xd8\xff\xe0 not a png'), /not a PNG file.*Export the icon/],
  ];
  const corrupt = pngHeaderOnly(1024, 1024);
  corrupt[20] ^= 1;
  cases.push(['corrupt.png', corrupt, /IHDR\) is corrupt/]);
  for (const [name, bytes, pattern] of cases) {
    await writeFile(path.join(project, 'assets', name), bytes);
    await rejectsWith(() => validateIconSource(project, { icon: `./assets/${name}` }), pattern);
  }
  await writeFile(path.join(project, 'assets', 'ok.png'), pngHeaderOnly(1024, 1024));
  assert.equal((await validateIconSource(project, { icon: './assets/ok.png' })).width, 1024);
  checks++;
  // No icon set: the default template icon is used, as Expo does. A field
  // that is set but empty or not a string is still an error.
  const defaulted = await validateIconSource(project, {});
  assert.equal(defaulted.defaulted, true);
  assert.equal(await realpath(defaulted.file), await realpath(templateIcon));
  assert.equal(defaulted.width, 1024);
  assert.equal((await validateIconSource(project, { icon: './assets/ok.png' })).defaulted, false);
  checks++;
  await rejectsWith(
    () => validateIconSource(project, { icon: '' }),
    /expo\.icon must be a project-relative path/
  );
  await rejectsWith(
    () => validateIconSource(project, { extra: { rnqnx: { icon: 7 } } }),
    /expo\.extra\.rnqnx\.icon must be a project-relative path/
  );
  await rejectsWith(
    () => validateIconSource(project, { icon: '../outside.png' }),
    /expo.icon "..\/outside.png" is not a readable file inside the project/
  );
  await rejectsWith(
    () => validateIconSource(project, { icon: './assets/missing.png' }),
    /missing\.png/
  );
  assert.deepEqual(iconSetting({ icon: './a.png', extra: { rnqnx: { icon: './b.png' } } }), {
    field: 'expo.extra.rnqnx.icon',
    relative: 'b.png',
    declared: './b.png',
  });
  checks++;

  // Tile label: shortName wins; expo.name stays the fallback.
  assert.deepEqual(resolveTileLabel({ name: 'Field Notes Pro' }), {
    field: 'expo.name',
    label: 'Field Notes Pro',
  });
  assert.deepEqual(
    resolveTileLabel({
      name: 'Field Notes Pro',
      extra: { rnqnx: { shortName: 'Notes' } },
    }),
    { field: 'expo.extra.rnqnx.shortName', label: 'Notes' }
  );
  checks += 2;
  for (const shortName of ['', ' Notes', 'Notes\nTwo', '\ud800', 'a'.repeat(129), 7])
    throwsWith(
      () => resolveTileLabel({ name: 'Notes', extra: { rnqnx: { shortName } } }),
      /shortName must be 1-128 characters/
    );
  // Emoji, CJK and RTL text are valid label text; their width is the tool's job.
  for (const shortName of ['学習ノート', 'Notes 📝', 'פתקים', '👩‍💻'])
    assert.equal(resolveTileLabel({ name: 'x', extra: { rnqnx: { shortName } } }).label, shortName);
  checks++;

  // Invisible format characters are rejected; ZWJ only inside emoji sequences.
  for (const shortName of [
    'Notes\u200B',
    'No\u200Btes',
    '\u202ENotes',
    'Notes\u00ADPad',
    'Notes\u2060',
    'Notes\u{E0041}',
    'A\u200DB',
  ])
    throwsWith(
      () => resolveTileLabel({ name: 'x', extra: { rnqnx: { shortName } } }),
      /invisible format character U\+[0-9A-F]{4,5}/
    );
  throwsWith(
    () => resolveTileLabel({ name: 'x', extra: { rnqnx: { shortName: '\u0301\u0302' } } }),
    /no visible character/
  );
  for (const shortName of ['👩‍💻', '👩🏽‍💻 Dev', '🏳️‍🌈'])
    assert.equal(resolveTileLabel({ name: 'x', extra: { rnqnx: { shortName } } }).label, shortName);
  checks++;

  // QNX has no launcher profile: the label is recorded as not measured and the
  // tool is not consulted.
  const qnx = measureTileLabel(
    () => {
      throw new Error('tool must not run');
    },
    'qnx',
    { field: 'expo.name', label: 'Anything at all, however long' }
  );
  assert.equal(qnx.status, 'not-measured');
  assert.equal(qnx.deviceMeasured, false);
  checks++;
  const marathon = LAUNCHER_PROFILES.marathon;
  assert.ok('fonts' in marathon);
  for (const font of marathon.fonts) assert.ok((await lstat(font)).isFile());
  checks++;

  // Overflow fails with the measured numbers and a suggestion; nothing is cut.
  throwsWith(
    () =>
      measureTileLabel(() => tool, 'marathon', {
        field: 'expo.name',
        label: 'Field Notes Professional Edition',
      }),
    /Tile label "Field Notes Professional Edition" \(expo\.name\) is [\d.]+ px too wide for the Marathon launcher: it measures [\d.]+ px in Source Sans 3 Regular \/ Source Sans 3 Light at 20 px, and the label box is 92\.4 px wide on one line with 2 px kept free\. Set expo\.extra\.rnqnx\.shortName to a shorter label, for example "Field/
  );
  // Markup is refused: the launcher's AutoText would render tags as formatting.
  throwsWith(
    () => measureTileLabel(() => tool, 'marathon', { field: 'expo.name', label: '<b>Note</b>' }),
    /looks like markup/
  );
  // Suggestions never contain markup fragments or entities.
  assert.throws(
    () =>
      measureTileLabel(() => tool, 'marathon', {
        field: 'expo.name',
        label: 'A&B Notes and More',
      }),
    (/** @type {Error} */ error) =>
      !/[<&]/.test(/for example "(.*)"\./.exec(error.message)?.[1] ?? '') &&
      /shorter label/.test(error.message)
  );
  checks++;
  const fit = measureTileLabel(() => tool, 'marathon', { field: 'expo.name', label: 'Notes' });
  assert.equal(fit.status, 'measured');
  assert.ok(fit.status === 'measured' && fit.widthPx <= fit.boxWidthPx && fit.lineCount === 1);
  checks++;
  if (real) {
    // The real Qt tool: CJK, emoji and RTL all measure on one line.
    for (const label of ['学習', 'Notes 📝', 'פתקים']) {
      const result = measureTileLabel(() => tool, 'marathon', { field: 'expo.name', label });
      assert.ok(result.status === 'measured' && result.lineCount === 1);
      checks++;
    }
  }

  // prepare-native end to end with the tool.
  const app = path.join(temporary, 'prepared');
  await mkdir(path.join(app, 'assets'), { recursive: true });
  await mkdir(path.join(app, 'build', 'marathon'), { recursive: true });
  await copyFile(templateIcon, path.join(app, 'assets', 'icon.png'));
  await writeFile(path.join(app, 'build', 'marathon', 'app.bundle.js'), 'bundle\n');
  const expo = {
    name: 'Field Notes Professional',
    icon: './assets/icon.png',
    extra: { rnqnx: { appId: 'org.rnqnx.notes', componentName: 'Notes', shortName: 'Notes' } },
  };
  await writeApp(app, expo);
  const args = ['--project', app, '--platform', 'marathon', '--identity-tool', tool];
  const descriptorPath = path.join(app, 'build', 'marathon', 'native-app.json');
  await prepareNativeMain([...args, '--dry-run']);
  await rejectsWith(() => lstat(descriptorPath), /ENOENT/);
  await rejectsWith(() => lstat(path.join(app, 'build', 'marathon', 'icons')), /ENOENT/);
  await prepareNativeMain(args);
  const realApp = await realpath(app);
  const descriptor = JSON.parse(await readFile(descriptorPath, 'utf8'));
  assert.equal(descriptor.shortName, 'Notes');
  assert.equal(descriptor.displayName, 'Field Notes Professional');
  assert.equal(descriptor.icon.source, path.join(realApp, 'assets', 'icon.png'));
  assert.equal(
    descriptor.icon.sha256,
    createHash('sha256')
      .update(await readFile(templateIcon))
      .digest('hex')
  );
  assert.deepEqual(
    descriptor.icon.sizes.map((/** @type {{size: number}} */ entry) => entry.size),
    [...ICON_SIZES]
  );
  for (const entry of descriptor.icon.sizes) {
    const bytes = await readFile(entry.path);
    assert.equal(createHash('sha256').update(bytes).digest('hex'), entry.sha256);
    assert.equal((await readPngHeader(entry.path)).width, entry.size);
  }
  assert.equal(descriptor.labelFit.status, 'measured');
  assert.equal(descriptor.labelFit.deviceMeasured, false);
  checks += 6;
  const before = (await lstat(descriptorPath)).mtimeMs;
  await prepareNativeMain(args);
  assert.equal((await lstat(descriptorPath)).mtimeMs, before);
  checks++;
  await verifyPreparedIdentity(realApp, expo, descriptor);
  checks++;
  // Drift is caught without the tool: a changed shortName or icon file.
  await rejectsWith(
    () =>
      verifyPreparedIdentity(
        realApp,
        { ...expo, extra: { rnqnx: { ...expo.extra.rnqnx, shortName: 'Other' } } },
        descriptor
      ),
    /stale/
  );
  await writeFile(descriptor.icon.sizes[0].path, 'tampered');
  await rejectsWith(() => verifyPreparedIdentity(realApp, expo, descriptor), /stale/);

  // A label that does not fit stops prepare-native before anything is written.
  const overflow = path.join(temporary, 'overflow');
  await mkdir(path.join(overflow, 'assets'), { recursive: true });
  await mkdir(path.join(overflow, 'build', 'marathon'), { recursive: true });
  await copyFile(templateIcon, path.join(overflow, 'assets', 'icon.png'));
  await writeFile(path.join(overflow, 'build', 'marathon', 'app.bundle.js'), 'bundle\n');
  await writeApp(overflow, {
    ...expo,
    extra: { rnqnx: { ...expo.extra.rnqnx, shortName: undefined } },
  });
  await rejectsWith(
    () =>
      prepareNativeMain(['--project', overflow, '--platform', 'marathon', '--identity-tool', tool]),
    /px too wide for the Marathon launcher/
  );
  await rejectsWith(
    () => lstat(path.join(overflow, 'build', 'marathon', 'native-app.json')),
    /ENOENT/
  );
  await rejectsWith(() => lstat(path.join(overflow, 'build', 'marathon', 'icons')), /ENOENT/);
  // A rejected icon also stops it, naming the file.
  await writeFile(path.join(overflow, 'assets', 'icon.png'), pngHeaderOnly(640, 640));
  await rejectsWith(
    () =>
      prepareNativeMain(['--project', overflow, '--platform', 'marathon', '--identity-tool', tool]),
    /expo\.icon "\.\/assets\/icon\.png" is 640x640/
  );
  // Without a tool prepare-native builds one with CMake. A stand-in cmake on
  // PATH plays the build here: first failing (no Qt), then producing the tool.
  const savedEnv = {
    tool: process.env.RNQNX_IDENTITY_TOOL,
    buildDir: process.env.RNQNX_IDENTITY_TOOL_BUILD_DIR,
    path: process.env.PATH,
  };
  const fakeBin = path.join(temporary, 'fake-bin');
  const cmakeLog = path.join(temporary, 'cmake.log');
  await mkdir(fakeBin);
  await writeFile(
    path.join(fakeBin, 'cmake'),
    `#!/usr/bin/env node
const fs = require('node:fs');
const path = require('node:path');
const args = process.argv.slice(2);
fs.appendFileSync(${JSON.stringify(cmakeLog)}, args.join(' ') + '\\n');
if (process.env.FAKE_CMAKE_FAIL) {
  console.error('Could not find a package configuration file provided by "Qt6"');
  process.exit(1);
}
if (args[0] === '--build') {
  const out = path.join(args[1], 'rnqnx_identity_tool');
  fs.copyFileSync(process.env.FAKE_CMAKE_TOOL, out);
  fs.chmodSync(out, 0o755);
} else fs.mkdirSync(args[args.indexOf('-B') + 1], { recursive: true });
`
  );
  await chmod(path.join(fakeBin, 'cmake'), 0o755);
  const autoBuildDir = path.join(temporary, 'auto-identity-tool');
  delete process.env.RNQNX_IDENTITY_TOOL;
  process.env.RNQNX_IDENTITY_TOOL_BUILD_DIR = autoBuildDir;
  process.env.PATH = `${fakeBin}${path.delimiter}${savedEnv.path}`;
  const autoApp = path.join(temporary, 'auto-built');
  await mkdir(path.join(autoApp, 'build', 'marathon'), { recursive: true });
  await writeFile(path.join(autoApp, 'build', 'marathon', 'app.bundle.js'), 'bundle\n');
  // No expo.icon at all: the default template icon goes through the tool.
  const { icon: _unused, ...expoWithoutIcon } = expo;
  await writeApp(autoApp, expoWithoutIcon);
  const autoArgs = ['--project', autoApp, '--platform', 'marathon'];
  /** @type {string[]} */
  const errors = [];
  const consoleError = console.error;
  console.error = (/** @type {unknown[]} */ ...line) => errors.push(line.join(' '));
  try {
    process.env.FAKE_CMAKE_FAIL = '1';
    await rejectsWith(
      () => prepareNativeMain(autoArgs),
      /Cannot build rnqnx_identity_tool, which generates the app icon set; "cmake -S" failed:\n[^]*Qt6[^]*Qt 6\.11\.1 or later[^]*No native-app\.json was written/
    );
    await rejectsWith(
      () => lstat(path.join(autoApp, 'build', 'marathon', 'native-app.json')),
      /ENOENT/
    );
    delete process.env.FAKE_CMAKE_FAIL;
    process.env.FAKE_CMAKE_TOOL = tool;
    await prepareNativeMain(autoArgs);
    assert.ok(errors.some((line) => /building rnqnx_identity_tool in /.test(line)));
    assert.ok(
      errors.some((line) =>
        /no app icon is set, so the default icon template\/qnx\/assets\/icon\.png is used\. Set expo\.icon/.test(
          line
        )
      )
    );
    const autoDescriptor = JSON.parse(
      await readFile(path.join(autoApp, 'build', 'marathon', 'native-app.json'), 'utf8')
    );
    assert.equal(autoDescriptor.icon.source, DEFAULT_ICON);
    assert.equal(
      autoDescriptor.icon.sha256,
      createHash('sha256')
        .update(await readFile(templateIcon))
        .digest('hex')
    );
    assert.deepEqual(
      autoDescriptor.icon.sizes.map((/** @type {{size: number}} */ entry) => entry.size),
      [...ICON_SIZES]
    );
    for (const entry of autoDescriptor.icon.sizes) assert.ok((await lstat(entry.path)).isFile());
    await verifyPreparedIdentity(await realpath(autoApp), expoWithoutIcon, autoDescriptor);
    checks += 4;
    // The built tool is reused: a rerun calls cmake no more.
    const callsBefore = (await readFile(cmakeLog, 'utf8')).trim().split('\n').length;
    await prepareNativeMain(autoArgs);
    assert.equal((await readFile(cmakeLog, 'utf8')).trim().split('\n').length, callsBefore);
    checks++;
  } finally {
    console.error = consoleError;
    delete process.env.FAKE_CMAKE_FAIL;
    delete process.env.FAKE_CMAKE_TOOL;
    process.env.PATH = savedEnv.path;
    if (savedEnv.buildDir === undefined) delete process.env.RNQNX_IDENTITY_TOOL_BUILD_DIR;
    else process.env.RNQNX_IDENTITY_TOOL_BUILD_DIR = savedEnv.buildDir;
    if (savedEnv.tool !== undefined) process.env.RNQNX_IDENTITY_TOOL = savedEnv.tool;
  }

  // Icons above 8192 px are refused before decoding.
  await writeFile(path.join(overflow, 'assets', 'icon.png'), pngHeaderOnly(9000, 9000));
  await rejectsWith(
    () => validateIconSource(overflow, { icon: './assets/icon.png' }),
    /9000x9000.*no larger than 8192x8192/
  );
  await writeFile(path.join(overflow, 'assets', 'icon.png'), pngHeaderOnly(640, 640));

  // A rerun on another host (different Qt, font engine or PNG encoder) keeps
  // the existing descriptor and icons when the host-independent parts match.
  const standIn = await standInIdentityTool();
  const hostA = path.join(temporary, 'host-a');
  await mkdir(path.join(hostA, 'assets'), { recursive: true });
  await mkdir(path.join(hostA, 'build', 'marathon'), { recursive: true });
  await copyFile(templateIcon, path.join(hostA, 'assets', 'icon.png'));
  await writeFile(path.join(hostA, 'build', 'marathon', 'app.bundle.js'), 'bundle\n');
  await writeApp(hostA, expo);
  const hostArgs = ['--project', hostA, '--platform', 'marathon', '--identity-tool', standIn];
  await prepareNativeMain(hostArgs);
  const hostDescriptor = path.join(hostA, 'build', 'marathon', 'native-app.json');
  const firstBytes = await readFile(hostDescriptor, 'utf8');
  process.env.RNQNX_FAKE_HOST_VARIANT = '1';
  try {
    await prepareNativeMain(hostArgs);
    assert.equal(await readFile(hostDescriptor, 'utf8'), firstBytes);
    checks++;
    // A real change is still caught.
    await writeApp(hostA, { ...expo, extra: { rnqnx: { ...expo.extra.rnqnx, shortName: 'Pad' } } });
    await rejectsWith(() => prepareNativeMain(hostArgs), /Generated native configuration differs/);
    await writeApp(hostA, expo);
    // Tampered icons on disk are caught instead of silently accepted.
    const firstIcon = JSON.parse(firstBytes).icon.sizes[0].path;
    await writeFile(firstIcon, 'tampered');
    await rejectsWith(() => prepareNativeMain(hostArgs), /stale/);
  } finally {
    delete process.env.RNQNX_FAKE_HOST_VARIANT;
  }

  // A symlink planted at an icon destination is refused, not written through.
  const planted = path.join(temporary, 'planted');
  const victim = path.join(temporary, 'victim.txt');
  await writeFile(victim, 'keep me');
  await mkdir(path.join(planted, 'assets'), { recursive: true });
  await mkdir(path.join(planted, 'build', 'marathon', 'icons'), { recursive: true });
  await copyFile(templateIcon, path.join(planted, 'assets', 'icon.png'));
  await writeFile(path.join(planted, 'build', 'marathon', 'app.bundle.js'), 'bundle\n');
  await writeApp(planted, expo);
  await symlink(victim, path.join(planted, 'build', 'marathon', 'icons', 'icon-16.png'));
  await rejectsWith(
    () =>
      prepareNativeMain([
        '--project',
        planted,
        '--platform',
        'marathon',
        '--identity-tool',
        standIn,
      ]),
    /symlink or not a regular file/
  );
  assert.equal(await readFile(victim, 'utf8'), 'keep me');
  await rejectsWith(
    () => lstat(path.join(planted, 'build', 'marathon', 'native-app.json')),
    /ENOENT/
  );
  checks++;

  // --skip-identity-tool is gone: there is no prepared app without an icon
  // set, and a descriptor with an empty set is stale to build-native.
  await rejectsWith(
    () => prepareNativeMain(['--project', app, '--platform', 'marathon', '--skip-identity-tool']),
    /--skip-identity-tool was removed: every prepared app gets a generated icon set/
  );
  await rejectsWith(
    () =>
      verifyPreparedIdentity(realApp, expo, {
        ...descriptor,
        icon: { ...descriptor.icon, sizes: [] },
      }),
    /stale/
  );
  await rm(path.dirname(standIn), { recursive: true, force: true });

  // Identity reaches native-app.json from a resolved Expo config, not only a
  // static app.json: (1) app.config.js and (2) config plugin options.
  const pluginRoot = path.join(root, 'packages', 'expo-qnx-config-plugins');
  /** @param {string} name @param {Record<string, string>} files */
  async function resolvedProject(name, files) {
    const dir = path.join(temporary, name);
    await mkdir(path.join(dir, 'assets'), { recursive: true });
    await mkdir(path.join(dir, 'art'), { recursive: true });
    await mkdir(path.join(dir, 'build', 'marathon'), { recursive: true });
    await mkdir(path.join(dir, 'node_modules', '@rn-qnx'), { recursive: true });
    await symlink(pluginRoot, path.join(dir, 'node_modules', '@rn-qnx', 'expo-qnx-config-plugins'));
    await copyFile(templateIcon, path.join(dir, 'art', 'tile.png'));
    await writeFile(path.join(dir, 'build', 'marathon', 'app.bundle.js'), 'bundle\n');
    await writeFile(path.join(dir, 'package.json'), '{"name":"resolved","version":"1.0.0"}');
    for (const [file, contents] of Object.entries(files))
      await writeFile(path.join(dir, file), contents);
    await prepareNativeMain(['--project', dir, '--platform', 'marathon', '--identity-tool', tool]);
    return JSON.parse(
      await readFile(path.join(dir, 'build', 'marathon', 'native-app.json'), 'utf8')
    );
  }
  const fromConfigJs = await resolvedProject('from-app-config-js', {
    'app.config.js': `module.exports = () => ({
  name: 'Field Notes Professional',
  slug: 'field-notes',
  extra: {
    rnqnx: {
      appId: 'org.rnqnx.notes',
      componentName: 'Notes',
      icon: './art/tile.png',
      shortName: 'Notes JS',
    },
  },
});
`,
  });
  assert.equal(fromConfigJs.shortName, 'Notes JS');
  assert.equal(path.basename(fromConfigJs.icon.source), 'tile.png');
  assert.equal(fromConfigJs.icon.sizes.length, ICON_SIZES.length);
  assert.equal(fromConfigJs.labelFit.status, 'measured');
  assert.deepEqual(Object.keys(fromConfigJs).slice(-4), [
    'window',
    'shortName',
    'icon',
    'labelFit',
  ]);
  checks++;
  const fromPlugin = await resolvedProject('from-plugin-options', {
    'app.json': JSON.stringify({
      expo: {
        name: 'Field Notes Professional',
        slug: 'field-notes',
        icon: './assets/missing-and-overridden.png',
        extra: { rnqnx: { shortName: 'Ignored by plugin' } },
        plugins: [
          [
            '@rn-qnx/expo-qnx-config-plugins',
            {
              appId: 'org.rnqnx.notes',
              componentName: 'Notes',
              icon: './art/tile.png',
              shortName: 'Pad',
            },
          ],
        ],
      },
    }),
  });
  // Plugin options win over extra.rnqnx and over expo.icon.
  assert.equal(fromPlugin.shortName, 'Pad');
  assert.equal(path.basename(fromPlugin.icon.source), 'tile.png');
  assert.equal(fromPlugin.labelFit.status, 'measured');
  // doctor sees the same plugin options statically, without running code.
  const pluginDoctor = await inspectConsumer(path.join(temporary, 'from-plugin-options'), {
    platform: 'marathon',
    identityTool: tool,
  });
  assert.equal(pluginDoctor.checks.find((check) => check.id === 'app-icon')?.status, 'passed');
  const pluginLabel = pluginDoctor.checks.find((check) => check.id === 'tile-label');
  assert.equal(pluginLabel?.status, 'passed');
  // The stand-in measures 10 px per character: 'Pad' (30 px), not the ignored extra.rnqnx value.
  if (!real) assert.equal(pluginLabel?.observed, '30.0 px');
  const jsDoctor = await inspectConsumer(path.join(temporary, 'from-app-config-js'), {
    platform: 'marathon',
  });
  assert.equal(jsDoctor.checks.find((check) => check.id === 'app-identity')?.status, 'not-checked');
  assert.match(
    jsDoctor.checks.find((check) => check.id === 'app-identity')?.message ?? '',
    /app\.config\.js is code/
  );
  checks += 2;

  // doctor reports the icon and label without running the tool by default.
  const report = await inspectConsumer(overflow, { platform: 'marathon' });
  const icon = report.checks.find((check) => check.id === 'app-icon');
  assert.equal(icon?.status, 'failed');
  assert.match(icon?.message ?? '', /640x640/);
  const label = report.checks.find((check) => check.id === 'tile-label');
  assert.equal(label?.status, 'not-checked');
  const measured = await inspectConsumer(overflow, { platform: 'marathon', identityTool: tool });
  assert.equal(measured.checks.find((check) => check.id === 'tile-label')?.status, 'failed');
  const good = await inspectConsumer(app, { platform: 'marathon', identityTool: tool });
  assert.equal(good.checks.find((check) => check.id === 'app-icon')?.status, 'passed');
  assert.equal(good.checks.find((check) => check.id === 'tile-label')?.status, 'passed');
  checks += 5;
  // doctor applies prepare-native's rules: a missing icon passes with the
  // default named, and it reports where the identity tool comes from.
  const defaultReport = await inspectConsumer(path.join(temporary, 'auto-built'), {
    platform: 'marathon',
    identityTool: tool,
  });
  const defaultIcon = defaultReport.checks.find((check) => check.id === 'app-icon');
  assert.equal(defaultIcon?.status, 'passed');
  assert.match(defaultIcon?.observed ?? '', /^default icon, 1024x1024 PNG$/);
  assert.match(
    defaultIcon?.message ?? '',
    /default icon template\/qnx\/assets\/icon\.png\. Set expo\.icon/
  );
  const toolCheck = defaultReport.checks.find((check) => check.id === 'identity-tool');
  assert.equal(toolCheck?.status, 'passed');
  assert.match(toolCheck?.message ?? '', /--identity-tool/);
  const savedForDoctor = {
    tool: process.env.RNQNX_IDENTITY_TOOL,
    buildDir: process.env.RNQNX_IDENTITY_TOOL_BUILD_DIR,
  };
  delete process.env.RNQNX_IDENTITY_TOOL;
  process.env.RNQNX_IDENTITY_TOOL_BUILD_DIR = path.join(temporary, 'never-built');
  try {
    const unbuilt = (await inspectConsumer(app, { platform: 'marathon' })).checks.find(
      (check) => check.id === 'identity-tool'
    );
    assert.equal(unbuilt?.status, 'not-checked');
    assert.match(unbuilt?.message ?? '', /prepare-native builds it in .*never-built/);
    await rejectsWith(() => lstat(path.join(temporary, 'never-built')), /ENOENT/);
    process.env.RNQNX_IDENTITY_TOOL_BUILD_DIR = path.join(temporary, 'auto-identity-tool');
    const reused = (await inspectConsumer(app, { platform: 'marathon' })).checks.find(
      (check) => check.id === 'identity-tool'
    );
    assert.equal(reused?.status, 'passed');
    assert.match(reused?.message ?? '', /automatic build/);
  } finally {
    if (savedForDoctor.buildDir === undefined) delete process.env.RNQNX_IDENTITY_TOOL_BUILD_DIR;
    else process.env.RNQNX_IDENTITY_TOOL_BUILD_DIR = savedForDoctor.buildDir;
    if (savedForDoctor.tool !== undefined) process.env.RNQNX_IDENTITY_TOOL = savedForDoctor.tool;
  }
  checks += 4;
  console.log(
    `App identity: ${checks} checks passed (${real ? 'real Qt identity tool' : 'protocol stand-in; Qt measurement runs in native/app-identity CTest'}).`
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
  if (!real) await rm(path.dirname(tool), { recursive: true, force: true });
}
