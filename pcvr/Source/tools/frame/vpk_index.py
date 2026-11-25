"""Read Source VPK directory entries without extracting or executing content."""
import struct
from pathlib import Path

def entries(path):
    with Path(path).open('rb') as stream:
        magic,version,size=struct.unpack('<3I',stream.read(12))
        assert magic==0x55aa1234 and version in (1,2) and size<64*1024*1024
        header=12 if version==1 else 28
        stream.seek(header);tree=stream.read(size)
    pos=0
    def word():
        nonlocal pos
        end=tree.index(b'\0',pos);value=tree[pos:end].decode('utf-8');pos=end+1;return value
    result={}
    while True:
        extension=word()
        if not extension:break
        while True:
            folder=word()
            if not folder:break
            while True:
                name=word()
                if not name:break
                crc,preload,archive,offset,length,terminator=struct.unpack_from('<I2H2IH',tree,pos)
                pos+=18;assert terminator==0xffff
                prefix=tree[pos:pos+preload];pos+=preload
                relative=(folder+'/' if folder!=' ' else '')+name+('.'+extension if extension!=' ' else '')
                result[relative]=dict(crc=crc,archive=archive,offset=offset,length=length,preload=prefix,inline=header+size)
    return result

def read(path,item):
    path=Path(path)
    if item['archive']==0x7fff:
        offset=item['inline']+item['offset']
    else:
        assert path.name.endswith('_dir.vpk')
        path=path.with_name(path.name[:-8]+f"_{item['archive']:03}.vpk");offset=item['offset']
    with path.open('rb') as stream:stream.seek(offset);data=stream.read(item['length'])
    assert len(data)==item['length']
    return item['preload']+data
