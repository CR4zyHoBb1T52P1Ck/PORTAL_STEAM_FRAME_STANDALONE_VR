"""Apply resource settings before Source starts, never while a map is running."""
import hashlib
import os
from pathlib import Path
import re
import tempfile

MARKER='// Portal Frame VR startup resource settings (managed)'
COMMAND='exec portal_frame_startup.cfg'

def startup_text(config):
    settings={}
    for line in config.splitlines():
        key,sep,value=line.partition('=')
        if sep:settings[key.strip()]=value.partition('#')[0].strip()
    def integer(key,default,allowed):
        try:value=int(settings.get(key,str(default)))
        except ValueError:value=default
        return value if value in allowed else default
    texture=integer('TextureQuality',0,(0,1,2))
    filtering=integer('AnisotropicFiltering',8,(1,2,4,8,16))
    return (MARKER+'\n// Loaded once by autoexec, before a map is opened.\n'
        f'mat_picmip {texture}\nmat_forceaniso {filtering}\n'
        'mat_vsync 0\nmat_viewportscale 1\nmat_viewportupscale 0\n')

def autoexec_text(original):
    # Preserve every existing command and comment. Never add a second exec.
    if re.search(r'^\s*exec\s+"?portal_frame_startup(?:\.cfg)?"?\s*(?://.*)?$',original,re.M|re.I):
        return original
    return original+('' if not original or original.endswith('\n') else '\n')+'\n'+MARKER+'\n'+COMMAND+'\n'

def apply_startup_settings(game,setup,active_names=()):
    if set(n.lower() for n in active_names)&{'hl2.exe','hl2vr.exe','portal.exe','portal2.exe'}:
        raise RuntimeError('Portal must be closed before applying startup resource settings')
    game=Path(game);setup=Path(setup)
    paths=[game/'bin/VR/config.txt',game/'portal/cfg/autoexec.cfg',game/'portal/cfg/portal_frame_startup.cfg']
    for p in paths:
        assert p.resolve().is_relative_to(game.resolve()),'Setting path escapes Portal'
        for q in [p,*p.parents]:
            if q==game.parent:break
            assert not q.is_symlink(),'Refuse linked Portal setting'
    config=paths[0].read_text(encoding='utf-8-sig')
    autoexec=paths[1].read_bytes() if paths[1].exists() else b''
    # ASCII additions preserve the user's encoding and all other bytes.
    current=autoexec.decode('latin1')
    updates={paths[1]:autoexec_text(current).encode('latin1'),paths[2]:startup_text(config).encode('ascii')}
    changed={p:data for p,data in updates.items() if not p.exists() or p.read_bytes()!=data}
    if not changed:return dict(changed=[],startup_sha256=hashlib.sha256(updates[paths[2]]).hexdigest())
    archive=setup/'Startup-Settings';assert not setup.is_symlink() and not archive.is_symlink()
    archive.mkdir(exist_ok=True)
    before={p:p.read_bytes() if p.exists() else None for p in changed}
    for p,data in before.items():
        if data is None:continue
        backup=archive/(p.name+'.'+hashlib.sha256(data).hexdigest()+'.before')
        assert not backup.is_symlink()
        if not backup.exists():backup.write_bytes(data)
        assert backup.read_bytes()==data
    def atomic(p,data):
        fd,temp=tempfile.mkstemp(prefix='.portal-startup-',dir=p.parent)
        try:
            with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
            os.replace(temp,p)
        finally:
            if os.path.exists(temp):os.unlink(temp)
    written=[]
    try:
        # Write settings before enabling autoexec's reference to them.
        for p in reversed(paths[1:]):
            if p not in changed:continue
            assert (p.read_bytes() if p.exists() else None)==before[p],'Setting changed concurrently'
            atomic(p,changed[p]);written.append(p)
    except BaseException:
        for p in reversed(written):
            if before[p] is None:p.unlink()
            else:atomic(p,before[p])
        raise
    return dict(changed=[str(p.relative_to(game)) for p in changed],startup_sha256=hashlib.sha256(updates[paths[2]]).hexdigest())
