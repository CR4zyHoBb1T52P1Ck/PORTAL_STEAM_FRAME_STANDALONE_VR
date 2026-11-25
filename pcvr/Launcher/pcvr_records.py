"""Local PCVR attempt records and bounded diagnostics. No automatic uploads."""
import datetime,hashlib,json,os,subprocess,tempfile,time,zipfile
from pathlib import Path
RUNTIME_LOGS=('portalvr.log','hl2_d3d9.log','portal/console.log')
VISUAL_KEYS={'EyeRenderWidth','AntiAliasing','SpatialUpscaler','UpscaleOutputWidth','UpscaleSharpness','RenderWindow','MirrorReuseEye','MirrorEye','MirrorFit','CreatorCameraMode','DetailedGpuTiming','FrameTimingLog','PortalRenderDepth','LegacyScreenEffects','TextureQuality','AnisotropicFiltering','DynamicShadows','MotionBlur','Bloom','RenderScalePercent'}

def identity(game):return hashlib.sha256(os.path.normcase(str(Path(game).resolve())).encode('utf-8')).hexdigest()
def atomic_json(path,value):
 path=Path(path)
 if path.is_symlink():raise RuntimeError('Linked record path preserved')
 fd,temp=tempfile.mkstemp(prefix='.pcvr-record-',dir=path.parent)
 try:
  with os.fdopen(fd,'w',encoding='utf-8') as f:json.dump(value,f,indent=2);f.write('\n');f.flush();os.fsync(f.fileno())
  os.replace(temp,path)
 finally:
  if os.path.exists(temp):os.unlink(temp)
def tail(path,root,limit=1024*1024):
 path=Path(path);root=Path(root).resolve()
 if not path.resolve().is_relative_to(root):raise RuntimeError('Log path escapes selected root')
 for item in (path,*path.parents):
  if item==root:break
  if item.is_symlink():raise RuntimeError('Linked log path preserved')
 if not path.is_file():return None
 with path.open('rb') as f:f.seek(max(0,path.stat().st_size-limit));return f.read(limit)
def capture_logs(game,folder,prefix):
 copied=[]
 for name in RUNTIME_LOGS:
  try:
   data=tail(Path(game)/name,game)
   if data is not None:(folder/(prefix+Path(name).name)).write_bytes(data);copied.append(name)
  except (OSError,RuntimeError):continue
 try:
  import pcvr_steamvr_evidence
  evidence=pcvr_steamvr_evidence.capture(folder,prefix)
  if evidence:copied.append(evidence)
 except (ImportError,OSError,ValueError):pass
 return copied

class StartupFailure(RuntimeError):pass
class StartupCancelled(RuntimeError):pass

def latest_attempt(game,attempts):
 attempts=Path(attempts);game_id=identity(game)
 if not attempts.is_dir():return attempts
 for folder in sorted(attempts.iterdir(),key=lambda p:p.name,reverse=True)[:500]:
  if not folder.is_dir() or folder.is_symlink():continue
  try:
   raw=tail(folder/'result.json',attempts,64*1024)
   if raw and json.loads(raw).get('game_identity')==game_id:return folder
  except (OSError,ValueError,RuntimeError,AttributeError):continue
 return attempts
