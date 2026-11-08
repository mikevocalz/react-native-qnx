import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { lstat, mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import {
  planNativeBuild,
  buildNativeMain,
} from '../../packages/react-native-qnx-cli/src/build-native.mjs';

const root = await mkdtemp(path.join(tmpdir(), 'rnqnx-native-build-'));
const project = path.join(root, 'my app');
await mkdir(project);
await writeFile(
  path.join(project, 'app.json'),
  JSON.stringify({
    expo: {
      name: 'My App',
      extra: { rnqnx: { appId: 'org.example.myapp', componentName: 'main' } },
    },
  })
);
let count = 0;
async function caseNamed(fn, label) {
  await fn();
  count++;
}
async function createDescriptor(platform, filename = 'app.bundle.js') {
  const dir = path.join(project, 'build', platform);
  await mkdir(dir, { recursive: true });
  const bundle = path.join(dir, filename);
  const bytes = Buffer.from('real local JS fixture');
  await writeFile(bundle, bytes);
  await writeFile(
    path.join(dir, 'native-app.json'),
    JSON.stringify(
      {
        schemaVersion: 1,
        generatedBy: 'react-native-qnx/prepare-native-v1',
        appId: 'org.example.myapp',
        displayName: 'My App',
        componentName: 'main',
        platform,
        bundle,
        bundleSha256: createHash('sha256').update(bytes).digest('hex'),
      },
      null,
      2
    ) + '\n'
  );
  return { dir, bundle };
}
try {
  await createDescriptor('qnx');
  await createDescriptor('marathon');
  await caseNamed(async () => {
    const plan = await planNativeBuild(project, 'qnx', true);
    assert.equal(plan.platform, 'qnx');
    assert.equal(plan.preview, true);
    assert.equal(plan.nativeBuildVerified, false);
    assert.ok(plan.configure.includes('-DRNQNX_CONSUMER_HOST_PREVIEW=ON'));
    assert.equal(plan.build[0], 'cmake');
    assert.equal(plan.build[1], '--build');
    assert.match(plan.output, /org\.example\.myapp/);
  }, 'explicit Linux preview');
  await caseNamed(async () => {
    const plan = await planNativeBuild(project, 'marathon');
    assert.equal(plan.platform, 'marathon');
    assert.equal(plan.configure[2], 'linux-host-renderer');
    assert.ok(plan.configure.includes('-DRNQNX_BUILD_MARATHON_HOST=ON'));
    assert.ok(!plan.configure.includes('-DRNQNX_CONSUMER_HOST_PREVIEW=ON'));
    assert.ok(plan.output.includes('/marathon/'));
  }, 'Marathon Linux target');
  await caseNamed(async () => {
    await assert.rejects(planNativeBuild(project, 'marathon', true), /host-preview/);
    await assert.rejects(planNativeBuild(project, 'bogus'), /qnx or marathon/);
    await assert.rejects(planNativeBuild(project, 'qnx'), /matching Hermes.*hbc/);
  }, 'target rejection');
  await caseNamed(async () => {
    const file = path.join(project, 'build/qnx/app.bundle.js');
    await writeFile(file, 'changed');
    await assert.rejects(planNativeBuild(project, 'qnx', true), /stale/);
    await writeFile(file, 'real local JS fixture');
  }, 'stale bundle rejects');
  await caseNamed(async () => {
    const file = path.join(project, 'build/marathon/native-app.json');
    const doc = JSON.parse(await readFile(file, 'utf8'));
    doc.appId = 'org.example.wrong';
    await writeFile(file, JSON.stringify(doc));
    await assert.rejects(planNativeBuild(project, 'marathon'), /stale/);
    doc.appId = 'org.example.myapp';
    await writeFile(file, JSON.stringify(doc));
  }, 'identity drift rejects');
  await caseNamed(async () => {
    await assert.rejects(
      buildNativeMain([
        '--project',
        project,
        '--platform',
        'qnx',
        '--host-preview',
        '--host-preview',
      ]),
      /Duplicate/
    );
    await assert.rejects(
      buildNativeMain(['--project', project, '--platform', 'bogus']),
      /qnx or marathon/
    );
  }, 'duplicate/invalid CLI');
  await caseNamed(async () => {
    const plan = await planNativeBuild(project, 'qnx', true);
    await assert.rejects(lstat(plan.buildDir), { code: 'ENOENT' });
    const rc = await buildNativeMain(['--project', project, '--host-preview', '--dry-run']);
    assert.equal(rc, 0);
    await assert.rejects(lstat(plan.buildDir), { code: 'ENOENT' });
  }, 'dry run does not create build');
  console.log(
    'Native consumer build planning: ' + count + ' checks passed (no native build or device claim).'
  );
} finally {
  await rm(root, { recursive: true, force: true });
}
