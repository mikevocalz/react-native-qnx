import json
import os
import pathlib
import re
import shutil
import signal
import subprocess
import tempfile
import time

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / 'scripts/pi400-desktop-startup.sh'
SOURCE = SCRIPT.read_text()
subprocess.run(['sh', '-n', str(SCRIPT)], check=True)

STUB = r'''#!/usr/bin/env python3
import json, os, pathlib, subprocess, sys, time
b=pathlib.Path(os.environ['FIXTURE'])
name=pathlib.Path(sys.argv[0]).name
args=sys.argv[1:]
def read(name, default):
 p=b/name
 return json.loads(p.read_text()) if p.exists() else default
def event(kind, argv):
 rows=read('events.json',[]); rows.append([kind,argv]); (b/'events.json').write_text(json.dumps(rows))
def publish(path):
 p=b/path; p.parent.mkdir(parents=True,exist_ok=True); p.touch()
def process(path):
 rows=read('processes.json',[])
 if path not in rows: rows.append(path)
 (b/'processes.json').write_text(json.dumps(rows))
if name=='uname': print(os.environ.get('BOARD','RaspberryPi400'))
elif name=='id': print(os.environ.get('FIXTURE_UID','0'))
elif name in ('ifconfig','slmctl'):
 event('FORBIDDEN_NETWORK_QUERY',args); sys.exit(90)
elif name=='waitfor': sys.exit(0 if pathlib.Path(args[0]).exists() else 1)
elif name=='pidin':
 for i,path in enumerate(read('processes.json',[])): print(100+i,path)
elif name=='sleep':
 if os.environ.get('BLOCK_SLEEP')=='1':
  (b/'sleep.started').touch(); time.sleep(120)
elif name in ('setfacl','chown'): event(name,args)
elif name=='on':
 event('on',args)
 env=dict(os.environ); i=0
 while i<len(args) and args[i].startswith('-'):
  flag=args[i]; i+=1
  if flag in ('-e','-u'):
   value=args[i]; i+=1
   if flag=='-e':
    key,value=value.split('=',1); env[key]=value
   else:
    assert value=='1000:1000,36', value
    env['FIXTURE_UID']='1000'
 command=args[i:]; exe=pathlib.Path(command[0]).name
 if exe=='sh':
  if pathlib.Path(command[1]).name=='usb_start.sh':
   if os.environ.get('FAIL_USB')!='1':
    publish('dev/usb/io-usb-otg'); publish('dev/usb/devu-hcd-bcm2711-xhci.so')
  else:
   subprocess.run(command,env=env,check=False)
 elif exe=='drm-rpi4':
  if os.environ.get('FAIL_DRM')!='1':
   publish('dev/drm/card0'); process(command[0])
   if os.environ.get('FAIL_RENDER_NODE')!='1': publish('dev/drm/renderD128')
 elif exe=='screen':
  if os.environ.get('FAIL_SCREEN')!='1': publish('dev/screen/command'); process(command[0])
 elif exe=='io-hid':
  if os.environ.get('FAIL_HID')!='1': publish('dev/io-hid/io-hid'); process(command[0])
 elif exe=='fullscreen-winmgr':
  if os.environ.get('FAIL_WM')!='1': process(command[0])
 else: raise AssertionError('Unexpected command '+repr(command))
elif name=='demolauncher':
 event('demolauncher',[os.environ.get('FIXTURE_UID'),*args])
 if os.environ.get('FAIL_LAUNCHER')=='1': sys.exit(1)
 process(sys.argv[0])
else: raise AssertionError('Unexpected utility '+name)
'''


