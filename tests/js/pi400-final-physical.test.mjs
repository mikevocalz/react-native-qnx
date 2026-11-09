import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  chmod,
  copyFile,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  rm,
  writeFile,
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { setTimeout as delay } from 'node:timers/promises';
import { commandRun, main, targetConnection } from '../../scripts/final-pi400-test.mjs';

const file = path.resolve('scripts/final-pi400-test.mjs');
const plan = spawnSync(process.execPath, [file, 'plan'], {
  encoding: 'utf8',
  timeout: 10000,
  maxBuffer: 1024 * 1024,
});
assert.equal(plan.status, 0, plan.stderr);
const report = JSON.parse(plan.stdout);
assert.equal(report.issue, 43);
assert.equal(report.mode, 'plan');
assert.equal(report.board, 'pi400');
assert.equal(report.stages.length, 6);
assert.equal(report.stages[0].name, 'non-root-target');
assert.ok(
  report.stages.every((/** @type {{status: string}} */ stage) => stage.status === 'not-run')
);
assert.equal(report.physicalVerified, false);
assert.equal(report.stockQstiQualified, false);
assert.equal(report.releaseAuthorized, false);

const help = spawnSync(process.execPath, [file, '--help'], {
  encoding: 'utf8',
  timeout: 10000,
});
assert.equal(help.status, 0, help.stderr);
assert.match(help.stdout, /RNQNX_FINAL_TEST_CONFIRM=pi400/);

const preserved = {
  confirm: process.env.RNQNX_FINAL_TEST_CONFIRM,
  board: process.env.RNQNX_PI_BOARD,
};
try {
  delete process.env.RNQNX_FINAL_TEST_CONFIRM;
  delete process.env.RNQNX_PI_BOARD;
  await assert.rejects(
    main(['execute', '--consumer-project', '/nonexistent']),
    /confirmation|board/
  );
} finally {
  if (preserved.confirm === undefined) delete process.env.RNQNX_FINAL_TEST_CONFIRM;
  else process.env.RNQNX_FINAL_TEST_CONFIRM = preserved.confirm;
  if (preserved.board === undefined) delete process.env.RNQNX_PI_BOARD;
  else process.env.RNQNX_PI_BOARD = preserved.board;
}

await assert.rejects(main(['execute']), /confirmation|board/);
await assert.rejects(
  main(['plan', '--consumer-project', '/tmp/something']),
  /Plan does not accept/
);
await assert.rejects(
  main(['plan', '--consumer-project', 'x', '--consumer-project', 'y']),
  /Duplicate option/
);
await assert.rejects(main(['other']), /Choose plan or execute/);
await assert.rejects(main(['plan', '--unexpected']), /Unknown option/);

for (const QNX_TARGET_USER of ['root', '0', '-admin', '-Fconfig']) {
  await assert.rejects(targetConnection({ QNX_TARGET_USER }), /non-root|unsupported/);
}
for (const QNX_TARGET_IP of ['-host', '-Fconfig', 'host_name']) {
  await assert.rejects(targetConnection({ QNX_TARGET_IP }), /hostname|unsupported/);
}
await assert.rejects(
  targetConnection({ QNX_SSH_OPTS: '-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null' }),
  /QNX_SSH_OPTS is not allowed/
);
await assert.rejects(targetConnection({ QNX_SSH_PORT: '65536' }), /port/);
await assert.rejects(targetConnection({ QNX_TARGET_DIR: '/data/../root' }), /confined/);
for (const RNQNX_PI_QT_FONTDIR of ['', 'fonts', '/data/../fonts', "/data/fonts';false"]) {
  await assert.rejects(targetConnection({ RNQNX_PI_QT_FONTDIR }), /font directory.*confined/);
}
assert.equal(
  (await targetConnection({ RNQNX_PI_QT_FONTDIR: '/usr/share/fonts/dejavu' })).fontDirectory,
  '/usr/share/fonts/dejavu'
);
assert.equal((await targetConnection({})).fontDirectory, undefined);

