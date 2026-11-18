"""Install Portal Frame VR into an owned Windows Portal copy; never launch/stop games."""
import argparse,datetime,hashlib,json,os,shutil,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'tools/frame'))
from generate_owned_assets import build
from portal_startup_settings import startup_text,autoexec_text
def digest(data):return hashlib.sha256(data).hexdigest()
def active():
 names=set()
 if os.name=='nt':
  r=subprocess.run(['tasklist','/fo','csv','/nh'],capture_output=True,text=True,check=True,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
  import csv
  names={row[0].lower() for row in csv.reader(r.stdout.splitlines()) if row}
 else:
  for p in Path('/proc').glob('[0-9]*/comm'):
   try:names.add(p.read_text().strip().lower())
   except OSError:pass
 if names&{'hl2.exe','hl2vr.exe','hl2','portal.exe','portal2.exe'}:raise RuntimeError('Close Portal yourself before installing. No application was stopped.')
def safe(game,name):
 p=game/name
 if not p.resolve().is_relative_to(game):raise RuntimeError('Path escapes Portal')
 for q in (p,*p.parents):
  if q==game.parent:break
  if q.is_symlink():raise RuntimeError('Linked installation path preserved: '+str(q))
 return p
def atomic(path,data):
 path.parent.mkdir(parents=True,exist_ok=True)
 fd,temp=tempfile.mkstemp(prefix='.portal-frame-',dir=path.parent)
 try:
  with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
  os.replace(temp,path)
 finally:
  if os.path.exists(temp):os.unlink(temp)
def install(game,dry=False):
 game=Path(game).resolve();active()
 if not (game/'hl2.exe').is_file() or not (game/'portal').is_dir():raise RuntimeError('Select the Windows Steam Portal folder containing hl2.exe and portal/')
 manifest=json.loads((ROOT/'manifest.json').read_text());updates={}
 for name,sha in manifest['files'].items():
  data=(ROOT/'payload'/name).read_bytes()
  if digest(data)!=sha:raise RuntimeError('Package checksum mismatch: '+name)
  updates[name]=data
 config='bin/VR/config.txt';target=safe(game,config)
 old=target.read_bytes() if target.exists() else b''
 # Existing preferences/comments survive; append only missing generic defaults.
 keys={line.partition('=')[0].strip() for line in old.decode('utf-8-sig').splitlines() if '=' in line}
 missing=[line for line in updates[config].decode().splitlines() if '=' in line and line.partition('=')[0].strip() not in keys]
 updates[config]=old+(b'\n' if old and not old.endswith(b'\n') else b'')+('\n'.join(missing)+'\n').encode()
 auto='portal/cfg/autoexec.cfg';p=safe(game,auto);original=p.read_bytes() if p.exists() else b''
 updates[auto]=autoexec_text(original.decode('latin1')).encode('latin1')
 updates['portal/cfg/portal_frame_startup.cfg']=startup_text(updates[config].decode('utf-8-sig')).encode()
 with tempfile.TemporaryDirectory(prefix='portal-frame-models-') as temp:
  output=Path(temp)/'generated';build(game,output)
  for p in (output/'models').rglob('*'):
   if p.is_file():updates['portal/custom/portal1vr/'+p.relative_to(output).as_posix()]=p.read_bytes()
 for name in updates:safe(game,name)
 if dry:return {'status':'validated-dry-run','files':len(updates),'game_changed':False,'game_launched':False}
 active()
 stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
 archive=safe(game,'.portal-frame-backups/'+stamp);archive.mkdir(parents=True)
 before={name:safe(game,name).read_bytes() if safe(game,name).exists() else None for name in updates}
 journal={'version':manifest.get('version','unknown'),'before':{k:None if v is None else digest(v) for k,v in before.items()},'after':{k:digest(v) for k,v in updates.items()},'status':'prepared'}
 for name,data in before.items():
  if data is not None:p=archive/'files'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
 log=archive/'installation.json';log.write_text(json.dumps(journal,indent=2));written=[]
 try:
  for name,data in updates.items():
   p=safe(game,name)
   if (p.read_bytes() if p.exists() else None)!=before[name]:raise RuntimeError('File changed during installation: '+name)
   atomic(p,data);written.append(name)
  journal['status']='installed'
 except BaseException:
  for name in reversed(written):
   p=safe(game,name)
   if digest(p.read_bytes())!=journal['after'][name]:continue
   if before[name] is None:p.unlink()
   else:atomic(p,before[name])
  journal['status']='rolled-back';raise
 finally:log.write_text(json.dumps(journal,indent=2))
 return {'status':'installed','files':len(updates),'backup':str(archive),'game_launched':False}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--portal-dir',required=True);p.add_argument('--dry-run',action='store_true');a=p.parse_args()
 print(json.dumps(install(a.portal_dir,a.dry_run),indent=2))
if __name__=='__main__':main()
