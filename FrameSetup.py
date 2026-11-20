"""Prepare the local standalone Steam launcher on Frame. Never launches a game."""
import argparse,datetime,hashlib,json,os,shlex,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parent

def active():
 for p in Path('/proc').glob('[0-9]*/comm'):
  try:name=p.read_text().strip().lower()
  except OSError:continue
  if name in {'hl2.exe','hl2vr.exe','hl2','portal.exe','portal2.exe'}:raise RuntimeError('Close Portal yourself before configuring its launcher; no game was stopped.')

def safe(path):
 for p in (path,*path.parents):
  if p.is_symlink():raise RuntimeError('Linked output path preserved: '+str(p))

def settings(game,setup):
 folder=setup/'Standalone-Launch'
 env='WINEDLLOVERRIDES="d3d9=n,b" WINEDEBUG="-all" '
 prefix=['/usr/bin/python3',str(folder/'standalone_launch.py'),'--']
 args=['--portal-root','Z:'+str(game),'-steam','-game','portal','-insecure','-window','-novid','-dx9',
       '-width','1280','-height','720','+mat_queue_mode','0','+mat_vsync','0','+mat_antialias','0','-condebug']
 return dict(executable=str(folder/'portal-standalone.exe'),working_directory=str(folder),
             launch_options=env+' '.join(map(shlex.quote,prefix))+' %command% '+' '.join(map(shlex.quote,args)),
             compatibility='Proton 11 ARM64 / your working ARM64 Proton-FEX version',vr_library=True)

def configure(game,setup=None,dry=False):
 if sys.platform=='win32':raise RuntimeError('Run FrameSetup.py on the Frame after transferring the patched Windows Portal folder. Use Install.py on Windows.')
 game=Path(game).expanduser().resolve();setup=Path(setup).expanduser().absolute() if setup else game.with_name(game.name+'_FrameVR_Setup')
 active();safe(setup)
 if not (game/'hl2.exe').is_file() or not (game/'bin/VR/config.txt').is_file():raise RuntimeError('Run Install.py against this owned Windows Portal folder first.')
 if setup==game or setup.is_relative_to(game) or game.is_relative_to(setup):raise RuntimeError('The launch setup folder must be separate from the game folder.')
 manifest=json.loads((ROOT/'manifest.json').read_text());sha=hashlib.sha256((game/'bin/d3d9.dll').read_bytes()).hexdigest()
 if sha!=manifest['files']['bin/d3d9.dll']:raise RuntimeError('Installed DLL does not match this release; run Install.py first.')
 for name in ('bin/VR/appid_guard.py','bin/VR/portal_startup_settings.py','bin/VR/portal_restart_helper.py'):
  if hashlib.sha256((game/name).read_bytes()).hexdigest()!=manifest['files'][name]:raise RuntimeError('Installed launch helper differs from this release; run Install.py first: '+name)
 inputs={}
 for name,want in manifest['launcher_files'].items():
  p=ROOT/name
  if not p.resolve().is_relative_to(ROOT):raise RuntimeError('Package path escapes release folder')
  data=p.read_bytes()
  if hashlib.sha256(data).hexdigest()!=want:raise RuntimeError('Launcher package checksum mismatch: '+name)
  inputs[Path(name).name]=data
 folder=setup/'Standalone-Launch';safe(folder)
 if (folder/'steam_appid.txt').exists():raise RuntimeError('A retail app-ID marker is present in the launch folder. Preserve it and choose a clean --setup-dir.')
 profile=dict(version=1,game=str(game),setup=str(setup),uri='auto',revision=manifest['build'],dll_sha256=sha)
 if (folder/'profile.json').is_file():
  old=json.loads((folder/'profile.json').read_text())
  if old.get('game')!=str(game):raise RuntimeError('Existing launch folder belongs to a different game; preserved.')
 updates={folder/'portal-standalone.exe':inputs['portal-standalone.exe'],folder/'standalone_launch.py':inputs['standalone_launch.py'],
          folder/'profile.json':(json.dumps(profile,indent=2)+'\n').encode()}
 steam=settings(game,setup)
 guide=('PORTAL FRAME VR: LOCAL STEAM SHORTCUT\n\nExecutable: '+steam['executable']+'\nWorking directory: '+steam['working_directory']+
        '\nCompatibility: '+steam['compatibility']+'\nMark as VR / include in VR library.\n\nLaunch options (one line):\n'+steam['launch_options']+
        '\n\nKeep the executable and working directory above: do not target hl2.exe directly.\nNo PC or Wi-Fi is required after setup. Quit/Steam Stop ends the game; no automatic deadline.\n')
 updates[setup/'Steam-Shortcut-Settings.txt']=guide.encode()
 if dry:return dict(status='validated-dry-run',steam_shortcut=steam,game_launched=False,game_changed=False)
 active();folder.mkdir(parents=True,exist_ok=True)
 stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ');backup=setup/'Setup-Backups'/stamp
 before={p:p.read_bytes() if p.exists() else None for p in updates}
 backup.mkdir(parents=True)
 for p,data in before.items():
  safe(p)
  if data is not None:
   b=backup/p.relative_to(setup);b.parent.mkdir(parents=True,exist_ok=True);b.write_bytes(data)
 def atomic(p,data):
  fd,temp=tempfile.mkstemp(prefix='.portal-local-',dir=p.parent)
  try:
   with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
   os.replace(temp,p)
  finally:
   if os.path.exists(temp):os.unlink(temp)
 written=[]
 try:
  for p,data in updates.items():
   if (p.read_bytes() if p.exists() else None)!=before[p]:raise RuntimeError('Launcher changed concurrently; preserved')
   atomic(p,data);written.append(p)
 except BaseException:
  for p in reversed(written):
   if before[p] is None:p.unlink()
   else:atomic(p,before[p])
  raise
 return dict(status='launcher-installed-configure-steam-shortcut',steam_shortcut=steam,settings_file=str(setup/'Steam-Shortcut-Settings.txt'),
             logs=str(folder/'Attempts'),backup=str(backup),pc_required_to_play=False,game_launched=False,game_settings_preserved=True)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--portal-dir',required=True);p.add_argument('--setup-dir');p.add_argument('--dry-run',action='store_true');a=p.parse_args()
 print(json.dumps(configure(a.portal_dir,a.setup_dir,a.dry_run),indent=2))
