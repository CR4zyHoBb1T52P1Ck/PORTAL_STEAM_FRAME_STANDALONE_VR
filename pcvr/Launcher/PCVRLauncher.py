"""Portal VR desktop launcher. No timed game termination or automatic uploads."""
import csv,datetime,hashlib,json,os,queue,re,subprocess,sys,threading,time,zipfile
from pathlib import Path
import pcvr_records
import pcvr_diagnostics
import pcvr_input_audit
import pcvr_presentation
ROOT=Path(__file__).resolve().parent
HIDDEN={'creationflags':getattr(subprocess,'CREATE_NO_WINDOW',0)}
GAMES={'hl2.exe','hl2vr.exe','portal.exe','portal2.exe'}

class UIQueue:
    """Workers only enqueue; Tk callbacks execute on the owning UI thread."""
    def __init__(self):
        self.pending=queue.Queue();self.owner=threading.get_ident();self.closed=False;self.lock=threading.Lock()
    def post(self,callback):
        with self.lock:
            if self.closed:return False
            self.pending.put(callback);return True
    def drain(self):
        if threading.get_ident()!=self.owner:raise RuntimeError('UI queue drained outside its owner thread')
        for _ in range(100):
            with self.lock:
                if self.closed:return
                try:callback=self.pending.get_nowait()
                except queue.Empty:return
            callback()
    def close(self):
        with self.lock:
            self.closed=True
            while not self.pending.empty():self.pending.get_nowait()

class RestartIntent:
    def __init__(self):self.epoch=0;self.session=None;self.pending=False
    def cancel(self):self.epoch+=1;self.pending=False
    def begin(self,session):self.cancel();self.session=session
    def request(self,session):
        if session is self.session:self.pending=True
    def take(self,session):
        if session is not self.session or not self.pending:return None
        self.pending=False;return (self.epoch,session)
    def valid(self,ticket):return ticket is not None and ticket[0]==self.epoch and ticket[1] is self.session

