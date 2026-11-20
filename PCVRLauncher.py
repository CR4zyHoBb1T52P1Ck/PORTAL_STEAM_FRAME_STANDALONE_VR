"""Portal VR desktop launcher. No timed game termination or automatic uploads."""
import csv,datetime,hashlib,json,os,re,subprocess,sys,threading,time,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parent
HIDDEN={'creationflags':getattr(subprocess,'CREATE_NO_WINDOW',0)}
GAMES={'hl2.exe','hl2vr.exe','portal.exe','portal2.exe'}
def names():
    r=subprocess.run(['tasklist','/fo','csv','/nh'],capture_output=True,text=True,check=True,**HIDDEN)
    return {x[0].lower() for x in csv.reader(r.stdout.splitlines()) if x}
def game_path(value):
    game=Path(value).expanduser().resolve()
    if not (game/'hl2.exe').is_file() or not (game/'portal').is_dir():raise RuntimeError('Select your owned Windows Steam Portal folder containing hl2.exe and portal/.')
    return game
def check(game,manifest):
    present=names();dll=game/'bin/d3d9.dll'
    actual=hashlib.sha256(dll.read_bytes()).hexdigest() if dll.is_file() else None
    return dict(build=manifest['build'],dll_matches=actual==manifest['files']['bin/d3d9.dll'],dll_sha256=actual,
                running_games=sorted(present&GAMES),steam_running='steam.exe' in present,steamvr_ready={'vrserver.exe','vrcompositor.exe'}<=present,
                game_launch_authorized=False,game_lifetime_limit=None)
def command(game,take=None):
    args=[str(game/'hl2.exe'),'-steam','-game','portal','-insecure','-window','-novid','-dx9','-width','1280','-height','720','+mat_queue_mode','0','+mat_vsync','0','+mat_antialias','0','-condebug']
    if take:
        if not re.fullmatch(r'portalvr_take_[A-Za-z0-9_]{1,64}',take):raise RuntimeError('Choose a local portalvr_take_*.dem recording.')
        if not (game/'portal'/(take+'.dem')).is_file():raise RuntimeError('Recording file is missing.')
        args+=['+demo_quitafterplayback','0','+playdemo',take]
    return args
def defaults(game):
    # A small separately backed-up PC profile. Preserve every unrelated setting.
    sys.path.insert(0,str(ROOT/'tools/frame'))
    config=game/'bin/VR/config.txt';data=config.read_bytes();text=data.decode('utf-8-sig')
    replacements={'RenderWindow':'1','CreatorCameraMode':'0'}
    for key,value in replacements.items():
        if re.search(r'^'+key+r'=',text,re.M):text=re.sub(r'^'+key+r'=.*$',key+'='+value,text,flags=re.M)
        else:text+='\n'+key+'='+value+'\n'
    archive=game/'.portal-frame-backups'/('pcvr-profile-'+str(time.time_ns()));archive.mkdir(parents=True)
    (archive/'config.txt.before').write_bytes(data)
    temp=config.with_name('pcvr-profile.tmp');temp.write_text(text);os.replace(temp,config)
    return str(archive)
