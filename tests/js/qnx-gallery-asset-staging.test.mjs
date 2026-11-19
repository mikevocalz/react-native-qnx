// The gallery requires bundled images and media that `bundle-qnx` does not
// embed. scripts/stage-gallery-assets.mjs stages them where the host reads
// them, and its check fails when any required asset is not staged. PR 159's
// review found anim.gif and anim.webp missing from a release deploy because the
// documented steps copied only icon.png.
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  missingFromDeploy,
  referencedAssets,
  stageAssets,
} from '../../scripts/stage-gallery-assets.mjs';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const gallery = path.join(repoRoot, 'apps/component-gallery');
const temp = await mkdtemp(path.join(os.tmpdir(), 'rnqnx-gallery-assets-'));

try {
  // The assets ExpoImageCard.js names are all discovered.
  const referenced = (await referencedAssets(gallery)).map((asset) => asset.staged);
  for (const staged of [
    'assets/assets/icon.png',
    'assets/assets/anim.gif',
    'assets/assets/anim.webp',
    'assets/assets/media/rnqnx-test-clip.mp4',
  ]) {
    assert.ok(referenced.includes(staged), `${staged} is referenced by the gallery`);
  }

  // Staging everything satisfies the check; dropping any one file fails it.
  const deploy = path.join(temp, 'deploy');
  const staged = await stageAssets(gallery, deploy);
  assert.deepEqual(staged, referenced);
  assert.deepEqual(await missingFromDeploy(gallery, deploy), []);
  for (const name of ['anim.gif', 'anim.webp', 'icon.png']) {
    const file = path.join(deploy, 'assets/assets', name);
    const original = await readFile(file);
    await rm(file);
    const problems = await missingFromDeploy(gallery, deploy);
    assert.equal(problems.length, 1, `${name} removed: ${problems.join('; ')}`);
    assert.match(problems[0], new RegExp(`not staged: assets/assets/${name.replace('.', '\\.')}`));
    await writeFile(file, original);
  }
  await writeFile(path.join(deploy, 'assets/assets/anim.gif'), 'stale');
  assert.match((await missingFromDeploy(gallery, deploy))[0], /stale: assets\/assets\/anim\.gif/);

  // An app that requires a file it does not ship fails instead of staging nothing.
  const app = path.join(temp, 'app');
  await mkdir(path.join(app, 'assets'), { recursive: true });
  await writeFile(path.join(app, 'assets/present.png'), 'png');
  await writeFile(
    path.join(app, 'Card.js'),
    "export const a = require('./assets/present.png'); export const b = require('./assets/absent.gif');\n"
  );
  await assert.rejects(
    referencedAssets(app),
    /Card\.js requires \.\/assets\/absent\.gif, but assets\/absent\.gif does not exist/
  );
  await assert.rejects(stageAssets(app, path.join(temp, 'out')), /absent\.gif/);

  console.log('qnx-gallery-asset-staging: ok');
} finally {
  await rm(temp, { recursive: true, force: true });
}