def start(game,manifest,attempts,validate,arguments,take=None,popen=None,cancelled=None):
 """Only called by explicit Play. Persist failures before creating a child."""
 game=Path(game);attempts=Path(attempts);attempts.mkdir(parents=True,exist_ok=True)
 folder=attempts/(datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')+'-'+str(time.time_ns()))
 folder.mkdir();record=dict(build=manifest['build'],game_identity=identity(game),
  started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),outcome='initializing',stage='runtime-checks',
  game_launch_authorized=True,game_started=False,lifetime_limit=None,mode='replay' if take else 'play',tools_revision=manifest.get('tools_revision','legacy'))
 atomic_json(folder/'result.json',record);log=(folder/'launcher.log').open('a',encoding='utf-8');child=None
 def cancellation():
  if cancelled is not None and cancelled():raise StartupCancelled('Startup cancelled before Portal was started.')
 try:
  cancellation()
  check=validate(game,manifest);record['startup_checks']=check
  if check.get('blocking_issues'):raise RuntimeError('Setup checks need attention:\n'+'\n'.join(check['blocking_issues']))
  if check['running_games']:raise RuntimeError('Portal is already running; finish that session yourself.')
  if not check['dll_matches']:raise RuntimeError('Install this build first; DLL checksum differs.')
  if not check['steam_running'] or not check['steamvr_ready']:raise RuntimeError('Start Steam and SteamVR first, then press Play when ready.')
  record['stage']='command-validation';args=arguments(game,take)
  record['previous_logs']=capture_logs(game,folder,'previous-');record['stage']='process-start';atomic_json(folder/'result.json',record)
  env=os.environ.copy();state=attempts.parent;temporary=state/'Temp';cache=state/'Cache';native=folder/'native'
  for p in (temporary,cache,native):p.mkdir(parents=True,exist_ok=True)
  env.update(SteamAppId='400',SteamGameId='400',TEMP=str(temporary),TMP=str(temporary),DXVK_STATE_CACHE_PATH=str(cache),DXVK_LOG_PATH=str(native))
  record['storage']=dict(temporary=str(temporary),dxvk_cache=str(cache),native_logs=str(native));atomic_json(folder/'result.json',record)
  cancellation()
  child=(popen or subprocess.Popen)(args,cwd=game,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
  record.update(pid=child.pid,stage='observing',outcome='running',game_started=True)
  try:atomic_json(folder/'result.json',record)
  except Exception as e:record['logging_error']=str(e) # A log failure must not discard a running child.
  return child,record,folder,log
 except Exception as e:
  record.update(outcome='user-cancelled' if isinstance(e,StartupCancelled) and child is None else 'initialization-failure' if child is None else 'observer-setup-failed-game-left-running',error_type=type(e).__name__,error=str(e))
  try:atomic_json(folder/'result.json',record)
  finally:log.close()
  error=StartupCancelled if isinstance(e,StartupCancelled) else StartupFailure
  raise error(str(e)+'\nAttempt record: '+str(folder)) from e

def export(game,attempts,destination,summary,budget=8*1024*1024):
 """Create a local ZIP, matching attempts by game identity and capping bytes."""
 if budget<0 or budget>64*1024*1024:raise ValueError('Invalid diagnostic byte budget')
 game=Path(game);attempts=Path(attempts);dest=Path(destination);omitted=[];used=0;game_id=identity(game)
 replacements=[]
 for path,marker in ((str(game),b'<PORTAL>'),(str(game).replace('\\','/'),b'<PORTAL>'),(str(Path.home()),b'<USER>')):
  replacements.append((path.encode(),marker));replacements.append((json.dumps(path,ensure_ascii=False)[1:-1].encode(),marker))
 def scrub(data):
  for before,after in sorted(replacements,key=lambda p:len(p[0]),reverse=True):data=data.replace(before,after)
  return data
 with zipfile.ZipFile(dest,'x',zipfile.ZIP_DEFLATED) as z:
  z.writestr('summary.json',scrub(json.dumps(summary,indent=2).encode()));z.writestr('REVIEW-BEFORE-SHARING.txt','Local export only. Known selected-game/home paths are replaced; other personal information may remain. Review before sharing. Nothing was uploaded. Older attempts without matching game identity are omitted.\n')
  def add(path,root,name,limit=512*1024):
   nonlocal used
   if used>=budget:omitted.append(dict(name=name,reason='byte-budget'));return
   try:data=tail(path,root,min(limit,budget-used))
   except (OSError,RuntimeError) as e:omitted.append(dict(name=name,reason=type(e).__name__));return
   if data is None:return
   data=scrub(data);z.writestr(name,data);used+=len(data)
  for name in RUNTIME_LOGS:add(game/name,game,'current/'+Path(name).name)
  try:config=tail(game/'bin/VR/config.txt',game,128*1024)
  except (OSError,RuntimeError):config=None;omitted.append(dict(name='visual-settings.txt',reason='unreadable-or-linked'))
  if config:
   lines=[line for line in config.decode('utf-8-sig',errors='replace').splitlines() if line.partition('=')[0].strip() in VISUAL_KEYS]
   z.writestr('visual-settings.txt',scrub(('\n'.join(lines)+'\n').encode()))
  matching=[]
  if attempts.is_dir():
   for folder in attempts.iterdir():
    if not folder.is_dir() or folder.is_symlink():continue
    try:
     raw=tail(folder/'result.json',attempts,64*1024)
     if raw is None:continue
     record=json.loads(raw)
     if record.get('game_identity')==game_id:matching.append(folder)
    except (OSError,ValueError,RuntimeError,AttributeError):continue
  for ordinal,folder in enumerate(sorted(matching,key=lambda p:p.name,reverse=True)[:3]):
   for name in ('result.json','launcher.log','previous-steamvr-input.json','exit-steamvr-input.json',*(prefix+Path(n).name for prefix in ('previous-','exit-') for n in RUNTIME_LOGS)):
    add(folder/name,attempts,'attempts/'+str(ordinal)+'/'+name)
   native=folder/'native'
   if native.is_dir() and not native.is_symlink():
    for item in sorted(native.glob('*.log'))[:4]:add(item,attempts,'attempts/'+str(ordinal)+'/native/'+item.name)
  z.writestr('export-coverage.json',json.dumps(dict(matching_attempts=len(matching),included_attempt_limit=3,log_bytes=used,log_byte_budget=budget,omitted=omitted,uploaded=False),indent=2))
 return dest
