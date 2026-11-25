"""Frame-local Steam launch prefix; no PC, network or app timeout required."""
import argparse,datetime,hashlib,json,os,sys,subprocess,time,traceback,uuid
from pathlib import Path

PORTAL={'hl2.exe','hl2vr.exe','portal.exe','portal2.exe'}
def processes():
    names=set()
    for p in Path('/proc').glob('[0-9]*/comm'):
        try:names.add(p.read_text().strip().lower())
        except OSError:pass
    return names

def archive_tail(source,target,limit=2*1024*1024):
    if not source.is_file() or source.is_symlink():return
    with source.open('rb') as stream:
        size=source.stat().st_size;stream.seek(max(0,size-limit));data=stream.read(limit)
    target.write_bytes(data)

def validate(profile,folder):
    assert profile['version']==1
    game=Path(profile['game']);setup=Path(profile['setup'])
    assert game.is_absolute() and setup.is_absolute() and game.is_dir()
    assert not game.is_symlink() and not setup.is_symlink()
    assert not setup.is_relative_to(game) and not game.is_relative_to(setup)
    assert folder==setup/'Standalone-Launch'
    assert profile['uri']=='auto' or (profile['uri'].startswith('steam://rungameid/') and profile['uri'].split('/')[-1].isdigit())
    for rel in ('bin/d3d9.dll','hl2.exe','bin/VR/config.txt'):
        p=game/rel;assert p.is_file() and p.resolve().is_relative_to(game.resolve()) and not p.is_symlink()
    return game,setup

def restart_uri(profile):
    if profile['uri']!='auto':return profile['uri']
    gameid=os.environ.get('SteamGameId','')
    if gameid.isdecimal() and int(gameid)>0xffffffff:return 'steam://rungameid/'+gameid
    appid=os.environ.get('SteamAppId','')
    if appid.isdecimal() and 0<int(appid)<=0xffffffff:return 'steam://rungameid/'+str((int(appid)<<32)|0x02000000)
    return None

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--self-test',action='store_true')
    parser.add_argument('command',nargs=argparse.REMAINDER);options=parser.parse_args()
    command=options.command;command=command[1:] if command[:1]==['--'] else command
    folder=Path(__file__).resolve().parent;profile=json.loads((folder/'profile.json').read_text())
    game,setup=validate(profile,folder)
    assert command,'Steam compatibility command missing'
    names=processes();assert not names&PORTAL,'Portal is already running; existing game preserved'
    stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex[:8]
    attempt=folder/'Attempts'/stamp;attempt.mkdir(parents=True)
    log=(attempt/'launch.log').open('a',buffering=1)
    os.dup2(log.fileno(),1);os.dup2(log.fileno(),2)
    state=dict(started_utc=stamp,revision=profile['revision'],pc_required=False,network_required=False,
               self_test=options.self_test,game_launched=False,lifetime_limit=None)
    def save():(attempt/'result.json').write_text(json.dumps(state,indent=2)+'\n')
    save()
    try:
        with (game/'bin/d3d9.dll').open('rb') as f:actual=hashlib.file_digest(f,'sha256').hexdigest()
        assert actual==profile['dll_sha256'],'Installed build differs from this library profile'
        sys.path.insert(0,str(game/'bin/VR'))
        from appid_guard import archive_generated_appid
        from portal_startup_settings import apply_startup_settings
        if not options.self_test:
            for name in ('portalvr.log','hl2_d3d9.log','portal/console.log'):
                archive_tail(game/name,attempt/('previous-'+Path(name).name))
            state['archived_appid']=archive_generated_appid(game,setup)
            state['startup_settings']=apply_startup_settings(game,setup,names)
            uri=restart_uri(profile)
            if uri:
                try:
                    with (folder/'restart-worker.log').open('a') as workerlog:
                        subprocess.Popen(['/usr/bin/python3',str(game/'bin/VR/portal_restart_helper.py'),'--game',str(game),'--setup',str(setup),'--uri',uri],
                                         stdout=workerlog,stderr=subprocess.STDOUT,start_new_session=True)
                except OSError as error:state['restart_helper_error']=str(error)
            else:state['restart_helper_unavailable']='Steam app identity unavailable; quit and relaunch manually.'
        # Steam targets a separate, harmless Windows relay directory. It cannot
        # mistake Source's generated root app-ID marker for shortcut ownership.
        state['command_dispatched']=True;state['game_launched']=not options.self_test;save()
        child=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
        state['compatibility_pid']=child.pid;save()
        try:code=child.wait() # Deliberately unbounded. Observer never kills game.
        except BaseException:
            state['outcome']='observer-detached';save();raise
        state['returncode']=code
        state['outcome']='cpu-self-test-passed' if options.self_test and code==0 else 'normal-exit' if code==0 else 'cpu-self-test-failed' if options.self_test else 'nonzero-exit-review-game-logs'
        state['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();save();return code
    except Exception as error:
        state['error']=str(error);state.setdefault('outcome','initialization-failure');save();traceback.print_exc();return 1

if __name__=='__main__':raise SystemExit(main())
