import json
from pathlib import Path
import shlex
import subprocess

build = Path('/workspace/scratch/252a22fac759/rnqnx-pr149/build/linux-host-renderer')
out = Path('/workspace/scratch/252a22fac759/rnsvg-profile/listener-candidate')
commands = subprocess.check_output(
    ['ninja', '-t', 'commands', 'rnqnx_qt_rnsvg_corpus_test'], cwd=build, text=True
).splitlines()
link = shlex.split(commands[-1])
assert link[:2] == [':', '&&'] and link[-2:] == ['&&', ':']
link = link[2:-2]
link[link.index('-o') + 1] = str(out / 'corpus-candidate')
link[link.index('librnqnx_fabric_adapter.a')] = str(out / 'librnqnx_fabric_adapter.a')
link = [
    '--dependency-file=' + str(out / 'corpus-candidate.link.d')
    if arg.startswith('--dependency-file=') else arg for arg in link
]
(out / 'corpus-candidate-link.json').write_text(json.dumps(link, indent=2) + '\n')
with (out / 'corpus-candidate-build.log').open('w') as log:
    subprocess.run(link, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)

lines = [
    '# Isolated validation: original corpus object; copied guarded adapter archive.',
    'add_test(host_listener_candidate "' + str(out / 'host-tests-candidate') + '" "-platform" "offscreen")',
    'set_tests_properties(host_listener_candidate PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")',
]
for shard in range(4):
    name = 'corpus_listener_candidate_' + str(shard)
    lines += [
        'add_test(' + name + ' "' + str(out / 'corpus-candidate')
        + '" "-platform" "offscreen" "--shard-index" "' + str(shard)
        + '" "--shard-count" "4")',
        'set_tests_properties(' + name
        + ' PROPERTIES ENVIRONMENT "QT_QUICK_BACKEND=software" TIMEOUT "300")',
    ]
(out / 'CTestTestfile.cmake').write_text('\n'.join(lines) + '\n')
print('Original corpus object linked with candidate adapter; isolated CTest retains all four 300-second shard limits.')
