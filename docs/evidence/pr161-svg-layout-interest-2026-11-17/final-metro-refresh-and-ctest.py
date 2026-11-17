import datetime
import hashlib
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

root = Path('/workspace/scratch/252a22fac759')
primary = root / 'rnqnx-pr161'
trial = root / 'rnqnx-pr161-baseline'
build = trial / 'build/linux-host-renderer'
out = root / 'pr161-svg-layout-controls'


def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(name, value):
    (out / name).write_text(json.dumps(value, indent=2) + '\n')


before = json.loads((out / 'final-candidate-source-before.json').read_text())['sha256']
paths = list(before)
assert {name: digest(trial / name) for name in paths} == before
assert {name: digest(primary / name) for name in paths} == before
assert json.loads((out / 'final-candidate-build.json').read_text())['exitCode'] == 0

entries = {
    'rnqnx-qt-svg.bundle.js': 'qt-svg-fixture.js',
    'rnqnx-rnsvg-corpus.bundle.js': 'rnsvg-corpus.js',
    'rnqnx-rnsvg-image.bundle.js': str(trial / 'tests/native/qt-rnsvg-fabric/rnsvg-image-fixture.js'),
    'rnqnx-rnsvg.bundle.js': 'rnsvg-fixture.js',
    'rnqnx-text-input-probe.bundle.js': 'text-input-probe.js',
}
stale = [name for name in json.loads((out / 'transitive-bundle-refresh-plan.json').read_text())['outputs']
         if (build / name).stat().st_mtime_ns == 1_000_000_000]
assert set(stale) == set(entries)
save('transitive-bundle-refresh-initial-stale.json', {
    'recordedAt': now(), 'outputs': stale,
    'result': 'The native rebuild succeeded, but freshness verification stopped before any CTest. Ninja restat retained these five outputs. Exact existing Metro commands are used below to refresh them without source changes or deletion.',
})
refresh = []
for name, entry in entries.items():
    command = [str(trial / 'node_modules/.bin/metro'), 'build', entry,
               '--platform', 'qnx', '--dev', 'false', '--out', str(build / name)]
    record = {'command': command, 'cwd': str(trial / 'apps/rn-playground'), 'startedAt': now()}
    print(json.dumps({'phase': 'metro', 'output': name, 'startedAt': record['startedAt']}), flush=True)
    with (out / ('refresh-' + name + '.log')).open('w') as log:
        result = subprocess.run(command, cwd=trial / 'apps/rn-playground', stdout=log, stderr=subprocess.STDOUT)
    record.update({'finishedAt': now(), 'exitCode': result.returncode, 'sha256': digest(build / name),
                   'mtimeNs': (build / name).stat().st_mtime_ns})
    refresh.append(record)
    save('transitive-bundle-refresh-direct.json', refresh)
    if result.returncode:
        sys.exit(result.returncode)

plan = json.loads((out / 'transitive-bundle-refresh-plan.json').read_text())
bundles = [{'path': name, 'sha256': digest(build / name), 'mtimeNs': (build / name).stat().st_mtime_ns}
           for name in plan['outputs']]
assert all(item['mtimeNs'] > 1_000_000_000 for item in bundles)
save('transitive-bundle-refresh-after.json', {'recordedAt': now(), 'outputs': bundles})

inventory_text = subprocess.check_output(['ctest', '--preset', 'linux-host-renderer', '--show-only=json-v1'], cwd=trial).decode()
(out / 'final-candidate-ctest-inventory.json').write_text(inventory_text)
inventory = json.loads(inventory_text)
assert len(inventory['tests']) == 49
corpus = [test for test in inventory['tests'] if 'rnqnx_qt_rnsvg_corpus_test' in test['name']]
assert len(corpus) == 4
assert all(next(p['value'] for p in test['properties'] if p['name'] == 'TIMEOUT') == 300 for test in corpus)
cache = (build / 'CMakeCache.txt').read_text().splitlines()
selected = [line for line in cache if line.startswith(('CMAKE_BUILD_TYPE:', 'HERMES_SLOW_DEBUG:', 'RNQNX_ENABLE_', 'RNQNX_REQUIRE_FULL_HOST_RENDERER:'))]
assert 'CMAKE_BUILD_TYPE:STRING=Debug' in selected
assert 'HERMES_SLOW_DEBUG:BOOL=ON' in selected
save('final-candidate-configuration.json', {'recordedAt': now(), 'testCount': 49,
     'corpusTimeouts': {test['name']: 300 for test in corpus}, 'cache': selected})

command = ['ctest', '--preset', 'linux-host-renderer', '--output-on-failure',
           '--output-junit', str(out / 'final-candidate-full.xml')]
start = now()
tick = time.monotonic()
print(json.dumps({'phase': 'full-ctest', 'startedAt': start, 'command': command}), flush=True)
with (out / 'final-candidate-full.log').open('w') as log:
    result = subprocess.run(command, cwd=trial, stdout=log, stderr=subprocess.STDOUT)
shutil.copyfile(build / 'Testing/Temporary/LastTest.log', out / 'final-candidate-full-last-test.log')
after = {name: digest(trial / name) for name in paths}
primary_after = {name: digest(primary / name) for name in paths}
record = {'command': command, 'cwd': str(trial), 'startedAt': start, 'finishedAt': now(),
          'elapsedSeconds': time.monotonic() - tick, 'exitCode': result.returncode, 'testCount': 49,
          'sourceUnchangedDuringRun': before == after == primary_after, 'sourceSha256': after,
          'scope': 'One complete exact-PR-161 candidate host graph; all four original corpus shards, unchanged limits and Hermes slow debug enabled. No current QNX/device qualification is inferred.'}
save('final-candidate-full.json', record)
print(json.dumps({key: value for key, value in record.items() if key != 'sourceSha256'}), flush=True)
print('\n'.join((out / 'final-candidate-full.log').read_text().splitlines()[-22:]), flush=True)
assert record['sourceUnchangedDuringRun']
sys.exit(result.returncode)