class Fixture:
    def __init__(self, **env):
        self.temp = tempfile.TemporaryDirectory(prefix='pi400-desktop-fixture-')
        self.base = pathlib.Path(self.temp.name)
        self.compat = self.base / 'compat'
        self.registry = self.base / 'home/qnxuser/.local/share/rnqnx/launcher'
        self.stock = self.base / 'system/etc/desktop_files'
        self.lock = self.base / 'dev/shmem/pi400-rnqnx-autostart.lock'
        for p in (self.compat/'sbin', self.compat/'lib/dll', self.compat/'usr/lib/graphics/rpi4-drm',
                  self.registry, self.stock, self.lock.parent, self.base/'tmp',
                  self.base/'bin', self.base/'system/bin', self.base/'proc/boot'):
            p.mkdir(parents=True, exist_ok=True)
        for name in ('screen-rpi4drm.so',): (self.compat/'lib/dll'/name).touch()
        (self.compat/'usr/lib/libdrm.so.1').touch()
        (self.compat/'usr/lib/graphics/rpi4-drm/graphics-pi400-hdmi2.conf').touch()
        (self.compat/'enable-rnqnx-autostart').touch()
        (self.registry/'owner').write_text('react-native-qnx/launcher-v1\n')
        for p in (self.registry, self.stock): (p/'desktop.conf').write_text('stock desktop\n')
        for name in ('on','id','uname','waitfor','pidin','sleep','chown','setfacl','ifconfig','slmctl'):
            self.executable(self.base/'bin'/name)
        for name in ('screen','fullscreen-winmgr','demolauncher','io-hid'):
            self.executable(self.base/'system/bin'/name)
        self.executable(self.compat/'sbin/drm-rpi4')
        self.executable(self.base/'proc/boot/usb_start.sh')
        paths={'/data/pi400-graphics-compat':str(self.compat),
               '/data/home/':str(self.base/'home')+'/', '/dev/null':'/dev/null'}
        paths.update({prefix:str(self.base)+prefix for prefix in ('/system/','/proc/boot/','/dev/','/tmp/')})
        pattern='|'.join(re.escape(p) for p in sorted(paths,key=len,reverse=True))
        source=re.sub(pattern,lambda match:paths[match.group()],SOURCE)
        self.script = self.base/'candidate.sh'
        self.script.write_text(source)
        self.env = dict(os.environ, PATH=str(self.base/'bin')+':'+os.environ['PATH'],
                        FIXTURE=str(self.base), **env)

    def executable(self, path):
        path.write_text(STUB)
        path.chmod(0o755)

    def run(self, *args):
        return subprocess.run(['sh',str(self.script),*args],env=self.env,capture_output=True,text=True,timeout=10)

    def events(self):
        p=self.base/'events.json'
        return json.loads(p.read_text()) if p.exists() else []

    def ready(self, result):
        assert result.returncode==0, result.stdout+result.stderr
        assert 'PI400_DESKTOP_PROCESSES_READY' in result.stdout, result.stdout
        assert not self.lock.exists()
        assert not any(row[0]=='FORBIDDEN_NETWORK_QUERY' for row in self.events())

    def close(self): self.temp.cleanup()


passed=[]
def check(name, action, **env):
    f=Fixture(**env)
    try:
        action(f)
        passed.append(name)
        print('PASS',name,flush=True)
    finally: f.close()


def fresh(f):
    f.ready(f.run())
    demo=[row[1] for row in f.events() if row[0]=='demolauncher']
    assert demo==[['1000','-a',str(f.registry)]], demo
    on=[row[1] for row in f.events() if row[0]=='on']
    assert any('1000:1000,36' in args for args in on)
    assert any('-s' in args and 'demolauncher,st,gtk' in args for args in on)

def inactive(f, mode):
    if mode=='marker': (f.compat/'enable-rnqnx-autostart').unlink()
    r=f.run(); assert r.returncode==0 and not f.events(), r.stdout+r.stderr

def rejected(f, message, *args):
    r=f.run(*args); assert r.returncode!=0 and message in r.stdout, r.stdout+r.stderr
    assert not f.lock.exists()

def locked(f):
    f.lock.write_text('another invocation\n')
    r=f.run(); assert r.returncode!=0 and 'LOCK_UNAVAILABLE' in r.stdout
    assert f.lock.read_text()=='another invocation\n' and not f.events()

def unsafe_logs(f):
    (f.compat/'desktop-logs').symlink_to(f.base/'tmp')
    rejected(f,'ERROR_LOG_DIRECTORY')
    assert not f.events()

def missing(f, kind):
    p=f.compat/('sbin/drm-rpi4' if kind=='EXECUTABLE' else 'usr/lib/libdrm.so.1')
    p.unlink(); rejected(f,'MISSING_'+kind); assert not f.events()

def failed_stage(f, message, forbidden):
    rejected(f,message)
    assert not any(forbidden in ' '.join(row[1]) for row in f.events() if row[0]=='on')

def degraded(f):
    r=f.run(); f.ready(r)
    assert 'WARNING_INPUT_UNAVAILABLE' in r.stdout

