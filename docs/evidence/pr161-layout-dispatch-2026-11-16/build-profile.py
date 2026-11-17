import hashlib
import json
from pathlib import Path
import shlex
import subprocess

repo=Path('/workspace/scratch/252a22fac759/rnqnx-pr149')
build=repo/'build/linux-host-renderer'
out=Path('/workspace/scratch/252a22fac759/rnsvg-profile')
source=repo/'tests/native/qt-rnsvg-fabric/RnsvgCorpusFabricTest.cpp'
text=source.read_text()
text='#include "PcSampler.h"\n'+text
text=text.replace('  QGuiApplication application(argc, argv);',
                  '  QGuiApplication application(argc, argv);\n  rnqnx_temporary_profile::Sampler sampler;')
text=text.replace('  timer.start();\n  do {\n    host.runtime().drainMicrotasks();\n    host.pumpEvents();\n    if (ready()) return;',
                  '''  timer.start();
  qint64 drainNs = 0, pumpNs = 0;
  int loops = 0;
  do {
    QElapsedTimer phase;
    phase.start();
    host.runtime().drainMicrotasks();
    drainNs += phase.nsecsElapsed();
    phase.restart();
    host.pumpEvents();
    pumpNs += phase.nsecsElapsed();
    ++loops;
    if (ready()) {
      std::cerr << "RNQNX_TEMP_AWAIT " << what << " loops=" << loops
                << " drain_ms=" << drainNs / 1000000.0 << " pump_ms=" << pumpNs / 1000000.0 << "\\n";
      return;
    }''')
text=text.replace('      // Every cell mounts one <Svg>;',
                  '      std::cerr << "RNQNX_TEMP_EVALUATE batch=" << b << " ms=" << batchTimer.elapsed() << "\\n";\n      // Every cell mounts one <Svg>;')
assert 'RNQNX_TEMP_AWAIT' in text and 'RNQNX_TEMP_EVALUATE' in text
profile_source=out/'RnsvgCorpusFabricTest-profile.cpp';profile_source.write_text(text)
commands=subprocess.run(['ninja','-t','commands','rnqnx_qt_rnsvg_corpus_test'],cwd=build,capture_output=True,text=True,check=True).stdout.splitlines()
compile_line=next(x for x in commands if str(source) in x and ' -c ' in x)
compile=shlex.split(compile_line)
original_object=compile[compile.index('-o')+1]
profile_object=out/'RnsvgCorpusFabricTest-profile.cpp.o'
compile[compile.index('-o')+1]=str(profile_object)
compile[compile.index('-c')+1]=str(profile_source)
compile.extend(['-I'+str(source.parent),'-I'+str(out)])
if '-MF' in compile: compile[compile.index('-MF')+1]=str(out/'profile.d')
if '-MT' in compile: compile[compile.index('-MT')+1]=str(profile_object)
link=shlex.split(commands[-1]);assert link[:2]==[':','&&'] and link[-2:]==['&&',':'];link=link[2:-2]
assert link.count(original_object)==1
link[link.index(original_object)]=str(profile_object)
link[link.index('-o')+1]=str(out/'rnsvg-corpus-profile')
link=[x if not x.startswith('--dependency-file=') else '--dependency-file='+str(out/'link.d') for x in link]
link.append('-ldl')
manifest={'base_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=repo,text=True).strip(),'original_test_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'profile_test_sha256':hashlib.sha256(profile_source.read_bytes()).hexdigest(),'working_directory':str(build),'compile':compile,'link':link}
(out/'commands.json').write_text(json.dumps(manifest,indent=2)+'\n')
with (out/'build.log').open('w') as log:
 subprocess.run(compile,cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
 subprocess.run(link,cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
print('Separate profiling executable built; original sources, objects and binaries untouched.')
