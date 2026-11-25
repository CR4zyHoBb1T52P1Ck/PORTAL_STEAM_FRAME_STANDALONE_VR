"""Preserve Portal's generated app-ID marker before a devkit launch."""
import os
import time
import uuid


def archive_generated_appid(game, setup):
    source=game/'steam_appid.txt'
    if not source.exists():
        return None
    if source.is_symlink():
        raise RuntimeError('Linked app-ID file preserved; inspect before launching')
    data=source.read_bytes()
    if data.rstrip(b'\0\r\n ')!=b'400':
        raise RuntimeError('Unexpected app-ID file preserved; inspect before launching')
    archive=setup/'Generated-AppIds'
    archive.mkdir(parents=True,exist_ok=True)
    destination=archive/('steam_appid-before-launch-'+str(time.time_ns())+'-'+uuid.uuid4().hex[:8]+'.txt')
    os.rename(source,destination)
    return str(destination)
