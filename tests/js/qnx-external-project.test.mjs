import assert from 'node:assert/strict';
import { mkdir, mkdtemp, readFile, readdir, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { planProject, projectMain } from '../../packages/react-native-qnx-cli/src/project.mjs';

const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-external-'));
const project = path.join(temporary, 'an app');
let checks = 0;
/** @param {unknown} actual @param {unknown} expected */
function equal(actual, expected) { assert.deepEqual(actual, expected); checks += 1; }
try {
  await mkdir(path.join(project, 'node_modules/metro'), { recursive: true });
  for (const name of ['index.js', 'metro.config.js', 'metro.marathon.config.js']) {
    await writeFile(path.join(project, name), 'throw new Error("Planning must not evaluate app/config");\n');
  }
  await writeFile(path.join(project, 'package.json'), JSON.stringify({ name: 'external-app' }));
  await writeFile(path.join(project, 'node_modules/metro/package.json'), JSON.stringify({
    name: 'metro', version: 'fixture-only', bin: { metro: 'cli.cjs' },
  }));
  const metroFile = path.join(project, 'node_modules/metro/cli.cjs');
  await writeFile(metroFile, `const fs = require('node:fs');
const args = process.argv.slice(2);
fs.writeFileSync(args[args.indexOf('--out') + 1], JSON.stringify(args));
`);
  const args = ['bundle', '--project', project];
  const plan = await planProject(args, temporary);
  assert.ok(plan);
  equal(plan.platform, 'qnx');
  equal(plan.projectName, 'external-app');
  equal(plan.nativeBuildVerified, false);
  equal(plan.runtimeVerified, false);
  equal(plan.metroVersion, 'fixture-only');
  equal(plan.output, path.join(project, 'build/qnx/app.bundle.js'));
  equal(await projectMain([...args, '--dry-run']), 0);
  await assert.rejects(readFile(path.join(project, 'build/qnx/app.bundle.js')), /ENOENT/);
  checks += 1;
  equal(await projectMain(args), 0);
  const captured = JSON.parse(await readFile(plan.output, 'utf8'));
  equal(captured[captured.indexOf('--platform') + 1], 'qnx');
  equal(captured[1], path.join(project, 'index.js'));
  equal((await readdir(path.dirname(plan.output))).sort(), ['app.bundle.js']);
  await assert.rejects(projectMain(args), /already exists/);
  checks += 1;
  const marathonArgs = [...args, '--platform', 'marathon'];
  equal(await projectMain(marathonArgs), 0);
  const marathon = JSON.parse(await readFile(path.join(project, 'build/marathon/app.bundle.js'), 'utf8'));
  equal(marathon[marathon.indexOf('--platform') + 1], 'marathon');
  equal(marathon[marathon.indexOf('--config') + 1], path.join(project, 'metro.marathon.config.js'));

  for (const extra of [
    ['--platform', 'android'], ['--platform', 'qnx', '--platform', 'marathon'],
    ['--out', '../escape.js'], ['--out', 'index.js'], ['--out', 'build/../index.js'],
    ['--entry', '../index.js'], ['--out', 'build//bad.js'], ['--entry', ''],
    ['--project', ''], ['--unknown'], ['extra'],
  ]) {
    await assert.rejects(planProject([...args, ...extra])); checks += 1;
  }
  await assert.rejects(planProject(['inspect', '--project', project, '--out', 'build/x.js']));
  checks += 1;
  await symlink(temporary, path.join(project, 'build/escape'));
  await assert.rejects(planProject([...args, '--out', 'build/escape/test.js']), /symlink/);
  checks += 1;
  await symlink(path.join(project, 'index.js'), path.join(project, 'alias.js'));
  await assert.rejects(planProject([...args, '--entry', 'alias.js', '--out', 'build/x.js']), /symlink/);
  checks += 1;

  await writeFile(metroFile, `require('node:fs').writeFileSync(process.argv.at(-1), 'partial'); process.exit(23);`);
  equal(await projectMain([...args, '--out', 'build/fail.js']), 23);
  await assert.rejects(readFile(path.join(project, 'build/fail.js')), /ENOENT/); checks += 1;
  equal((await readdir(path.join(project, 'build'))).filter((name) => name.startsWith('.rnqnx-')), []);
  await writeFile(metroFile, 'process.exit(0);');
  await assert.rejects(projectMain([...args, '--out', 'build/missing.js']), /ENOENT/); checks += 1;
  await writeFile(metroFile, `require('node:fs').writeFileSync(process.argv.at(-1), '');`);
  await assert.rejects(projectMain([...args, '--out', 'build/empty.js']), /nonempty/); checks += 1;

  // The bundle process must not overwrite a destination created after planning.
  await writeFile(metroFile, `const fs = require('node:fs');
fs.writeFileSync('build/race.js', 'preserve');
fs.writeFileSync(process.argv.at(-1), 'bundle');`);
  await assert.rejects(projectMain([...args, '--out', 'build/race.js']), /EEXIST/); checks += 1;
  equal(await readFile(path.join(project, 'build/race.js'), 'utf8'), 'preserve');
  equal(await readFile(path.join(project, 'index.js'), 'utf8'), 'throw new Error("Planning must not evaluate app/config");\n');
  console.log(`qnx-external-project: ${checks} checks passed (synthetic Metro process, no native run).`);
} finally {
  await rm(temporary, { recursive: true, force: true });
}