const fixture = await mkdtemp(path.join(tmpdir(), 'rnqnx-pi400-safety-'));
try {
  const preflight = await readFile('scripts/pi-preflight.sh', 'utf8');
  const remotePreflight = preflight.match(/<<'RNQNX_REMOTE'\n([\s\S]*?)\nRNQNX_REMOTE/)?.[1];
  assert.ok(remotePreflight);
  for (const processor of ['aarch64le', 'x86_64']) {
    /** @type {import('node:child_process').SpawnSyncReturns<string>} */
    const checked = spawnSync(
      '/bin/bash',
      [
        '-c',
        `
      uname() {
        case "$1" in
          -p) printf '%s\\n' "$PROCESSOR" ;;
          -m) echo RaspberryPi400 ;;
          *) echo fixture-qnx ;;
        esac
      }
      [() {
        if builtin test "$1" = -d && builtin test "$2" = /dev/screen; then return 0; fi
        if builtin test "$1" = '!' && builtin test "$2" = -d && builtin test "$3" = /dev/screen; then return 1; fi
        builtin [ "$@"
      }
      ${remotePreflight}
    `,
        '--',
        path.join(fixture, 'preflight-target'),
      ],
      { encoding: 'utf8', env: { ...process.env, PROCESSOR: processor } }
    );
    assert.equal(checked.status, processor === 'aarch64le' ? 0 : 21, checked.stderr);
    assert.match(checked.stdout, /board=RaspberryPi400/);
    assert.match(checked.stdout, new RegExp('machine=' + processor));
  }
  assert.equal(
    await commandRun([process.execPath, '-e', 'process.stdout.write("ok")'], 5000),
    'ok'
  );
  await assert.rejects(commandRun(['/nonexistent/rnqnx-command'], 1000), /ENOENT/);
  await assert.rejects(commandRun([process.execPath], 0), /positive timeout/);
  await assert.rejects(commandRun([process.execPath, '-e', 'process.exit(3)'], 1000), /failed 3/);
  for (const overflow of [false, true]) {
    const start = performance.now();
    await assert.rejects(
      commandRun(
        [
          process.execPath,
          '-e',
          `
        process.on('SIGTERM', () => {});
        if (${overflow}) process.stdout.write(Buffer.alloc(9 * 1024 * 1024));
        setTimeout(() => process.exit(0), 5000);
      `,
        ],
        overflow ? 5000 : 300
      ),
      overflow ? /overflowed output/ : /timed out/
    );
    assert.ok(performance.now() - start < 2500, 'ignored SIGTERM must not defeat the deadline');
  }
  const descendant = path.join(fixture, 'descendant.mjs');
  const heartbeat = path.join(fixture, 'heartbeat');
  await writeFile(
    descendant,
    `
    import { writeFileSync } from 'node:fs';
    process.on('SIGTERM', () => {});
    setInterval(() => writeFileSync(process.argv[2], String(Date.now())), 20);
    setTimeout(() => process.exit(0), 5000);
  `
  );
  const start = performance.now();
  await assert.rejects(
    commandRun(
      [
        process.execPath,
        '-e',
        `
    require('node:child_process').spawn(process.execPath, ${JSON.stringify([descendant, heartbeat])}, { stdio: 'inherit' });
    process.exit(0);
  `,
      ],
      500
    ),
    /timed out/
  );
  assert.ok(
    performance.now() - start < 2500,
    'descendant-held pipes must not hold the promise open'
  );
  await delay(100);
  const stopped = await readFile(heartbeat, 'utf8');
  await delay(100);
  assert.equal(await readFile(heartbeat, 'utf8'), stopped, 'the descendant must stop working');

  for (const mode of [
    'success',
    'font-success',
    'font-directory-missing',
    'font-directory-empty',
    'root-alias',
    'wrong-board',
    'missing-marker',
    'invalid-png',
    'missing-hermes',
  ]) {
    const sandbox = path.join(fixture, mode);
    for (const dir of [
      'scripts/lib',
      'packages/react-native-qnx-cli/src',
      'bin',
      'app/build/qnx/native/consumer/qnx/org.example.test',
      'build/hermes-qnx-aarch64/lib',
    ]) {
      await mkdir(path.join(sandbox, dir), { recursive: true });
    }
    for (const relative of [
      'scripts/final-pi400-test.mjs',
      'scripts/lib/qnx-target.sh',
      'packages/react-native-qnx-cli/src/build-native.mjs',
      'packages/react-native-qnx-cli/src/prepare-native.mjs',
    ]) {
      await copyFile(path.resolve(relative), path.join(sandbox, relative));
    }
    const app = path.join(sandbox, 'app');
    const fontDirectory = path.join(sandbox, 'fonts');
    if (mode === 'font-success' || mode === 'font-directory-empty') {
      await mkdir(fontDirectory);
      if (mode === 'font-success')
        await writeFile(path.join(fontDirectory, 'fixture.ttf'), 'inert font-access fixture');
    }
    const bundle = path.join(app, 'build/qnx/release.hbc');
    const bytes = Buffer.from('inert HBC fixture; not executable QNX bytecode');
    await writeFile(bundle, bytes);
    await writeFile(
      path.join(app, 'app.json'),
      JSON.stringify({
        expo: {
          name: 'Test',
          extra: { rnqnx: { appId: 'org.example.test', componentName: 'main' } },
        },
      })
    );
    await writeFile(
      path.join(app, 'build/qnx/native-app.json'),
      JSON.stringify({
        schemaVersion: 1,
        generatedBy: 'react-native-qnx/prepare-native-v1',
        appId: 'org.example.test',
        displayName: 'Test',
        componentName: 'main',
        platform: 'qnx',
        bundle,
        bundleSha256: createHash('sha256').update(bytes).digest('hex'),
      })
    );
    await writeFile(
      path.join(app, 'build/qnx/native/consumer/qnx/org.example.test/org.example.test'),
      'inert native executable fixture'
    );
    if (mode !== 'missing-hermes')
      await writeFile(
        path.join(sandbox, 'build/hermes-qnx-aarch64/lib/libhermesvmlean.so'),
        'inert matching Hermes fixture'
      );
    const key = path.join(sandbox, 'fixture private key');
    await writeFile(key, 'not a real credential');
    const trace = path.join(sandbox, 'calls.jsonl');
    for (const tool of ['bash', 'ssh', 'scp', 'node', 'git', 'id', 'uname']) {
      const stub = path.join(sandbox, 'bin', tool);
      await writeFile(
        stub,
        `#!${process.execPath}
        const fs = require('node:fs');
        const cp = require('node:child_process');
        const path = require('node:path');
        const tool = path.basename(process.argv[1]);
        const args = process.argv.slice(2);
        fs.appendFileSync(${JSON.stringify(trace)}, JSON.stringify({ tool, args }) + '\\n');
        if (tool === 'git') console.log('inert-fixture-commit');
        if (tool === 'id') console.log(${mode === 'root-alias' ? '0' : '1000'});
        if (tool === 'uname') console.log(args[0] === '-s' ? 'QNX' : args[0] === '-p' ? 'aarch64le' : ${JSON.stringify(mode === 'wrong-board' ? 'NotPi400' : 'RaspberryPi400')});
        if (tool === 'bash') {
          const run = cp.spawnSync('/bin/bash', ['-c', 'source "$1"; rnqnx_target_init; rnqnx_ssh true; rnqnx_scp_to "$2" /fixture', '--', ${JSON.stringify(path.join(sandbox, 'scripts/lib/qnx-target.sh'))}, ${JSON.stringify(bundle)}], { stdio: 'inherit' });
          process.exit(run.status ?? 1);
        }
        if (tool === 'ssh') {
          const remote = args.at(-1);
          if (remote.startsWith('uid=')) {
            const run = cp.spawnSync('/bin/sh', ['-c', remote], { stdio: 'inherit' });
            process.exit(run.status ?? 1);
          }
          if (remote.includes('--screenshot')) {
            if (!fs.existsSync(${JSON.stringify(path.join(sandbox, 'hermes-staged'))})) process.exit(87);
            if (${JSON.stringify(mode)} !== 'missing-marker') console.log('RNQNX_PLAYGROUND_SCREENSHOT final.png 1x1');
          }
        }
        if (tool === 'scp') {
          if (args.at(-1).endsWith('/libhermesvmlean.so')) {
            if (fs.readFileSync(args.at(-2), 'utf8') !== 'inert matching Hermes fixture') process.exit(88);
            fs.writeFileSync(${JSON.stringify(path.join(sandbox, 'hermes-staged'))}, 'staged');
          }
          if (args.at(-2).endsWith('/final.png')) {
            const png = Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aFo0AAAAASUVORK5CYII=', 'base64');
            fs.writeFileSync(args.at(-1), ${JSON.stringify(mode)} === 'invalid-png' ? 'not PNG' : png);
          }
        }
      `
      );
      await chmod(stub, 0o700);
    }
    const executed = spawnSync(
      process.execPath,
      [path.join(sandbox, 'scripts/final-pi400-test.mjs'), 'execute', '--consumer-project', app],
      {
        env: {
          ...process.env,
          PATH: path.join(sandbox, 'bin') + path.delimiter + process.env.PATH,
          RNQNX_FINAL_TEST_CONFIRM: 'pi400',
          RNQNX_PI_BOARD: 'pi400',
          QNX_HOST: sandbox,
          QNX_TARGET: sandbox,
          QNX_TARGET_USER: 'operator',
          QNX_TARGET_IP: 'pi400.invalid',
          QNX_TARGET_DIR: '/data/home/operator/rnqnx',
          QNX_SSH_KEY: key,
          QNX_SSH_PORT: '2222',
          QNX_SSH_OPTS: '',
          QNX_SSH_BATCH_MODE: 'no',
          QNX_SSH_CONNECT_TIMEOUT: '9999',
          QNX_SSH_MACS: 'untrusted-macs',
          RNQNX_PI_QT_FONTDIR: mode.startsWith('font-') ? fontDirectory : undefined,
        },
        encoding: 'utf8',
        timeout: 15000,
      }
    );
    const directories = await readdir(path.join(sandbox, 'artifacts/pi-final'));
    assert.equal(directories.length, 1);
    const receipt = JSON.parse(
      await readFile(
        path.join(sandbox, 'artifacts/pi-final', directories[0], 'result.json'),
        'utf8'
      )
    );
    const calls = (await readFile(trace, 'utf8'))
      .trim()
      .split('\n')
      .map((line) => JSON.parse(line));
    for (const call of calls.filter((call) => ['ssh', 'scp'].includes(call.tool))) {
      for (const option of [
        'BatchMode=yes',
        'StrictHostKeyChecking=yes',
        'ConnectTimeout=10',
        'MACs=hmac-sha2-256',
      ])
        assert.ok(call.args.includes(option), call.tool + ': ' + option);
      assert.ok(call.args.includes(key), 'retain configured identity, including spaces');
      assert.ok(call.args.includes(call.tool === 'ssh' ? '-p' : '-P'));
      assert.ok(call.args.includes('2222'));
      assert.ok(!call.args.includes('BatchMode=no'));
    }
    if (mode === 'success' || mode === 'font-success') {
      assert.equal(executed.status, 0, executed.stderr);
      assert.equal(receipt.physicalRuntimeCapture, true, 'only an inert simulation receipt');
      assert.equal(receipt.releaseAuthorized, false);
      assert.equal(receipt.stockQstiQualified, false);
      assert.equal(receipt.coldBootQualified, false);
      assert.ok(receipt.hermesRuntime.sha256);
      assert.ok(receipt.capture.sha256);
      assert.equal(receipt.qtFontDirectory, mode === 'font-success' ? fontDirectory : null);
      const render = calls
        .find((call) => call.tool === 'ssh' && call.args.at(-1).includes('--screenshot'))
        .args.at(-1);
      assert.ok(render.includes('LD_LIBRARY_PATH=. '));
      if (mode === 'font-success') {
        assert.ok(render.includes("QT_QPA_FONTDIR='" + fontDirectory + "' ./app"));
        assert.match(receipt.results[0].output, /RNQNX_QT_FONTDIR_VERIFIED/);
      } else assert.ok(!render.includes('QT_QPA_FONTDIR='));
      assert.ok(
        receipt.results.every(
          (/** @type {{status: string}} */ result) => result.status === 'passed'
        )
      );
      assert.deepEqual(
        calls.filter((call) => call.tool === 'bash').map((call) => call.args[0]),
        ['scripts/pi-preflight.sh', 'scripts/install-pi.sh', 'scripts/validate-runtime-qnx.sh']
      );
      assert.ok(
        calls.some((call) => call.tool === 'scp' && call.args.includes(bundle)),
        'upload the descriptor-selected release.hbc, not app.hbc'
      );
    } else {
      assert.notEqual(executed.status, 0, mode);
      const failedStage = {
        'root-alias': 'non-root-target',
        'font-directory-missing': 'non-root-target',
        'font-directory-empty': 'non-root-target',
        'wrong-board': 'non-root-target',
        'missing-marker': 'physical-consumer-render',
        'invalid-png': 'physical-capture-download',
        'missing-hermes': 'full-fabric-qt-consumer-crossbuild',
      }[mode];
      assert.equal(receipt.results.at(-1).name, failedStage);
      assert.equal(receipt.results.at(-1).status, 'failed');
      assert.notEqual(receipt.physicalRuntimeCapture, true);
      assert.equal(receipt.remoteCleanupVerified, false);
      if (mode === 'root-alias' || mode === 'wrong-board' || mode.startsWith('font-'))
        assert.ok(
          !calls.some((call) => call.tool === 'bash'),
          'no preflight or deployment with invalid target identity or font profile'
        );
      if (mode === 'missing-hermes')
        assert.ok(
          !calls.some((call) => call.tool === 'scp' && call.args.at(-1).includes('/final-')),
          'no isolated uploads without Hermes'
        );
    }
  }
} finally {
  await rm(fixture, { recursive: true, force: true });
}

console.log(
  'Pi400 final test: consent/identity, strict shared SSH, descriptor bundle, staged Hermes, capture receipts and bounded process-tree tests pass; inert host simulations only, physical hardware NOT run.'
);
