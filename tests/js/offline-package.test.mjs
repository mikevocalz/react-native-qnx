import test from 'node:test';
import assert from 'node:assert/strict';
import {
  mkdtemp,
  mkdir,
  writeFile,
  readFile,
  rm,
  symlink,
  rename,
  stat,
  chmod,
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import {
  createPackage,
  verifyPackage,
  neededLibraries,
  publishDirectory,
} from '../../scripts/offline-package.mjs';

// Synthetic structural ELF, never claimed executable or target evidence.
function elf() {
  const b = Buffer.alloc(120);
  b.set([127, 69, 76, 70, 2, 1, 1]);
  b.writeUInt16LE(2, 16);
  b.writeUInt16LE(62, 18);
  b.writeBigUInt64LE(64n, 32);
  b.writeUInt16LE(56, 54);
  b.writeUInt16LE(1, 56);
  b.writeUInt32LE(1, 64);
  b.writeBigUInt64LE(120n, 96);
  return b;
}
async function fixture() {
  const root = await mkdtemp(path.join(tmpdir(), 'rnqnx-package-'));
  const source = path.join(root, 'input'),
    output = path.join(root, 'release'),
    spec = path.join(root, 'spec.json');
  await mkdir(source);
  await writeFile(path.join(source, 'app'), elf());
  await writeFile(path.join(source, 'app.js'), 'globalThis.ready = true;');
  await writeFile(
    path.join(source, 'LICENSE'),
    'Fixture license inventory; not real vendor permission'
  );
  const manifest = {
    schema: 'rnqnx-offline-input/v1',
    version: '1.2.3',
    arch: 'x86_64',
    files: [
      {
        source: 'app',
        path: 'bin/app',
        role: 'executable',
        component: 'fixture',
        requires: ['js/app.js'],
      },
      { source: 'app.js', path: 'js/app.js', role: 'bundle', component: 'fixture', requires: [] },
      {
        source: 'LICENSE',
        path: 'licenses/fixture.txt',
        role: 'license',
        component: 'fixture',
        requires: [],
      },
    ],
  };
  await writeFile(spec, JSON.stringify(manifest));
  return { root, source, output, spec, manifest };
}
test('creates and verifies hashed versioned offline payload without overwriting', async () => {
  const f = await fixture();
  try {
    const manifest = await createPackage(f.spec, f.source, f.output);
    assert.equal(manifest.files.length, 3);
    assert.equal(manifest.version, '1.2.3');
    await verifyPackage(f.output);
    await assert.rejects(createPackage(f.spec, f.source, f.output), /EEXIST/);
    await writeFile(path.join(f.output, 'js/app.js'), 'changed');
    await assert.rejects(verifyPackage(f.output), /Integrity mismatch/);
  } finally {
    await rm(f.root, { recursive: true, force: true });
  }
});
test('rejects missing closure and license inventory before publication', async () => {
  const f = await fixture();
  try {
    f.manifest.files[0].requires = ['missing/asset'];
    await writeFile(f.spec, JSON.stringify(f.manifest));
    await assert.rejects(createPackage(f.spec, f.source, f.output), /Missing declared closure/);
    f.manifest.files[0].requires = [];
    f.manifest.files.pop();
    await writeFile(f.spec, JSON.stringify(f.manifest));
    await assert.rejects(createPackage(f.spec, f.source, f.output), /Missing license/);
  } finally {
    await rm(f.root, { recursive: true, force: true });
  }
});
test('rejects source and destination traversal, symlinks and unlisted payload', async () => {
  const f = await fixture();
  try {
    f.manifest.files[0].path = '../escape';
    await writeFile(f.spec, JSON.stringify(f.manifest));
    await assert.rejects(createPackage(f.spec, f.source, f.output), /Unsafe/);
    f.manifest.files[0].path = 'bin/app';
    await writeFile(f.spec, JSON.stringify(f.manifest));
    await rm(path.join(f.source, 'app'));
    await symlink('/bin/sh', path.join(f.source, 'app'));
    await assert.rejects(createPackage(f.spec, f.source, f.output), /regular/);
    await rm(path.join(f.source, 'app'));
    await writeFile(path.join(f.source, 'app'), elf());
    await createPackage(f.spec, f.source, f.output);
    await writeFile(path.join(f.output, 'unexpected'), 'oops');
    await assert.rejects(verifyPackage(f.output), /Unlisted/);
  } finally {
    await rm(f.root, { recursive: true, force: true });
  }
});
test('ELF parser validates architecture and detects real host loader/library closure without execution', async () => {
  assert.deepEqual(neededLibraries(elf(), 'x86_64'), []);
  assert.throws(() => neededLibraries(elf(), 'aarch64'), /ELF/);
  assert.throws(() => neededLibraries(Buffer.alloc(10), 'x86_64'), /ELF/);
  const real = await readFile(process.execPath);
  if (process.platform === 'linux' && process.arch === 'x64') {
    assert.ok(neededLibraries(real, 'x86_64').some((name) => /libc|ld-linux/.test(name)));
    const f = await fixture();
    try {
      await writeFile(path.join(f.source, 'app'), real);
      await assert.rejects(createPackage(f.spec, f.source, f.output), /Missing ELF dependency/);
    } finally {
      await rm(f.root, { recursive: true, force: true });
    }
  }
});

test('resolves ELF runtime/plugin dependencies and declared assets, rejects transitive gaps', async () => {
  const f = await fixture();
  try {
    const dynamic = Buffer.alloc(512);
    elf().copy(dynamic);
    dynamic.writeUInt16LE(2, 56);
    dynamic.writeBigUInt64LE(512n, 96);
    dynamic.writeUInt32LE(2, 120);
    dynamic.writeBigUInt64LE(176n, 128);
    dynamic.writeBigUInt64LE(64n, 152);
    dynamic.writeBigUInt64LE(1n, 176);
    dynamic.writeBigUInt64LE(1n, 184);
    dynamic.writeBigUInt64LE(5n, 192);
    dynamic.writeBigUInt64LE(256n, 200);
    dynamic.writeBigUInt64LE(10n, 208);
    dynamic.writeBigUInt64LE(11n, 216);
    Buffer.from('\0libfoo.so\0').copy(dynamic, 256);
    assert.deepEqual(neededLibraries(dynamic, 'x86_64'), ['libfoo.so']);
    await writeFile(path.join(f.source, 'app'), dynamic);
    await assert.rejects(
      createPackage(f.spec, f.source, f.output),
      /Missing ELF dependency libfoo.so/
    );
    await writeFile(path.join(f.source, 'library'), elf());
    await writeFile(path.join(f.source, 'plugin'), elf());
    await writeFile(path.join(f.source, 'asset'), 'image fixture');
    f.manifest.files.push(
      {
        source: 'library',
        path: 'lib/libfoo.so',
        role: 'library',
        component: 'fixture',
        requires: [],
      },
      {
        source: 'plugin',
        path: 'plugins/platforms/libplatform.so',
        role: 'plugin',
        component: 'fixture',
        requires: ['assets/image'],
      },
      { source: 'asset', path: 'assets/image', role: 'asset', component: 'fixture', requires: [] }
    );
    await writeFile(f.spec, JSON.stringify(f.manifest));
    assert.equal((await createPackage(f.spec, f.source, f.output)).files.length, 6);
    // The library itself now requires a missing second library.
    await rm(f.output, { recursive: true });
    Buffer.from('\0libbar.so\0').copy(dynamic, 256);
    await writeFile(path.join(f.source, 'library'), dynamic);
    await assert.rejects(
      createPackage(f.spec, f.source, f.output),
      /Missing ELF dependency libbar.so/
    );
  } finally {
    await rm(f.root, { recursive: true, force: true });
  }
});

test('publication failure removes only owned output and never exposes manifest early', async () => {
  const root = await mkdtemp(path.join(tmpdir(), 'rnqnx-publish-'));
  const stage = path.join(root, 'stage'),
    output = path.join(root, 'release');
  try {
    await mkdir(stage);
    await writeFile(path.join(stage, 'payload'), 'bytes');
    await writeFile(path.join(stage, 'manifest.json'), '{}');
    let calls = 0;
    await assert.rejects(
      publishDirectory(stage, output, async (from, to) => {
        await assert.rejects(stat(path.join(output, 'manifest.json')), /ENOENT/);
        if (++calls === 2) throw new Error('Injected publication I/O error');
        await rename(from, to);
      }),
      /Injected/
    );
    assert.equal(calls, 2);
    await assert.rejects(stat(output), /ENOENT/);
    await mkdir(output);
    await writeFile(path.join(output, 'existing'), 'preserved');
    await assert.rejects(publishDirectory(stage, output), /EEXIST/);
    assert.equal(await readFile(path.join(output, 'existing'), 'utf8'), 'preserved');
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test('verifier rejects a self-consistent but non-executable manifest mode', async () => {
  const f = await fixture();
  try {
    await createPackage(f.spec, f.source, f.output);
    const manifestPath = path.join(f.output, 'manifest.json');
    const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
    manifest.files.find((/** @type {{role:string}} */ entry) => entry.role === 'executable').mode =
      420;
    await chmod(path.join(f.output, 'bin/app'), 420);
    await writeFile(manifestPath, JSON.stringify(manifest));
    await assert.rejects(verifyPackage(f.output), /Integrity mismatch/);
  } finally {
    await rm(f.root, { recursive: true, force: true });
  }
});
