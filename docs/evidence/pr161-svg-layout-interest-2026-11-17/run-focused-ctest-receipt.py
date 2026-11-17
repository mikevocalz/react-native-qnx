import datetime
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

repo = Path('/workspace/scratch/252a22fac759/rnqnx-pr161-baseline')
build = repo / 'build/linux-host-renderer'
out = repo.parent / 'pr161-svg-layout-controls'
prefix, selection, scope = sys.argv[1:4]
command = ['ctest', '--preset', 'linux-host-renderer', '-R', selection,
           '--output-on-failure', '--output-junit', str(out / (prefix + '.xml'))]
start = datetime.datetime.now(datetime.timezone.utc).isoformat()
print(json.dumps({'startedAt': start, 'command': command}), flush=True)
with (out / (prefix + '.log')).open('w') as log:
    result = subprocess.run(command, cwd=repo, stdout=log, stderr=subprocess.STDOUT)
shutil.copyfile(build / 'Testing/Temporary/LastTest.log', out / (prefix + '-last-test.log'))
paths = ['vnext/src/fabric/QtRnsvgBridgeLayoutEvents.cpp',
         'tests/native/qt-rnsvg-fabric/RnsvgFabricTest.cpp',
         'apps/rn-playground/rnsvg-fixture.js',
         'packages/react-native-qnx/Libraries/NativeComponent/BaseViewConfig.js',
         'build/linux-host-renderer/rnqnx-rnsvg.bundle.js',
         'build/linux-host-renderer/CMakeFiles/rnqnx_qt_rnsvg_fabric_test.dir/tests/native/qt-rnsvg-fabric/RnsvgFabricTest.cpp.o',
         'build/linux-host-renderer/CMakeFiles/rnqnx_fabric_adapter.dir/vnext/src/fabric/QtRnsvgBridgeLayoutEvents.cpp.o']
record = {'command': command, 'cwd': str(repo), 'startedAt': start,
          'finishedAt': datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'exitCode': result.returncode, 'scope': scope,
          'sha256': {p: hashlib.sha256((repo / p).read_bytes()).hexdigest() for p in paths}}
(out / (prefix + '.json')).write_text(json.dumps(record, indent=2) + '\n')
print(json.dumps(record))
sys.exit(result.returncode)
