"""Per-launch Frame restart worker. Runs only after the user launches Portal."""
import argparse
import json
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
try:import fcntl
except ImportError:fcntl=None # State-machine tests also run on the Windows host.

from appid_guard import archive_generated_appid
from portal_startup_settings import apply_startup_settings

PORTAL={'hl2.exe','hl2vr.exe','portal.exe','portal2.exe'}
OTHER_GAMES={'ctr-vr.exe'}

def valid_request(value,now,session_start,seen):
    return (isinstance(value,dict) and value.get('version')==1 and
        value.get('operation')=='restart_portal' and
        isinstance(value.get('created_at'),(int,float)) and
        session_start-2<=value['created_at']<=now+2 and now-value['created_at']<=45 and
        isinstance(value.get('nonce'),str) and re.fullmatch(r'[0-9]+-[0-9]+',value['nonce']) is not None and
        value['nonce'] not in seen)

class RestartState:
    def __init__(self,now):
        self.started=now;self.last_game=now;self.saw_game=False
        self.requested=None;self.quiet_since=None;self.launch_pending=False
    def tick(self,now,portal,other,request=False):
        if portal:self.last_game=now;self.saw_game=True
        if request and not self.launch_pending and self.requested is None and (portal or now-self.last_game<5):
            self.requested=now;self.quiet_since=None
        if self.launch_pending:
            if portal:self.launch_pending=False;return 'running'
            if now-self.last_game>90:return 'stop'
            return 'wait'
        if self.requested is not None:
            if now-self.requested>75:self.requested=None;return 'restart-timeout'
            if portal or other:self.quiet_since=None;return 'wait'
            if self.quiet_since is None:self.quiet_since=now
            if now-self.quiet_since>=8:
                self.requested=None;self.launch_pending=True;self.last_game=now;return 'launch'
            return 'wait'
        if self.saw_game and now-self.last_game>45:return 'stop'
        if not self.saw_game and now-self.started>120:return 'stop'
        return 'running' if portal else 'wait'

def atomic(path,text):
    assert not path.is_symlink()
    fd,pending=tempfile.mkstemp(prefix='.portal-restart-',dir=path.parent)
    try:
        with os.fdopen(fd,'w') as stream:stream.write(text);stream.flush();os.fsync(stream.fileno())
        os.replace(pending,path)
    finally:
        if os.path.exists(pending):os.unlink(pending)

def process_names():
    names=set()
    for file in Path('/proc').glob('[0-9]*/comm'):
        try:names.add(file.read_text().strip().lower())
        except OSError:pass
    return names

def main():
    args=argparse.ArgumentParser(description=__doc__)
    args.add_argument('--game',required=True);args.add_argument('--setup',required=True)
    args.add_argument('--uri',required=True)
    options=args.parse_args()
    game=Path(options.game);setup=Path(options.setup)
    assert game.name=='Portal_Frame_VR_Experimental' and game.is_dir() and not game.is_symlink()
    assert setup==game.with_name(game.name+'_Setup_20261116') and not setup.is_symlink()
    assert options.uri=='steam://rungameid/10005311291717058560'
    runtime=game/'bin/VR';assert runtime.resolve().is_relative_to(game.resolve())
    for parent in (game/'bin',runtime):assert not parent.is_symlink()
    folder=setup/'Restart-Helper';folder.mkdir(exist_ok=True);assert not folder.is_symlink()
    lock=(folder/'worker.lock').open('a')
    try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:return 0
    heartbeat=runtime/'restart-helper-alive.txt';request=runtime/'restart-request.json'
    started=time.time();state=RestartState(started);seen=set()
    script_sha=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    def log(event,**details):
        with (folder/'events.jsonl').open('a') as stream:
            stream.write(json.dumps(dict(time=time.time(),event=event,**details))+'\n')
    log('worker-start',pid=os.getpid(),lifetime_limit=None)
    try:
        while True:
            now=time.time();atomic(heartbeat,str(int(now)+5)+'\n'+script_sha+'\n')
            requested=False
            if request.is_file() and not request.is_symlink():
                try:
                    assert request.stat().st_size<2048
                    value=json.loads(request.read_text())
                    if valid_request(value,now,started,seen):
                        seen.add(value['nonce']);requested=True;log('restart-request',nonce=value['nonce'])
                except (OSError,ValueError,AssertionError):pass
            names=process_names();action=state.tick(now,bool(names&PORTAL),bool(names&OTHER_GAMES),requested)
            if action=='stop':break
            if action=='restart-timeout':log('restart-timeout')
            if action=='launch':
                # Preserve the prior run before Portal resets its log at startup.
                archive=folder/('Before-Restart-'+str(time.time_ns()));archive.mkdir()
                for source in (game/'portalvr.log',game/'hl2_d3d9.log',game/'portal/console.log'):
                    if source.is_file() and not source.is_symlink():shutil.copyfile(source,archive/source.name)
                archived=archive_generated_appid(game,setup)
                startup_settings=apply_startup_settings(game,setup,process_names())
                log('startup-settings',**startup_settings)
                with (folder/'steam-uri-console.log').open('a') as output:
                    command=subprocess.Popen(['/usr/bin/steam',options.uri],stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
                try:command_code=command.wait(timeout=15)
                except subprocess.TimeoutExpired:command_code=None # Observer detaches; command/app stays running.
                log('launch-command',returncode=command_code,command_pending=command_code is None,command_pid=command.pid,archived_appid=archived,gameplay_verified=False)
                if command_code not in (None,0):break
            time.sleep(.25)
    finally:
        if heartbeat.is_file() and not heartbeat.is_symlink():heartbeat.unlink()
        log('worker-stop');lock.close()
    return 0

if __name__=='__main__':raise SystemExit(main())
