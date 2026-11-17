import datetime
import hashlib
import json
import os
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


paths = subprocess.check_output(
    ['git', '-C', str(primary), 'diff', '--name-only', 'HEAD']
).decode().splitlines()
assert len(paths) == 23, paths
before = {name: digest(primary / name) for name in paths}
assert {name: digest(trial / name) for name in paths} == before
assert before['vnext/src/fabric/QtRnsvgBridgeLayoutEvents.cpp'] == (
    '20b46b497bb18cade48899742dbc70794cb0c5aa94555b9b0ba97dbf802faab3'
)
save('final-candidate-source-before.json', {
    'recordedAt': now(), 'primary': str(primary), 'testClone': str(trial),
    'sha256': before,
    'scope': 'All 23 reviewed changed source, generator and test files match byte-for-byte.',
})

plan = json.loads((out / 'transitive-bundle-refresh-plan.json').read_text())
assert len(plan['outputs']) == 9
bundle_before = []
for name in plan['outputs']:
    assert Path(name).name == name and name.endswith('.bundle.js')
    path = build / name
    stat = path.stat()
    bundle_before.append({
        'path': name, 'sha256': digest(path),
        'atimeNs': stat.st_atime_ns, 'mtimeNs': stat.st_mtime_ns,
    })
save('transitive-bundle-refresh-before.json', {
    'recordedAt': now(),
    'method': 'Preserve output bytes and record timestamps, then set only these nine application bundle mtimes to one second after the epoch so ordinary Ninja dependencies rebuild them.',
    'outputs': bundle_before,
})
for item in bundle_before:
    os.utime(build / item['path'], ns=(item['atimeNs'], 1_000_000_000))

command = ['cmake', '--build', '--preset', 'linux-host-renderer', '--parallel', '3']
start = now()
tick = time.monotonic()
print(json.dumps({'phase': 'build', 'startedAt': start, 'command': command}), flush=True)
with (out / 'final-candidate-build.log').open('w') as log:
    result = subprocess.run(command, cwd=trial, stdout=log, stderr=subprocess.STDOUT)
build_record = {
    'command': command, 'cwd': str(trial), 'startedAt': start,
    'finishedAt': now(), 'elapsedSeconds': time.monotonic() - tick,
    'exitCode': result.returncode,
}
save('final-candidate-build.json', build_record)
print(json.dumps(build_record), flush=True)
if result.returncode:
    print('\n'.join((out / 'final-candidate-build.log').read_text().splitlines()[-35:]), flush=True)
    sys.exit(result.returncode)

bundle_after = [{
    'path': item['path'], 'sha256': digest(build / item['path']),
    'mtimeNs': (build / item['path']).stat().st_mtime_ns,
} for item in bundle_before]
assert all(item['mtimeNs'] > 1_000_000_000 for item in bundle_after)
save('transitive-bundle-refresh-after.json', {'recordedAt': now(), 'outputs': bundle_after})

inventory_text = subprocess.check_output(
    ['ctest', '--preset', 'linux-host-renderer', '--show-only=json-v1'], cwd=trial
).decode()
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
save('final-candidate-configuration.json', {
    'recordedAt': now(), 'testCount': 49,
    'corpusTimeouts': {test['name']: 300 for test in corpus}, 'cache': selected,
})

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
record = {
    'command': command, 'cwd': str(trial), 'startedAt': start,
    'finishedAt': now(), 'elapsedSeconds': time.monotonic() - tick,
    'exitCode': result.returncode, 'testCount': 49,
    'sourceUnchangedDuringRun': before == after == primary_after,
    'sourceSha256': after,
    'scope': 'One complete exact-PR-161 candidate host graph, with all four original corpus shards, unchanged limits and Hermes slow debug enabled. No current QNX/device qualification is inferred.',
}
save('final-candidate-full.json', record)
print(json.dumps({key: value for key, value in record.items() if key != 'sourceSha256'}), flush=True)
print('\n'.join((out / 'final-candidate-full.log').read_text().splitlines()[-22:]), flush=True)
assert record['sourceUnchangedDuringRun']
sys.exit(result.returncode)
