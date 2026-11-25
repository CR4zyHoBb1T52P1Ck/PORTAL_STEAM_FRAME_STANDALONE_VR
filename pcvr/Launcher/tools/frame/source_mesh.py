"""Small bounds-checked reader for the retail Portal MDL/VVD/DX90 VTX tuple.

Formats: Valve source-sdk-2013 public/studio.h and public/optimize.h.
No Valve mesh data is embedded in this module.
"""
import struct
from pathlib import Path

def unpack(data,fmt,at):
    size=struct.calcsize(fmt)
    if at<0 or at+size>len(data):raise ValueError('Mesh record outside file')
    return struct.unpack_from(fmt,data,at)
def integer(data,at):return unpack(data,'<i',at)[0]
def string(data,at):
    if at<0 or at>=len(data):raise ValueError('Mesh string outside file')
    return data[at:data.index(b'\0',at)].decode('utf8')
def mdl_model(data):
    assert data[:4]==b'IDST' and integer(data,232)==1
    body=integer(data,236);assert integer(data,body+4)==1
    return body+integer(data,body+12)
def vvd_vertices(data):
    assert data[:4]==b'IDSV' and integer(data,4)==4 and integer(data,12)==1
    assert integer(data,48)==0,'Only verified single-LOD/no-fixup assets are accepted'
    n=integer(data,16);start=integer(data,56);tangent=integer(data,60)
    assert start>=64 and tangent>=start+n*48 and tangent+n*16<=len(data)
    return [dict(record=data[start+48*i:start+48*(i+1)],position=unpack(data,'<3f',start+48*i+16),
        normal=unpack(data,'<3f',start+48*i+28),uv=unpack(data,'<2f',start+48*i+40),
        tangent=unpack(data,'<4f',tangent+16*i)) for i in range(n)]
def vtx_lod(data):
    assert integer(data,0)==7 and integer(data,20)==1 and integer(data,28)==1
    body=integer(data,32);assert integer(data,body)==1
    model=body+integer(data,body+4);assert integer(data,model)==1
    return model+integer(data,model+4)
def triangles(mdl,vtx):
    model=mdl_model(mdl);meshes=integer(mdl,model+72);lod=vtx_lod(vtx)
    assert integer(vtx,lod)==meshes
    for m in range(meshes):
        mh=model+integer(mdl,model+76)+116*m
        base=integer(mdl,mh+12);material=integer(mdl,mh)
        vm=lod+integer(vtx,lod+4)+9*m
        for g in range(integer(vtx,vm)):
            sg=vm+integer(vtx,vm+4)+25*g
            nv,vo,ni,io,ns,so=unpack(vtx,'<6i',sg)
            assert nv>=0 and ni>=0 and ns>=0
            verts=[unpack(vtx,'<H',sg+vo+9*i+4)[0]+base for i in range(nv)]
            indices=unpack(vtx,'<'+str(ni)+'H',sg+io)
            for s in range(ns):
                strip=sg+so+27*s
                count,offset=unpack(vtx,'<2i',strip);flag=vtx[strip+18]
                ids=indices[offset:offset+count];assert len(ids)==count
                if flag&1:
                    assert count%3==0
                    groups=[ids[k:k+3] for k in range(0,count,3)]
                elif flag&2:groups=[(ids[k+1],ids[k],ids[k+2]) if k%2 else ids[k:k+3] for k in range(count-2)]
                else:raise ValueError('Unsupported strip flag')
                for tri in groups:
                    if len(set(tri))==3:yield material,tuple(verts[i] for i in tri)
def inverse_bind(mdl,bone):
    off=integer(mdl,160)+216*bone+96
    a=unpack(mdl,'<12f',off)
    return tuple(tuple(a[4*r+c] for c in range(4)) for r in range(3))
def point(matrix,p):return tuple(sum(matrix[r][c]*p[c] for c in range(3))+matrix[r][3] for r in range(3))
def rigid_inverse(matrix):
    return tuple(tuple(matrix[c][r] for c in range(3))+(-sum(matrix[c][r]*matrix[c][3] for c in range(3)),) for r in range(3))
def read_tuple(folder,name):return tuple((Path(folder)/(name+'.'+ext)).read_bytes() for ext in ('mdl','vvd','dx90.vtx'))
def bounds(points):return tuple((min(p[c] for p in points),max(p[c] for p in points)) for c in range(3))

if __name__=='__main__':
    import sys,json
    for name in ('v_portalgun','w_portalgun','v_hands'):
        mdl,vvd,vtx=read_tuple(sys.argv[1],name);verts=vvd_vertices(vvd);tris=list(triangles(mdl,vtx))
        print(name,'verts',len(verts),'triangles',len(tris),'bounds',bounds([v['position'] for v in verts]))
        if name=='v_portalgun':
            local=inverse_bind(mdl,24);selected={i for material,tri in tris if material==1 for i in tri}
            print('Gun base local bounds',bounds([point(local,verts[i]['position']) for i in selected]),'model-to-base',local)
