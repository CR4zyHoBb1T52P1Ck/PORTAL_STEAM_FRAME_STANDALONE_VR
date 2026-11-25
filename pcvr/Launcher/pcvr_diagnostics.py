"""Read-only Portal PCVR checks. No graphics device, network or process startup."""
import hashlib,json,os,struct
from pathlib import Path

LINKS={
 'GitHub':'https://github.com/CR4zyHoBb1T52P1Ck/PORTAL_STEAM_FRAME_STANDALONE_VR',
 'Releases':'https://github.com/CR4zyHoBb1T52P1Ck/PORTAL_STEAM_FRAME_STANDALONE_VR/releases',
 'Report an issue':'https://github.com/CR4zyHoBb1T52P1Ck/PORTAL_STEAM_FRAME_STANDALONE_VR/issues/new',
 'Ko-fi':'https://ko-fi.com/devdoesdevthings',
 'YouTube':'https://www.youtube.com/@DevDoesDevThings'}

def plain(path,root):
 path=Path(path);root=Path(root).resolve()
 if not path.resolve().is_relative_to(root):raise ValueError('Path leaves selected root')
 for p in (path,*path.parents):
  if p==root:break
  if p.exists() and (p.is_symlink() or getattr(p.lstat(),'st_file_attributes',0)&0x400):raise ValueError('Linked path preserved')
 return path

def binary(path):
 """PE architecture from a small header read; never load a DLL or executable."""
 try:
  with Path(path).open('rb') as f:
   h=f.read(64)
   if len(h)!=64 or h[:2]!=b'MZ':return 'invalid-PE'
   offset=struct.unpack_from('<I',h,60)[0]
   if offset<64 or offset>16*1024*1024:return 'invalid-PE'
   f.seek(offset);h=f.read(6)
   if len(h)!=6 or h[:4]!=b'PE\0\0':return 'invalid-PE'
   return {0x14c:'x86',0x8664:'x64',0xaa64:'ARM64'}.get(struct.unpack_from('<H',h,4)[0],'unknown-PE')
 except OSError:return 'missing-or-unreadable'

def digest(path):
 with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def native_rig_config(path):
 """Read the mode contract without modifying mutable calibration."""
 try:
  rows=[]
  for line in Path(path).read_text(encoding='utf-8-sig').splitlines():
   key,sep,value=line.partition('=')
   if sep and key.strip()=='NativeModels':rows.append(value.strip().lower())
  return dict(enabled=rows==['true'],values=rows)
 except (OSError,UnicodeError):return dict(enabled=False,values=[])

def openvr_registration(local=None):
 p=Path(local or os.environ.get('LOCALAPPDATA',''))/'openvr/openvrpaths.vrpath'
 try:
  if p.stat().st_size>256*1024:raise ValueError('Oversized runtime registry')
  data=json.loads(p.read_text(encoding='utf-8-sig'))
  if not isinstance(data,dict):raise ValueError('Invalid runtime registry')
  runtime=data.get('runtime',[])
  if not isinstance(runtime,list) or any(not isinstance(x,str) for x in runtime):raise ValueError('Invalid runtime list')
  rows=[]
  for x in runtime:
   candidates=[Path(x)/'bin/vrclient.dll',Path(x)/'bin/win32/vrclient.dll']
   chosen=next((c for c in candidates if binary(c)=='x86'),candidates[0])
   rows.append(dict(path=x,x86_client=binary(chosen),client=str(chosen)))
  return dict(file=str(p),runtime=rows,readable=True)
 except (OSError,ValueError,TypeError) as e:return dict(file=str(p),readable=False,error=type(e).__name__,runtime=[])

def inspect(game,manifest,present,local=None):
 game=Path(game).resolve();issues=[];warnings=[];files=[]
 for name,want in manifest['files'].items():
  if name=='bin/VR/config.txt':continue # User preferences are intentionally mutable.
  try:
   p=plain(game/name,game);actual=digest(p);status='match' if actual==want else 'different'
  except (OSError,ValueError):status='missing-or-unreadable';actual=None
  files.append(dict(path=name,status=status,sha256=actual))
  if status!='match':issues.append('Install/update Contact-55: '+name+' ('+status+').')
 for n in ('hl2.exe','bin/d3d9.dll','bin/openvr_api.dll'):
  arch=binary(game/n)
  if arch!='x86':issues.append(n+' must be x86; found '+arch+'.')
 generated=['portal/custom/portal1vr/models/weapons/'+name+'.'+ext for name in ('v_portalgun','v_hands') for ext in ('mdl','vvd','dx90.vtx')]+['bin/VR/gun-shell51.bin']
 generated_files=[]
 for n in generated:
  try:p=plain(game/n,game);valid=p.is_file() and p.stat().st_size>0
  except (OSError,ValueError):valid=False
  expected=manifest.get('generated_hashes',{}).get(n)
  if valid and expected:
   try:valid=digest(p)==expected
   except OSError:valid=False
  generated_files.append(dict(path=n,present=valid,expected_sha256=expected))
  if not valid:issues.append('Run Install/update to generate owned assets: '+n+'.')
 config=game/'bin/VR/config.txt'
 if not config.is_file():issues.append('Install/update to create VR settings.')
 rig=native_rig_config(config)
 if manifest.get('requires_native_rig') and not rig['enabled']:issues.append('Native Portal rig is disabled or missing. Set NativeModels=true in bin/VR/config.txt; legacy mode omits native gun contact, grip and hand masking.')
 busy=sorted(set(present)&{'hl2.exe','hl2vr.exe','portal.exe','portal2.exe'})
 if busy:issues.append('An existing Portal session is open; finish it yourself before Play/install.')
 steam='steam.exe' in present;vr={'vrserver.exe','vrcompositor.exe'}<=set(present)
 if not steam:issues.append('Start Steam before Play.')
 if not vr:issues.append('Start SteamVR with the dedicated button when ready, then Check setup.')
 registry=openvr_registration(local)
 if not any(x['x86_client']=='x86' for x in registry['runtime']):warnings.append('Registered x86 OpenVR client not confirmed. Repair/check SteamVR registration if startup fails.')
 warnings.append('Graphics-driver/headset readiness is not tested by this CPU-only check.')
 return dict(build=manifest['build'],tools_revision=manifest.get('tools_revision'),game=str(game),dll_matches=all(x['status']=='match' for x in files if x['path']=='bin/d3d9.dll'),steam_running=steam,steamvr_ready=vr,running_games=busy,files=files,generated_files=generated_files,openvr=registry,blocking_issues=issues,warnings=warnings,ready_for_owner_launch=not issues,game_launch_authorized=False,game_lifetime_limit=None,headset_accepted=False,visual_quality_changed=False)

def storage(root):
 import shutil
 p=Path(root).resolve();disk=shutil.disk_usage(p)
 return dict(root=str(p),free_bytes=disk.free,total_bytes=disk.total,temporary=str(p/'Temp'),diagnostics=str(p/'PCVR-Logs'),native_cache=str(p/'Cache'),game_relative_logs='Source/native logs may also remain in the selected game folder; collected after exit.',installed_runtime_exception='SteamVR and vendor driver global stores retain their installed NTFS/user-profile locations. No shared cache or credentials are relocated.')
