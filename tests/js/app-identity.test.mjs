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
import { identityToolForTests } from './helpers/identity-tool.mjs';

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
    await rejectsWith(
      () => validateIconSource(project, { expo: { icon: `./assets/${name}` } }),
      pattern
    );
  }
  await writeFile(path.join(project, 'assets', 'ok.png'), pngHeaderOnly(1024, 1024));
  assert.equal(
    (await validateIconSource(project, { expo: { icon: './assets/ok.png' } })).width,
    1024
  );
  checks++;
  await rejectsWith(() => validateIconSource(project, { expo: {} }), /No app icon is set/);
  await rejectsWith(
    () => validateIconSource(project, { expo: { icon: '../outside.png' } }),
    /expo.icon "..\/outside.png" is not a readable file inside the project/
  );
  await rejectsWith(
    () => validateIconSource(project, { expo: { icon: './assets/missing.png' } }),
    /missing\.png/
  );
  assert.deepEqual(
    iconSetting({ expo: { icon: './a.png', extra: { rnqnx: { icon: './b.png' } } } }),
    { field: 'expo.extra.rnqnx.icon', relative: 'b.png', declared: './b.png' }
  );
  checks++;

  // Tile label: shortName wins; expo.name stays the fallback.
  assert.deepEqual(resolveTileLabel({ expo: { name: 'Field Notes Pro' } }), {
    field: 'expo.name',
    label: 'Field Notes Pro',
  });
  assert.deepEqual(
    resolveTileLabel({
      expo: { name: 'Field Notes Pro', extra: { rnqnx: { shortName: 'Notes' } } },
    }),
    { field: 'expo.extra.rnqnx.shortName', label: 'Notes' }
  );
  checks += 2;
  for (const shortName of ['', ' Notes', 'Notes\nTwo', '\ud800', 'a'.repeat(129), 7])
    throwsWith(
      () => resolveTileLabel({ expo: { name: 'Notes', extra: { rnqnx: { shortName } } } }),
      /shortName must be 1-128 characters/
    );
  // Emoji, CJK and RTL text are valid label text; their width is the tool's job.
  for (const shortName of ['学習ノート', 'Notes 📝', 'פתקים', '👩‍💻'])
    assert.equal(
      resolveTileLabel({ expo: { name: 'x', extra: { rnqnx: { shortName } } } }).label,
      shortName
    );
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
    /Tile label "Field Notes Professional Edition" \(expo\.name\) is [\d.]+ px too wide for the Marathon launcher: it measures [\d.]+ px in Source Sans 3 Regular \/ Source Sans 3 Light at 20 px, and the label box is 92\.4 px wide on one line\. Set expo\.extra\.rnqnx\.shortName to a shorter label, for example "Field/
  );
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
  await verifyPreparedIdentity(realApp, { expo }, descriptor);
  checks++;
  // Drift is caught without the tool: a changed shortName or icon file.
  await rejectsWith(
    () =>
      verifyPreparedIdentity(
        realApp,
        { expo: { ...expo, extra: { rnqnx: { ...expo.extra.rnqnx, shortName: 'Other' } } } },
        descriptor
      ),
    /stale/
  );
  await writeFile(descriptor.icon.sizes[0].path, 'tampered');
  await rejectsWith(() => verifyPreparedIdentity(realApp, { expo }, descriptor), /stale/);

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