def observe_exit(child,record,folder,log,game=None):
    """Observe without deadlines; kept alive silently if the GUI closes."""
    try:
        code=child.wait()
        record.update(stage='exited',returncode=code,outcome='normal-exit' if code==0 else 'crash-exception' if code&0x80000000 else 'nonzero-exit-review-runtime-log',finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
    except Exception as e:
        record.update(stage='observer-failed',outcome='observer-failed-game-left-running',observer_error=str(e))
    finally:
        try:
            if game is not None:record['exit_logs']=pcvr_records.capture_logs(game,folder,'exit-')
            pcvr_records.atomic_json(folder/'result.json',record)
        except Exception as e:
            record['logging_error']=str(e)
            try:(folder/'observer-record-error.txt').write_text(str(e),encoding='utf-8')
            except OSError:pass
        finally:log.close()
    return record['outcome']
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
def verified_check(game,manifest):
    report=pcvr_diagnostics.inspect(game,manifest,names());binding=pcvr_input_audit.installed(game);report['index_binding_audit']=binding
    if not binding['valid']:report['blocking_issues']+=binding['errors']
    report['ready_for_owner_launch']=not report['blocking_issues'];return report

def command(game,take=None):
    args=[str(game/'hl2.exe'),'-steam','-game','portal','-insecure','-window','-novid','-dx9','-width','1280','-height','720','+mat_queue_mode','0','+mat_vsync','0','+mat_antialias','0','-condebug']
    if take:
        if not re.fullmatch(r'portalvr_take_[A-Za-z0-9_]{1,64}',take):raise RuntimeError('Choose a local portalvr_take_*.dem recording.')
        if not (game/'portal'/(take+'.dem')).is_file():raise RuntimeError('Recording file is missing.')
        args+=['+demo_quitafterplayback','0','+playdemo',take]
    return args
def defaults(game,backup_root=None):
    import pcvr_profile09
    return pcvr_profile09.update(game,Path(backup_root) if backup_root else game/'.portal-frame-backups')

def main():
    import tkinter as tk
    from tkinter import ttk,filedialog,scrolledtext
    if os.name!='nt':raise RuntimeError('This launcher is for Windows PCVR. Use FrameSetup.py on Steam Frame.')
    manifest=json.loads((ROOT/'manifest.json').read_text());window=tk.Tk();window.title('Portal VR — '+manifest.get('build','unknown')+' / '+manifest.get('tools_revision','PCVR')+' Launcher');window.geometry('1180x820');appearance=pcvr_presentation.setup(window,tk,ttk,ROOT)
    state_root=ROOT.parent/'State';state_root.mkdir(exist_ok=True);state_path=state_root/'PCVR-Local-Preferences.json';saved={}
    try:saved=json.loads(state_path.read_text())
    except (OSError,ValueError):pass
    selected=tk.StringVar(value=saved.get('portal_dir',''));status=tk.StringVar(value='Select your Windows Portal folder. Existing games remain under your control.')
    header=ttk.Frame(window,padding=(24,10));header.pack(fill='x',padx=12,pady=(12,6))
    brand=ttk.Frame(header);brand.pack(fill='x')
    pcvr_presentation.iris(brand).pack(side='left',padx=(0,16))
    identity=ttk.Frame(brand);identity.pack(side='left',fill='x',expand=True)
    ttk.Label(identity,text='APERTURE SCIENCE  /  IMMERSIVE TESTING INITIATIVE',style='Caption.TLabel').pack(anchor='w')
    ttk.Label(identity,text='PERSONAL TEST CHAMBER',style='Title.TLabel').pack(anchor='w')
    ttk.Label(identity,text=manifest['build']+'  //  OPENVR · INDEX · CREATOR TOOLS').pack(anchor='w')
    row=ttk.Frame(header);row.pack(fill='x',pady=(12,6))
    ttk.Label(row,text='PORTAL FOLDER  >',style='Caption.TLabel').pack(side='left',padx=(0,10))
    path_entry=ttk.Entry(row,textvariable=selected);path_entry.pack(side='left',fill='x',expand=True);pcvr_presentation.edit_actions(path_entry,'entry')
    ttk.Button(row,text='Browse',command=lambda:selected.set(filedialog.askdirectory() or selected.get())).pack(side='right',padx=(8,0))
    status_entry=ttk.Entry(header,textvariable=status,state='readonly',style='Status.TEntry');status_entry.pack(fill='x');pcvr_presentation.edit_actions(status_entry,'entry',True)
    footer=ttk.Frame(window,padding=(14,7));footer.pack(side='bottom',fill='x',padx=12,pady=(4,10))
    ttk.Label(footer,text='DEVELOPMENT FEED  //',style='Caption.TLabel').pack(side='left',padx=(0,10))
    for name in ('GitHub','Releases','YouTube','Ko-fi'):
        ttk.Button(footer,text=name,style='Small.TButton',command=lambda n=name:pcvr_presentation.open_link(n)).pack(side='left',padx=3)
    panes=ttk.Panedwindow(window,orient='vertical');panes.pack(fill='both',expand=True,padx=12,pady=6)
    notebook=ttk.Notebook(panes);panes.add(notebook,weight=4)
    play=pcvr_presentation.page(notebook,'01  ACCESS')
    replay=pcvr_presentation.page(notebook,'02  RECORDINGS')
    diag=pcvr_presentation.page(notebook,'03  DIAGNOSTICS')
    console=pcvr_presentation.Console(panes);panes.add(console.frame,weight=2);output=console.text
    console.append('APERTURE PERSONAL TEST CHAMBER // LOCAL TELEMETRY ONLINE')
    console.append('Select and copy output here. Ctrl+F searches. Drag the divider to resize this panel.')
    attempts=state_root/'PCVR-Logs';attempts.mkdir(exist_ok=True);latest={};child_state={};ui=UIQueue();action_lock=threading.Lock();restart=RestartIntent();closing=threading.Event()
    def pump():
        ui.drain()
        if not ui.closed:window.after(50,pump)
    window.after(50,pump)
    def say(text):
        ui.post(lambda:console.append(text))
    def get():
        game=game_path(selected.get());state_path.write_text(json.dumps({'portal_dir':str(game)},indent=2)+'\n');return game
    def job(fn,exclusive=False,needs_game=True):
        # Capture Tk values before starting any worker. Exclusive operations
        # reject double-clicks instead of racing installation or game startup.
        try:game=get() if needs_game else None
        except Exception as e:say('Action failed: '+str(e));return
        if exclusive and not action_lock.acquire(blocking=False):say('Setup or launch is already in progress.');return
        def run():
            try:fn(game)
            except Exception as e:
                say('Action failed: '+str(e));ui.post(lambda:status.set('Review the log below. No game was stopped.'))
            finally:
                if exclusive:action_lock.release()
        threading.Thread(target=run,daemon=False).start()
    def inspect(game):
        r=verified_check(game,manifest);r['storage']=pcvr_diagnostics.storage(state_root);latest.update(summary=r);say(json.dumps(r,indent=2));ui.post(lambda:status.set('Read-only diagnostics complete.'))
    def install(game):
        busy=names()&GAMES
        if busy:raise RuntimeError('Close Portal yourself before installing; existing game preserved.')
        r=subprocess.run([sys.executable,str(ROOT/'Install.py'),'--portal-dir',str(game)],capture_output=True,text=True,**HIDDEN)
        say(r.stdout);say(r.stderr)
        if r.returncode:raise RuntimeError('Installer failed; inspect its backup journal.')
    def profile(game):
        if names()&GAMES:raise RuntimeError('Close Portal yourself before changing its PC mirror profile.')
        say('PC mirror profile saved; backup: '+defaults(game,state_root/'Backups'))
    def launch(game,take=None):
        previous=child_state.get('child')
        if previous and previous.poll() is None:raise RuntimeError('This launcher already has a running Portal session.')
        child,record,folder,log=pcvr_records.start(game,manifest,attempts,verified_check,command,take,cancelled=closing.is_set)
        latest.update(folder=folder,game=game);child_state.update(child=child)
        say('Portal started. Quit in-game or use Steam Stop when you decide to finish. Closing this launcher leaves Portal running.')
        child_state['session_start']=time.time();restart.begin(child)
        def observe():
            outcome=observe_exit(child,record,folder,log,game);say('Portal exited: '+outcome)
            if record.get('logging_error'):say('Exit logging issue: '+record['logging_error'])
            # Only the UI may schedule a user-requested restart. Closing it
            # detaches restart handling but preserves this silent exit observer.
            if outcome!='observer-failed-game-left-running':
                def finished():
                    ticket=restart.take(child)
                    if ticket is not None:
                        window.after(1500,lambda:job(lambda _:launch(game),True,False) if restart.valid(ticket) else None)
                ui.post(finished)
        threading.Thread(target=observe,daemon=False).start()
    pcvr_presentation.note(play,'CHAMBER ACCESS\nStart SteamVR, check your setup, then enter Portal when ready.')
    action_grid=ttk.Frame(play);action_grid.pack(fill='x',pady=8);action_grid.columnconfigure(0,weight=1);action_grid.columnconfigure(1,weight=1)
    actions=[('Play Portal VR',launch,True,True),('Start SteamVR',lambda _:os.startfile('steam://rungameid/250820'),True,False),('Check setup',inspect,False,True),('Apply native rig + accurate eye mirror',profile,True,True),('Install / update PCVR mod',install,True,True)]
    for i,(label,fn,exclusive,needs_game) in enumerate(actions):
        ttk.Button(action_grid,text=label,style='Play.TButton' if label=='Play Portal VR' else 'TButton',command=lambda f=fn,x=exclusive,g=needs_game:job(f,x,g)).grid(row=i//2,column=i%2,sticky='ew',padx=(0,12) if i%2==0 else (12,0),pady=8)
    pcvr_presentation.note(play,'In VR: Creator menu → spectator follow/orbit/free camera. Optional focused-window WASD/QE + arrows parks player movement while editing the camera.\nThe default desktop mirror copies a selected eye without another scene render. Creator cameras use a separate view. SteamVR overlays are separate from the eye scene.')
    recording_frame=ttk.Frame(replay);recording_frame.pack(fill='x')
    recordings=tk.Listbox(recording_frame,height=10,background=pcvr_presentation.BG,foreground=pcvr_presentation.INK,selectbackground=pcvr_presentation.CYAN,selectforeground=pcvr_presentation.BG,exportselection=False,font=('Consolas',11));recordings.pack(side='left',fill='both',expand=True);pcvr_presentation.edit_actions(recordings,'listbox',True)
    recording_scroll=ttk.Scrollbar(recording_frame,command=recordings.yview);recording_scroll.pack(side='right',fill='y');recordings.configure(yscrollcommand=recording_scroll.set)
    def refresh(game):
        items=[p.stem for p in sorted((game/'portal').glob('portalvr_take_*.dem'),key=lambda p:p.stat().st_mtime,reverse=True)]
        def show():
            recordings.delete(0,'end')
            for name in items:recordings.insert('end',name)
        ui.post(show)
    def play_take():
        choice=recordings.curselection()
        if not choice:say('Select a recording first.');return
        take=recordings.get(choice[0]);job(lambda game:launch(game,take),True)
    ttk.Button(replay,text='Refresh local recordings',command=lambda:job(refresh)).pack(anchor='w',pady=5)
    ttk.Button(replay,text='Play selected take in SteamVR',command=play_take).pack(anchor='w',pady=5)
    pcvr_presentation.note(replay,'Record and stop takes from the in-game Creator menu. Native demo timeline, pause and playback speed are available there.\nNative demos retain the world/game events; injected VR finger/body poses are not yet captured as a pose replay.')
    def export(game):
        summary=verified_check(game,manifest);summary['storage']=pcvr_diagnostics.storage(state_root);dest=attempts/('Diagnostics-'+str(time.time_ns())+'.zip')
        pcvr_records.export(game,attempts,dest,summary)
        say('Diagnostic archive saved locally: '+str(dest))
    ttk.Button(diag,text='Run read-only setup checks',command=lambda:job(inspect)).pack(anchor='w',pady=6)
    ttk.Button(diag,text='Export local diagnostic ZIP for review',command=lambda:job(export)).pack(anchor='w',pady=6)
    pcvr_presentation.note(diag,'No automatic upload, recurring background task or timed game shutdown is installed. The launcher observes exit codes and preserves per-attempt logs.')
    ttk.Button(diag,text='Open H: diagnostics folder',command=lambda:os.startfile(str(attempts))).pack(anchor='w',pady=6)
    ttk.Button(diag,text='Open latest attempt',command=lambda:job(lambda game:os.startfile(str(pcvr_records.latest_attempt(game,attempts))))).pack(anchor='w',pady=6)
    ttk.Button(diag,text='Read PCVR acceptance guide',command=lambda:os.startfile(str(ROOT.parent/'PCVR-Acceptance.md'))).pack(anchor='w',pady=6)
    notes_page=pcvr_presentation.page(notebook,'04  TEST NOTES')
    pcvr_presentation.note(notes_page,'OBSERVATION LOG\nPaste timecodes, test results and reproduction steps here. Save keeps a local version on H:.')
    notes=scrolledtext.ScrolledText(notes_page,height=12,undo=True,wrap='word',background=pcvr_presentation.BG,foreground=pcvr_presentation.INK,insertbackground=pcvr_presentation.CYAN,selectbackground=pcvr_presentation.CYAN,selectforeground=pcvr_presentation.BG,font=('Consolas',11),padx=12,pady=10,exportselection=False)
    notes.pack(fill='both',expand=True,pady=8);pcvr_presentation.edit_actions(notes)
    notes_path=state_root/'Test-Notes.txt'
    if notes_path.is_file() and notes_path.stat().st_size<1024*1024:
        notes.insert('1.0',notes_path.read_text(encoding='utf-8'))
    def save_notes():
        text=notes.get('1.0','end-1c');history=state_root/'Notes';history.mkdir(exist_ok=True)
        snapshot=history/('Test-Notes-'+str(time.time_ns())+'.txt');snapshot.write_text(text,encoding='utf-8')
        temp=state_root/'Test-Notes.tmp';temp.write_text(text,encoding='utf-8');os.replace(temp,notes_path)
        say('Test notes saved locally: '+str(snapshot));return 'break'
    ttk.Button(notes_page,text='Save local notes  [Ctrl+S]',command=save_notes).pack(anchor='w',pady=6)
    notes.bind('<Control-s>',lambda _:save_notes())
    appearance_page=pcvr_presentation.page(notebook,'05  ABOUT / DISPLAY')
    appearance['controller'].controls(appearance_page)
    ttk.Checkbutton(appearance_page,text='Background grid accents',variable=appearance['enabled']).pack(anchor='w',pady=8)
    pcvr_presentation.note(appearance_page,'Background dimming')
    ttk.Scale(appearance_page,from_=0,to=100,variable=appearance['dimmer']).pack(fill='x',pady=8)
    pcvr_presentation.note(appearance_page,'PCVR-07 launcher display update. Game runtime and gameplay settings are unchanged. No automatic download, install, upload or game shutdown.\nFrame development remains on hardware hiatus. PCVR headset acceptance is separate.')
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
                        seen.add(value['nonce']);restart.request(child);say('In-game restart requested. Waiting for its normal exit; no game deadline is imposed.')
        except (OSError,ValueError,ImportError):pass
        window.after(1000,restart_tick)
    sys.path.insert(0,str(ROOT/'tools/frame'));window.after(1000,restart_tick)
    ttk.Button(play,text='Cancel pending restart',command=restart.cancel).pack(anchor='w',pady=4)
    def close():
        appearance['controller'].flush();closing.set();restart.cancel();ui.close();window.destroy()
    appearance['controller'].notify=say
    appearance['controller'].apply()
    def restore_split():
        try:panes.sashpos(0,int(panes.winfo_height()*appearance['controller'].pane_ratio))
        except tk.TclError:pass
    def remember_split(event):
        if event.widget==panes and panes.winfo_height()>0:
            appearance['controller'].set_pane(panes.sashpos(0)/panes.winfo_height())
    panes.bind('<ButtonRelease-1>',remember_split,add='+');window.after(120,restore_split)
    window.protocol('WM_DELETE_WINDOW',close);window.mainloop()
if __name__=='__main__':main()
