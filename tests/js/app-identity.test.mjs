import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import {
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
  await rejectsWith(() => validateIconSource(project, {}), /No app icon is set/);
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
  // Without a tool the message says how to build one.
  const saved = process.env.RNQNX_IDENTITY_TOOL;
  delete process.env.RNQNX_IDENTITY_TOOL;
  await rejectsWith(
    () => prepareNativeMain(['--project', app, '--platform', 'marathon']),
    /RNQNX_BUILD_IDENTITY_TOOL=ON/
  );
  if (saved !== undefined) process.env.RNQNX_IDENTITY_TOOL = saved;

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

  // --skip-identity-tool: no tool, no icons, label recorded as not measured,
  // and build-native's re-verification accepts the empty set.
  const savedTool = process.env.RNQNX_IDENTITY_TOOL;
  delete process.env.RNQNX_IDENTITY_TOOL;
  const skipped = path.join(temporary, 'skipped');
  await mkdir(path.join(skipped, 'assets'), { recursive: true });
  await mkdir(path.join(skipped, 'build', 'qnx'), { recursive: true });
  await copyFile(templateIcon, path.join(skipped, 'assets', 'icon.png'));
  await writeFile(path.join(skipped, 'build', 'qnx', 'app.bundle.js'), 'bundle\n');
  await writeApp(skipped, expo);
  await prepareNativeMain(['--project', skipped, '--platform', 'qnx', '--skip-identity-tool']);
  const skippedDescriptor = JSON.parse(
    await readFile(path.join(skipped, 'build', 'qnx', 'native-app.json'), 'utf8')
  );
  assert.deepEqual(skippedDescriptor.icon.sizes, []);
  assert.equal(skippedDescriptor.labelFit.status, 'not-measured');
  assert.match(skippedDescriptor.labelFit.reason, /--skip-identity-tool/);
  await verifyPreparedIdentity(await realpath(skipped), expo, skippedDescriptor);
  await rejectsWith(() => lstat(path.join(skipped, 'build', 'qnx', 'icons')), /ENOENT/);
  await rejectsWith(
    () =>
      prepareNativeMain(['--project', skipped, '--skip-identity-tool', '--identity-tool', standIn]),
    /either --identity-tool or --skip-identity-tool/
  );
  if (savedTool !== undefined) process.env.RNQNX_IDENTITY_TOOL = savedTool;
  checks += 4;
  await rm(path.dirname(standIn), { recursive: true, force: true });

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
  console.log(
    `App identity: ${checks} checks passed (${real ? 'real Qt identity tool' : 'protocol stand-in; Qt measurement runs in native/app-identity CTest'}).`
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
  if (!real) await rm(path.dirname(tool), { recursive: true, force: true });
}
