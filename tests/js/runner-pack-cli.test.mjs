import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { chmod, mkdir, mkdtemp, readFile, readdir, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { gunzipSync } from 'node:zlib';
import {
  listTarGz,
  validateRunnerManifest,
} from '../../packages/react-native-qnx-cli/src/runner.mjs';
import { publishMain } from '../../scripts/publish-runner-release.mjs';

// Host test. `rnqnx runner pack` runs as a child process on a synthetic stage
// with a stand-in hermesc that writes a Hermes bytecode header (version 99).
// No runner binary runs, nothing is uploaded, and nothing touches a board.
const bin = path.join(process.cwd(), 'packages/react-native-qnx-cli/src/bin.mjs');
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-pack-cli-'));
const sha256 = (/** @type {Buffer} */ bytes) => createHash('sha256').update(bytes).digest('hex');

const hermesc = path.join(temporary, 'fake-hermesc');
// Called as: hermesc -emit-binary -out <file> <source>. Magic, then version 99.
await writeFile(
  hermesc,
  '#!/bin/sh\nprintf \'\\306\\037\\274\\003\\301\\003\\031\\037\\143\\000\\000\\000\' > "$3"\n'
);
await chmod(hermesc, 0o755);

const stage = path.join(temporary, 'stage');
await mkdir(path.join(stage, 'lib'), { recursive: true });
await writeFile(path.join(stage, 'bin-runner'), 'synthetic runner\n');
await writeFile(path.join(stage, 'lib/libfixture.so'), 'synthetic library\n');
await chmod(path.join(stage, 'bin-runner'), 0o755);
const builtinsFile = path.join(temporary, 'builtins.json');
await writeFile(
  builtinsFile,
  JSON.stringify({
    fabricComponents: [],
    expoModules: [{ version: '1.0.0', package: '@rn-qnx/expo-constants-qnx' }],
    nitroModules: [],
  })
);
const libsFile = path.join(temporary, 'libs.txt');
await writeFile(libsFile, 'libscreen.so.1\nlibc.so.6\n');

/** @param {string} out @param {string[]} [extra] @param {NodeJS.ProcessEnv} [env] */
const pack = (out, extra = [], env = {}) =>
  spawnSync(
    process.execPath,
    [
      bin,
      'runner',
      'pack',
      '--stage',
      stage,
      '--out',
      out,
      '--id',
      'pi400-test',
      '--target',
      'qnx-aarch64-pi400',
      '--hermesc',
      hermesc,
      '--hermes',
      'hermes-v260318099.0.5',
      '--qt',
      '6.12.0',
      '--sdp',
      '8.0.5',
      '--builtins',
      builtinsFile,
      '--target-libs',
      libsFile,
      '--react-native-qnx',
      '0.1.0',
      '--react-native',
      '0.88.1',
      '--nitro-abi',
      '1.0.0-alpha.nightly.136.gdb81de61bd32',
      ...extra,
    ],
    { encoding: 'utf8', env: { ...process.env, SOURCE_DATE_EPOCH: '', ...env } }
  );

/** @type {string[]} */
const cases = [];
/** @param {string} name @param {() => Promise<void>} body */
const check = async (name, body) => {
  await body();
  cases.push(name);
};

try {
  const out1 = path.join(temporary, 'out1');
  const first = pack(out1, ['--published-at', '2026-11-20T00:00:00Z']);

  await check('pack writes the archive, manifest and descriptor', async () => {
    assert.equal(first.status, 0, first.stderr);
    assert.deepEqual((await readdir(out1)).sort(), [
      'pi400-test.tar.gz',
      'rnqnx-runner-manifest.json',
      'rnqnx-runtime.json',
    ]);
    const report = JSON.parse(first.stdout);
    assert.equal(report.published, false);
    assert.equal(report.entries, 3);
  });

  const archive = await readFile(path.join(out1, 'pi400-test.tar.gz'));
  const manifestText = await readFile(path.join(out1, 'rnqnx-runner-manifest.json'), 'utf8');
  const descriptorText = await readFile(path.join(out1, 'rnqnx-runtime.json'), 'utf8');

  await check('manifest records the archive sha256 and size', async () => {
    const manifest = validateRunnerManifest(JSON.parse(manifestText));
    const entry = manifest.runners[0];
    assert.equal(entry.sha256, sha256(archive));
    assert.equal(entry.size, archive.length);
    assert.equal(entry.url, 'pi400-test.tar.gz');
    assert.equal(entry.runtime?.hermesBytecodeVersion, 99);
    assert.deepEqual(entry.targetLibraries, ['libc.so.6', 'libscreen.so.1']);
    assert.equal(manifest.publishedAt, '2026-11-20T00:00:00Z');
  });

  await check('JSON files have sorted keys and one trailing newline', async () => {
    for (const text of [manifestText, descriptorText]) {
      assert.ok(text.endsWith('}\n') && !text.endsWith('\n\n'));
      const walk = (/** @type {unknown} */ value) => {
        if (Array.isArray(value)) value.forEach(walk);
        else if (value && typeof value === 'object') {
          const keys = Object.keys(value);
          assert.deepEqual(keys, [...keys].sort());
          Object.values(value).forEach(walk);
        }
      };
      walk(JSON.parse(text));
    }
  });

  await check('archive holds the staged files plus the identical descriptor', async () => {
    assert.deepEqual(await listTarGz(path.join(out1, 'pi400-test.tar.gz')), [
      'bin-runner',
      'lib/libfixture.so',
      'rnqnx-runtime.json',
    ]);
    const tar = gunzipSync(archive);
    assert.ok(tar.includes(Buffer.from(descriptorText)), 'descriptor bytes sit inside the tar');
    // First header: name, then mtime and owner fields are zero.
    assert.equal(tar.subarray(0, 9).toString(), 'bin-runne');
    assert.equal(tar.subarray(136, 148).toString(), '00000000000\0');
    assert.equal(tar.subarray(108, 124).toString(), '0000000\x000000000\0');
    // gzip header carries no timestamp.
    assert.deepEqual([...archive.subarray(4, 8)], [0, 0, 0, 0]);
  });

  await check('a second run gives byte-identical output', async () => {
    const out2 = path.join(temporary, 'out2');
    const second = pack(out2, ['--published-at', '2026-11-20T00:00:00Z']);
    assert.equal(second.status, 0, second.stderr);
    for (const name of await readdir(out1))
      assert.ok(
        (await readFile(path.join(out1, name))).equals(await readFile(path.join(out2, name))),
        `${name} is identical`
      );
  });

  await check('SOURCE_DATE_EPOCH sets publishedAt and matches --published-at', async () => {
    const out3 = path.join(temporary, 'out3');
    const third = pack(out3, [], { SOURCE_DATE_EPOCH: '1795132800' });
    assert.equal(third.status, 0, third.stderr);
    const manifest = JSON.parse(
      await readFile(path.join(out3, 'rnqnx-runner-manifest.json'), 'utf8')
    );
    assert.equal(manifest.publishedAt, '2026-11-20T00:00:00.000Z');
    // Same archive: only the manifest time text differs.
    assert.ok(
      (await readFile(path.join(out3, 'pi400-test.tar.gz'))).equals(archive),
      'archive bytes do not depend on the time input'
    );
  });

  await check('stage order and file mtimes do not change the archive', async () => {
    const out4 = path.join(temporary, 'out4');
    await writeFile(path.join(stage, 'bin-runner'), 'synthetic runner\n'); // new mtime
    const fourth = pack(out4, ['--published-at', '2026-11-20T00:00:00Z']);
    assert.equal(fourth.status, 0, fourth.stderr);
    assert.ok((await readFile(path.join(out4, 'pi400-test.tar.gz'))).equals(archive));
  });

  // --- Failure paths. Each leaves the output directory without partial files.
  await check('no time input is refused', async () => {
    const out = path.join(temporary, 'fail-time');
    const result = pack(out);
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /--published-at|SOURCE_DATE_EPOCH/);
    await assert.rejects(readdir(out), { code: 'ENOENT' });
  });

  await check('a malformed SOURCE_DATE_EPOCH is refused', async () => {
    const result = pack(path.join(temporary, 'fail-epoch'), [], { SOURCE_DATE_EPOCH: 'yesterday' });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /SOURCE_DATE_EPOCH/);
  });

  await check('missing stage is refused', async () => {
    const result = spawnSync(
      process.execPath,
      [
        bin,
        'runner',
        'pack',
        '--stage',
        path.join(temporary, 'nope'),
        '--out',
        path.join(temporary, 'fail-stage'),
      ]
        .concat([
          '--id',
          'x1',
          '--target',
          'macos-arm64',
          '--hermesc',
          hermesc,
          '--hermes',
          'h1',
          '--qt',
          '6.12.0',
        ])
        .concat([
          '--sdp',
          'none',
          '--builtins',
          builtinsFile,
          '--published-at',
          '2026-11-20T00:00:00Z',
        ]),
      { encoding: 'utf8' }
    );
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /not a readable directory/);
  });

  await check('a missing required flag is named', async () => {
    const result = spawnSync(process.execPath, [bin, 'runner', 'pack', '--stage', stage], {
      encoding: 'utf8',
    });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /runner pack needs --out/);
  });

  await check('missing hermesc is refused', async () => {
    const result = pack(path.join(temporary, 'fail-hermesc'), [
      '--published-at',
      '2026-11-20T00:00:00Z',
      '--hermesc',
      path.join(temporary, 'no-such-hermesc'),
    ]);
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /ENOENT|no-such-hermesc/);
  });

  await check('unreadable and malformed builtins files are refused', async () => {
    const absent = pack(path.join(temporary, 'fail-b1'), [
      '--published-at',
      '2026-11-20T00:00:00Z',
      '--builtins',
      path.join(temporary, 'absent.json'),
    ]);
    assert.notEqual(absent.status, 0);
    assert.match(absent.stderr, /Cannot read --builtins/);
    const broken = path.join(temporary, 'broken.json');
    await writeFile(broken, '{ not json');
    const result = pack(path.join(temporary, 'fail-b2'), [
      '--published-at',
      '2026-11-20T00:00:00Z',
      '--builtins',
      broken,
    ]);
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /Cannot read --builtins/);
  });

  await check('a bad manifest field is refused before anything is written', async () => {
    const unknownKey = path.join(temporary, 'unknown-key.json');
    await writeFile(unknownKey, JSON.stringify({ nitroModules: [], expoModules: [], extra: [] }));
    const out = path.join(temporary, 'fail-manifest');
    const result = pack(out, ['--published-at', '2026-11-20T00:00:00Z', '--builtins', unknownKey]);
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /builtins has unknown keys: extra/);
    await assert.rejects(readdir(out), { code: 'ENOENT' });
    const badId = pack(out, ['--published-at', '2026-11-20T00:00:00Z', '--id', 'Bad Id']);
    assert.notEqual(badId.status, 0);
    assert.match(badId.stderr, /\.id is invalid/);
    const badTarget = pack(out, ['--published-at', '2026-11-20T00:00:00Z', '--target', 'plan9']);
    assert.notEqual(badTarget.status, 0);
    assert.match(badTarget.stderr, /target is not supported/);
  });

  await check('an existing output file is never overwritten', async () => {
    const result = pack(out1, ['--published-at', '2026-11-20T00:00:00Z']);
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /EEXIST/);
    assert.equal(sha256(await readFile(path.join(out1, 'pi400-test.tar.gz'))), sha256(archive));
  });

  await check('a stage that already holds rnqnx-runtime.json or a symlink is refused', async () => {
    const dirty = path.join(temporary, 'dirty-stage');
    await mkdir(dirty);
    await writeFile(path.join(dirty, 'rnqnx-runtime.json'), '{}');
    const run = (/** @type {string} */ dir) =>
      spawnSync(
        process.execPath,
        [
          bin,
          'runner',
          'pack',
          '--stage',
          dir,
          '--out',
          path.join(temporary, `o-${path.basename(dir)}`),
        ]
          .concat(['--id', 'x1', '--target', 'macos-arm64', '--hermesc', hermesc, '--hermes', 'h1'])
          .concat(['--qt', '6.12.0', '--sdp', 'none', '--builtins', builtinsFile])
          .concat([
            '--react-native-qnx',
            '0.1.0',
            '--react-native',
            '0.88.1',
            '--nitro-abi',
            '1.0.0',
          ])
          .concat(['--published-at', '2026-11-20T00:00:00Z']),
        { encoding: 'utf8' }
      );
    assert.match(run(dirty).stderr, /already contains rnqnx-runtime\.json/);
    const linked = path.join(temporary, 'linked-stage');
    await mkdir(linked);
    await symlink(stage, path.join(linked, 'link'));
    assert.match(run(linked).stderr, /is a symlink/);
  });

  // --- Publish script (dry run and refusals). A fake gh records its arguments.
  const record = path.join(temporary, 'gh-args.txt');
  const fakeGh = path.join(temporary, 'fake-gh');
  await writeFile(fakeGh, `#!/bin/sh\nprintf '%s\\n' "$@" > '${record}'\n`);
  await chmod(fakeGh, 0o755);
  /** @param {string[]} argv @param {NodeJS.ProcessEnv} [env] */
  const publish = async (argv, env = {}) => {
    /** @type {string[]} */ const out = [];
    /** @type {string[]} */ const err = [];
    const code = await publishMain(argv, {
      env,
      out: (line) => out.push(line),
      err: (line) => err.push(line),
    });
    return { code, out: out.join('\n'), err: err.join('\n') };
  };
  const recorded = () =>
    readFile(record, 'utf8').then(
      (text) => text,
      () => null
    );
  const base = ['--dir', out1, '--tag', 'runner-pi400-test', '--gh', fakeGh];

  await check('publish defaults to a dry run that prints the command and sha256s', async () => {
    const result = await publish(base);
    assert.equal(result.code, 0, result.err);
    assert.match(result.out, /Dry run: nothing was created/);
    assert.ok(result.out.includes(`${sha256(archive)}  pi400-test.tar.gz`));
    assert.ok(
      result.out.includes(`${sha256(Buffer.from(manifestText))}  rnqnx-runner-manifest.json`)
    );
    assert.match(
      result.out,
      /Command: gh release create runner-pi400-test --repo mikevocalz\/react-native-qnx --draft /
    );
    assert.ok(result.out.includes(path.join(out1, 'pi400-test.tar.gz')));
    assert.equal(await recorded(), null, 'gh was not run');
  });

  await check('a dry run ignores the approval variable', async () => {
    const result = await publish(base, { RNQNX_RELEASE_OWNER_APPROVED: 'runner-pi400-test' });
    assert.equal(result.code, 0);
    assert.equal(await recorded(), null, 'gh was not run');
  });

  await check('--confirm without the approval variable is refused', async () => {
    const result = await publish([...base, '--confirm']);
    assert.equal(result.code, 1);
    assert.match(result.err, /RNQNX_RELEASE_OWNER_APPROVED must be set to runner-pi400-test/);
    assert.equal(await recorded(), null, 'gh was not run');
  });

  await check('--confirm with the wrong tag in the approval variable is refused', async () => {
    for (const value of ['other-tag', 'true', '1', '']) {
      const result = await publish([...base, '--confirm'], { RNQNX_RELEASE_OWNER_APPROVED: value });
      assert.equal(result.code, 1, `value ${JSON.stringify(value)}`);
      assert.equal(await recorded(), null, 'gh was not run');
    }
  });

  await check('a tampered archive is refused even with approval', async () => {
    const copy = path.join(temporary, 'tampered');
    await mkdir(copy);
    for (const name of await readdir(out1))
      await writeFile(path.join(copy, name), await readFile(path.join(out1, name)));
    await writeFile(
      path.join(copy, 'pi400-test.tar.gz'),
      Buffer.concat([archive, Buffer.from([0])])
    );
    const result = await publish(
      ['--dir', copy, '--tag', 'runner-pi400-test', '--gh', fakeGh, '--confirm'],
      {
        RNQNX_RELEASE_OWNER_APPROVED: 'runner-pi400-test',
      }
    );
    assert.equal(result.code, 1);
    assert.match(result.err, /does not match its manifest entry/);
    assert.equal(await recorded(), null, 'gh was not run');
  });

  await check('missing directory, bad tag and missing flags are refused', async () => {
    assert.equal((await publish(['--dir', path.join(temporary, 'none'), '--tag', 'v1'])).code, 1);
    assert.match(
      (await publish([...base.slice(0, 2), '--tag', 'latest'])).err,
      /not a usable release tag/
    );
    assert.match(
      (await publish([...base.slice(0, 2), '--tag=-rf'])).err,
      /not a usable release tag/
    );
    assert.equal((await publish(['--tag', 'v1'])).code, 2);
  });

  await check('with --confirm and the exact tag approved, gh gets the draft command', async () => {
    const result = await publish([...base, '--confirm'], {
      RNQNX_RELEASE_OWNER_APPROVED: 'runner-pi400-test',
    });
    assert.equal(result.code, 0, result.err);
    const args = (await recorded())?.trim().split('\n');
    assert.deepEqual(args?.slice(0, 6), [
      'release',
      'create',
      'runner-pi400-test',
      '--repo',
      'mikevocalz/react-native-qnx',
      '--draft',
    ]);
    assert.ok(args?.includes(path.join(out1, 'rnqnx-runner-manifest.json')));
  });

  console.log(`runner pack CLI and publish script: ${cases.length} cases passed`);
  for (const name of cases) console.log(`  ok - ${name}`);
} finally {
  await rm(temporary, { recursive: true, force: true });
}
