"""Byte-preserving PCVR scene-capture preferences; no game lifetime controls."""
import os,re,tempfile,time
from pathlib import Path
VALUES={'RenderWindow':'1','CreatorCameraMode':'0','MirrorReuseEye':'true','MirrorFit':'0'}
def patch(data,values=VALUES):
    out=data
    for key,value in values.items():
        pattern=rb'(?m)(^|(?<=\xef\xbb\xbf))'+re.escape(key.encode())+rb'=[^\r\n]*'
        replacement=(key+'='+value).encode('ascii')
        if re.search(pattern,out):out=re.sub(pattern,lambda m:m[1]+replacement,out)
        else:out+=(b'' if out.endswith((b'\n',b'\r')) else b'\r\n')+replacement+b'\r\n'
    return out

def atomic(path,data):
    path=Path(path);fd,tmp=tempfile.mkstemp(prefix='.pcvr09-',dir=path.parent)
    try:
        with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
        os.replace(tmp,path)
    finally:
        if os.path.exists(tmp):os.unlink(tmp)

def update(game,backup_root):
    config=Path(game)/'bin/VR/config.txt';before=config.read_bytes();after=patch(before)
    archive=Path(backup_root)/('pcvr-eye-profile-'+str(time.time_ns()));archive.mkdir(parents=True)
    (archive/'config.txt.before').write_bytes(before)
    if config.read_bytes()!=before:raise RuntimeError('Concurrent preferences edit preserved')
    atomic(config,after)
    if config.read_bytes()!=after:raise RuntimeError('Preference readback mismatch')
    return str(archive)