def main():
    import tkinter as tk
    from tkinter import ttk,filedialog,scrolledtext
    if os.name!='nt':raise RuntimeError('This launcher is for Windows PCVR. Use FrameSetup.py on Steam Frame.')
    manifest=json.loads((ROOT/'manifest.json').read_text());window=tk.Tk();window.title('Portal VR — PCVR / Creator Launcher');window.geometry('1020x710')
    state_path=ROOT/'PCVR-Local-Preferences.json';saved={}
    try:saved=json.loads(state_path.read_text())
    except (OSError,ValueError):pass
    selected=tk.StringVar(value=saved.get('portal_dir',''));status=tk.StringVar(value='Select your Windows Portal folder. Existing games remain under your control.')
    header=ttk.Frame(window,padding=12);header.pack(fill='x');ttk.Label(header,text='PORTAL VR  •  PCVR / CREATOR TOOLS',font=('Segoe UI',18,'bold')).pack(anchor='w')
    row=ttk.Frame(header);row.pack(fill='x',pady=10);ttk.Entry(row,textvariable=selected).pack(side='left',fill='x',expand=True)
    ttk.Button(row,text='Choose Portal folder',command=lambda:selected.set(filedialog.askdirectory() or selected.get())).pack(side='right',padx=8)
    ttk.Label(header,textvariable=status,wraplength=960).pack(anchor='w')
    notebook=ttk.Notebook(window);notebook.pack(fill='both',expand=True,padx=12,pady=6)
    play=ttk.Frame(notebook,padding=12);replay=ttk.Frame(notebook,padding=12);diag=ttk.Frame(notebook,padding=12)
    notebook.add(play,text='Play / Setup');notebook.add(replay,text='Recordings / Theatre');notebook.add(diag,text='Diagnostics')
    output=scrolledtext.ScrolledText(window,height=12,state='disabled');output.pack(fill='both',padx=12,pady=10)
    attempts=ROOT/'PCVR-Logs';attempts.mkdir(exist_ok=True);latest={};child_state={}
    def say(text):
        def append():output.configure(state='normal');output.insert('end',str(text)+'\n');output.see('end');output.configure(state='disabled')
        window.after(0,append)
    def get():
        game=game_path(selected.get());state_path.write_text(json.dumps({'portal_dir':str(game)},indent=2)+'\n');return game
    def job(fn):
        def run():
            try:fn()
            except Exception as e:say('Action failed: '+str(e));window.after(0,lambda:status.set('Review the log below. No game was stopped.'))
        threading.Thread(target=run,daemon=True).start()
    def inspect():
        game=get();r=check(game,manifest);latest.update(summary=r);say(json.dumps(r,indent=2));window.after(0,lambda:status.set('Read-only diagnostics complete.'))
    def install():
        game=get();busy=names()&GAMES
        if busy:raise RuntimeError('Close Portal yourself before installing; existing game preserved.')
        r=subprocess.run([sys.executable,str(ROOT/'Install.py'),'--portal-dir',str(game)],capture_output=True,text=True,**HIDDEN)
        say(r.stdout);say(r.stderr)
        if r.returncode:raise RuntimeError('Installer failed; inspect its backup journal.')
    def profile():
        game=get()
        if names()&GAMES:raise RuntimeError('Close Portal yourself before changing its PC mirror profile.')
        say('PC mirror profile saved; backup: '+defaults(game))
    def launch(take=None):
        game=get();r=check(game,manifest)
        if r['running_games']:raise RuntimeError('Portal is already running. Finish that session yourself before a new launch.')
        if not r['dll_matches']:raise RuntimeError('Install this PCVR build first; DLL checksum differs.')
        if not r['steam_running'] or not r['steamvr_ready']:raise RuntimeError('Start Steam and SteamVR first, then press Play when ready.')
        folder=attempts/(datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'));folder.mkdir()
        for name in ('portalvr.log','hl2_d3d9.log','portal/console.log'):
            p=game/name
            if p.is_file():
                with p.open('rb') as f:f.seek(max(0,p.stat().st_size-1024*1024));(folder/('previous-'+p.name)).write_bytes(f.read())
        env=os.environ.copy();env.update(SteamAppId='400',SteamGameId='400')
        log=(folder/'launcher.log').open('a');child=subprocess.Popen(command(game,take),cwd=game,env=env,stdout=log,stderr=subprocess.STDOUT)
        record=dict(pid=child.pid,build=manifest['build'],started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),lifetime_limit=None,mode='replay' if take else 'play')
        latest.update(folder=folder,game=game);child_state.update(child=child)
        (folder/'result.json').write_text(json.dumps(record,indent=2)+'\n');say('Portal started. Quit in-game or use Steam Stop when you decide to finish. Closing this launcher leaves Portal running.')
        child_state['session_start']=time.time();child_state['restart_pending']=False
        code=child.wait() # Observer only, deliberately unbounded.
        record.update(returncode=code,outcome='normal-exit' if code==0 else 'crash-exception' if code&0x80000000 else 'nonzero-exit-review-runtime-log',finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
        (folder/'result.json').write_text(json.dumps(record,indent=2)+'\n');log.close();say('Portal exited: '+record['outcome'])
        if child_state.pop('restart_pending',False):window.after(1500,lambda:job(launch))
    ttk.Label(play,text='1. Install the mod into your owned Windows Portal folder.\n2. Optionally enable the desktop mirror profile.\n3. Start SteamVR and press Play when ready.',wraplength=900).pack(anchor='w',pady=8)
    for label,fn in [('Install / update PCVR mod',install),('Apply PC mirror profile (preserves other settings)',profile),('Start SteamVR',lambda:os.startfile('steam://rungameid/250820')),('Play Portal VR',launch),('Check setup',inspect)]:ttk.Button(play,text=label,command=lambda f=fn:job(f)).pack(anchor='w',pady=5)
    ttk.Label(play,text='In VR: Creator menu → spectator follow/orbit/free camera. Optional focused-window WASD/QE + arrows parks player movement while editing the camera.\nThe mirror is an extra render; disable it when maximum headset performance matters.',wraplength=900).pack(anchor='w',pady=10)
    recordings=tk.Listbox(replay,height=10);recordings.pack(fill='x')
    def refresh():
        game=get();items=sorted((game/'portal').glob('portalvr_take_*.dem'),key=lambda p:p.stat().st_mtime,reverse=True);recordings.delete(0,'end')
        for p in items:recordings.insert('end',p.stem)
    def play_take():
        choice=recordings.curselection()
        if not choice:raise RuntimeError('Select a recording first.')
        launch(recordings.get(choice[0]))
    ttk.Button(replay,text='Refresh local recordings',command=lambda:job(refresh)).pack(anchor='w',pady=5)
    ttk.Button(replay,text='Play selected take in SteamVR',command=lambda:job(play_take)).pack(anchor='w',pady=5)
    ttk.Label(replay,text='Record and stop takes from the in-game Creator menu. Native demo timeline, pause and playback speed are available there.\nNative demos retain the world/game events; injected VR finger/body poses are not yet captured as a pose replay.',wraplength=900).pack(anchor='w',pady=8)
    def export():
        game=get();summary=check(game,manifest);dest=attempts/('Diagnostics-'+str(time.time_ns())+'.zip')
        with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED) as z:
            z.writestr('summary.json',json.dumps(summary,indent=2));z.writestr('REVIEW-BEFORE-SHARING.txt','Local diagnostics only. Review paths/log contents before sharing; nothing was uploaded.\n')
            for name in ('portalvr.log','hl2_d3d9.log'):
                p=game/name
                if p.is_file():
                    with p.open('rb') as f:f.seek(max(0,p.stat().st_size-1024*1024));z.writestr(name,f.read())
        say('Diagnostic archive saved locally: '+str(dest))
    ttk.Button(diag,text='Run read-only setup checks',command=lambda:job(inspect)).pack(anchor='w',pady=6)
    ttk.Button(diag,text='Export local diagnostic ZIP for review',command=lambda:job(export)).pack(anchor='w',pady=6)
    ttk.Label(diag,text='No automatic upload, recurring background task or timed game shutdown is installed. The launcher observes exit codes and preserves per-attempt logs.').pack(anchor='w',pady=10)
    seen=set()
    def restart_tick():
        try:
            child=child_state.get('child');game=latest.get('game')
            if game and child and child.poll() is None:
                runtime=game/'bin/VR';heartbeat=runtime/'restart-helper-alive.txt';temp=runtime/'pcvr-heartbeat.tmp'
                temp.write_text(str(int(time.time())+4)+'\npcvr-launcher\n');os.replace(temp,heartbeat)
                request=runtime/'restart-request.json'
                if request.is_file() and request.stat().st_size<2048:
                    from portal_restart_helper import valid_request
                    value=json.loads(request.read_text())
                    if valid_request(value,time.time(),child_state.get('session_start',time.time()),seen):
                        seen.add(value['nonce']);child_state['restart_pending']=True;say('In-game restart requested. Waiting for its normal exit; no game deadline is imposed.')
        except (OSError,ValueError,ImportError):pass
        window.after(1000,restart_tick)
    sys.path.insert(0,str(ROOT/'tools/frame'));window.after(1000,restart_tick)
    ttk.Button(play,text='Cancel pending restart',command=lambda:child_state.update(restart_pending=False)).pack(anchor='w',pady=4)
    window.protocol('WM_DELETE_WINDOW',window.destroy);window.mainloop()
if __name__=='__main__':main()
