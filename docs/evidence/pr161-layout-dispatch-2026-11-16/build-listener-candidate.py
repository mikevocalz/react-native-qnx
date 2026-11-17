import difflib
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess

repo=Path('/workspace/scratch/252a22fac759/rnqnx-pr149')
candidate_repo=Path('/workspace/scratch/252a22fac759/rnqnx-pr161')
build=repo/'build/linux-host-renderer'
out=Path('/workspace/scratch/252a22fac759/rnsvg-profile/listener-candidate');out.mkdir(exist_ok=True)
source=repo/'vnext/src/fabric/ShadowViewMutationAdapter.cpp'
original=source.read_text()
before='''      if (auto viewEmitter =
              std::dynamic_pointer_cast<const facebook::react::ViewEventEmitter>(
                  view.eventEmitter)) {
        viewEmitter->onLayout(view.layoutMetrics);
      }'''
after='''      if (const auto* props = dynamic_cast<const facebook::react::ViewProps*>(view.props.get());
          props != nullptr && props->onLayout) {
        if (auto viewEmitter =
                std::dynamic_pointer_cast<const facebook::react::ViewEventEmitter>(
                    view.eventEmitter)) {
          viewEmitter->onLayout(view.layoutMetrics);
        }
      }'''
assert original.count(before)==2
patched=original.replace(before,after)
patched_source=out/'ShadowViewMutationAdapter.cpp';patched_source.write_text(patched)
(out/'experiment.patch').write_text(''.join(difflib.unified_diff(original.splitlines(keepends=True),patched.splitlines(keepends=True),fromfile='original/ShadowViewMutationAdapter.cpp',tofile='candidate/ShadowViewMutationAdapter.cpp')))
commands=subprocess.run(['ninja','-t','commands','rnqnx_qt_rnsvg_corpus_test'],cwd=build,capture_output=True,text=True,check=True).stdout.splitlines()
def compile_copy(original_path,new_source,output):
 line=next(x for x in commands if str(original_path) in x and ' -c ' in x)
 cmd=shlex.split(line);cmd[cmd.index('-o')+1]=str(output);cmd[cmd.index('-c')+1]=str(new_source)
 cmd.append('-I'+str(original_path.parent))
 if '-MF' in cmd:cmd[cmd.index('-MF')+1]=str(output)+'.d'
 if '-MT' in cmd:cmd[cmd.index('-MT')+1]=str(output)
 return cmd
obj=out/'ShadowViewMutationAdapter.cpp.o'
compile=compile_copy(source,patched_source,obj)
archive=out/'librnqnx_fabric_adapter.a'
shutil.copyfile(build/'librnqnx_fabric_adapter.a',archive)
members=subprocess.check_output(['ar','t',str(archive)],text=True).splitlines();assert members.count(obj.name)==1
profile=json.loads((out.parent/'commands.json').read_text())
link=profile['link'];assert link.count('librnqnx_fabric_adapter.a')==1
link[link.index('librnqnx_fabric_adapter.a')]=str(archive)
link[link.index('-o')+1]=str(out/'rnsvg-corpus-listener-candidate')
link=[x if not x.startswith('--dependency-file=') else '--dependency-file='+str(out/'link.d') for x in link]
with (out/'build.log').open('w') as log:
 subprocess.run(compile,cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
 subprocess.run(['ar','rcs',str(archive),str(obj)],cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
 subprocess.run(link,cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
(out/'commands.json').write_text(json.dumps({'original_adapter_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'candidate_adapter_sha256':hashlib.sha256(patched_source.read_bytes()).hexdigest(),'compile':compile,'archive':str(archive),'link':link},indent=2)+'\n')
print('Separate listener-guard corpus executable built; original graph unchanged.')

test_source=candidate_repo/'tests/cpp/QtReactHostHostTest.cpp'
commands=subprocess.run(['ninja','-t','commands','qt_react_host_host_test'],cwd=build,capture_output=True,text=True,check=True).stdout.splitlines()
test_obj=out/'QtReactHostHostTest.cpp.o'
test_compile=compile_copy(repo/'tests/cpp/QtReactHostHostTest.cpp',test_source,test_obj)
test_link=shlex.split(commands[-1]);assert test_link[:2]==[':','&&'] and test_link[-2:]==['&&',':'];test_link=test_link[2:-2]
original_test_obj=next(x for x in test_link if x.endswith('/QtReactHostHostTest.cpp.o'))
test_link[test_link.index(original_test_obj)]=str(test_obj)
with (out/'test-build.log').open('w') as log:
 subprocess.run(test_compile,cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
 for label,use_candidate in [('baseline',False),('candidate',True)]:
  command=test_link.copy();command[command.index('-o')+1]=str(out/('host-tests-'+label))
  command=[x if not x.startswith('--dependency-file=') else '--dependency-file='+str(out/(label+'.link.d')) for x in command]
  if use_candidate:command[command.index('librnqnx_fabric_adapter.a')]=str(archive)
  subprocess.run(command,cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
  (out/(label+'-test-link.json')).write_text(json.dumps(command,indent=2)+'\n')
(out/'test-compile.json').write_text(json.dumps(test_compile,indent=2)+'\n')
print('Unchanged regression test object linked against both original and candidate adapters.')
