"""Bounded, Portal-only local binding evidence. No runtime connection or upload."""
import json,os
from pathlib import Path

def capture(folder,prefix=''):
 try:
  registry=Path(os.environ.get('LOCALAPPDATA',''))/'openvr/openvrpaths.vrpath'
  data=json.loads(registry.read_text(encoding='utf-8-sig'))
  if not isinstance(data,dict):return None
  roots=data.get('log',[])
  if not isinstance(roots,list):return None
  rows=[]
  for raw in roots[:2]:
   if not isinstance(raw,str):continue
   root=Path(raw)
   for name in ('vrserver.txt','vrclient_hl2.txt'):
    p=root/name
    if not p.is_file() or p.is_symlink():continue
    with p.open('rb') as f:f.seek(max(0,p.stat().st_size-1024*1024));text=f.read(1024*1024).decode('utf-8',errors='replace')
    lines=[line for line in text.splitlines() if 'steam.app.400' in line or ('SetActionManifest' in line and '\\portal\\' in line.lower()) or 'bindings_knuckles_pcvr02.json' in line]
    rows.append(dict(source=name,source_bytes=p.stat().st_size,lines=lines[-64:]))
  output=Path(folder)/(prefix+'steamvr-input.json');output.write_text(json.dumps(dict(scope='Portal binding lines only; tail may include older sessions; correlate timestamps',logs=rows,uploaded=False),indent=2)+'\n');return output.name
 except (OSError,ValueError,TypeError):return None