def repeat(f):
    f.ready(f.run()); count=sum(row[0]=='on' for row in f.events())
    f.ready(f.run()); assert sum(row[0]=='on' for row in f.events())==count

def fallback(f, symlink=False):
    if symlink:
        moved=f.base/'untrusted-overlay'; f.registry.rename(moved); f.registry.symlink_to(moved)
    else: (f.registry/'owner').write_text('not-owned\n')
    f.ready(f.run())
    assert ['demolauncher',['1000','-a',str(f.stock)]] in f.events()

def config_symlink(f):
    config=f.registry/'desktop.conf'
    config.unlink()
    config.symlink_to(f.stock/'desktop.conf')
    f.ready(f.run())
    assert ['demolauncher',['1000','-a',str(f.stock)]] in f.events()

def user_mode(f):
    r=f.run('--launcher-user'); assert r.returncode==0, r.stdout+r.stderr
    assert f.events()==[['demolauncher',['1000','-a',str(f.registry)]]]

def signal_cleanup(f):
    p=subprocess.Popen(['sh',str(f.script)],env=f.env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,start_new_session=True)
    try:
        deadline=time.monotonic()+5
        while not (f.base/'sleep.started').exists() and p.poll() is None:
            if time.monotonic()>deadline: raise AssertionError('No blocking wait reached')
            time.sleep(.01)
        assert f.lock.exists()
        os.killpg(p.pid,signal.SIGTERM)
        out,err=p.communicate(timeout=5)
        assert p.returncode==143, (p.returncode,out,err)
        assert not f.lock.exists()
    finally:
        if p.poll() is None: os.killpg(p.pid,signal.SIGKILL); p.wait()

check('Fresh desktop without any network query',fresh)
check('No marker is inert',lambda f:inactive(f,'marker'))
check('Other board is inert',lambda f:inactive(f,'board'),BOARD='RaspberryPi5')
check('Driver administration requires root',lambda f:rejected(f,'REQUIRES_DRIVER_ADMIN'),FIXTURE_UID='1000')
check('Launcher branch rejects root',lambda f:rejected(f,'LAUNCHER_UID','--launcher-user'))
check('Launcher branch only executes user launcher',user_mode,FIXTURE_UID='1000')
check('Unknown argument fails before services',lambda f:rejected(f,'ERROR_ARGUMENT','--unknown'))
check('Missing driver fails closed',lambda f:missing(f,'EXECUTABLE'))
check('Missing runtime fails closed',lambda f:missing(f,'RUNTIME'))
check('Existing operation lock retained',locked)
check('Symlink log directory rejected before service startup',unsafe_logs)
check('DRM failure does not start Screen',lambda f:failed_stage(f,'ERROR_DRM','/system/bin/screen'),FAIL_DRM='1')
check('Missing render node does not start Screen',lambda f:failed_stage(f,'ERROR_DRM_RENDER_NODE','/system/bin/screen'),FAIL_RENDER_NODE='1')
check('Screen failure does not start window manager',lambda f:failed_stage(f,'ERROR_SCREEN','fullscreen-winmgr'),FAIL_SCREEN='1')
check('USB failure does not suppress graphics',degraded,FAIL_USB='1')
check('HID failure does not suppress graphics',degraded,FAIL_HID='1')
check('Window manager failure does not start launcher',lambda f:failed_stage(f,'ERROR_WINDOW_MANAGER','--launcher-user'),FAIL_WM='1')
check('Launcher exit is reported',lambda f:rejected(f,'ERROR_LAUNCHER'),FAIL_LAUNCHER='1')
check('Repeated controller does not restart services',repeat)
check('Unowned overlay falls back to untouched stock',fallback)
check('Symlink overlay falls back to untouched stock',lambda f:fallback(f,True))
check('Symlink desktop configuration falls back to stock',config_symlink)
check('TERM removes only its own operation lock',signal_cleanup,FAIL_WM='1',BLOCK_SLEEP='1')
assert not any(word in SOURCE for word in ('ifconfig ', 'slmctl ', 'qconn ', 'rnqnx-hello-world', 'post_startup.sh', 'reboot', 'config.txt', '.ifs'))
print(f'{len(passed)} executed host fixtures passed; syntax passed. No QNX execution, physical cold-boot, display or input qualification claimed.')
