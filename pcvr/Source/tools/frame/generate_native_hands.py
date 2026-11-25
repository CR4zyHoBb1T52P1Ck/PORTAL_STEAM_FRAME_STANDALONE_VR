"""Retarget the owned Chell arm mesh/UVs to both native VR hand chains.

The retail standalone v_hands mesh uses an HEV glove atlas. Reusing Chell's
texture alone is insufficient: generate matching geometry, UVs and weights.
No game asset data is embedded in this generator.
"""
import hashlib,json,math,struct
from source_mesh import *

def create_group(records,indices,bones):
    count=len(records);assert count<65536 and bones<=53
    vo=25;io=vo+count*9;so=io+len(indices)*2
    data=bytearray(struct.pack('<6iB',count,vo,len(indices),io,1,so,2))
    for i,record in enumerate(records):
        num=record[15];ids=record[12:15];assert 1<=num<=3 and all(b<bones for b in ids[:num])
        data+=struct.pack('<4BH3B',0,1,2,num,i,*ids)
    data+=struct.pack('<'+str(len(indices))+'H',*indices)
    data+=struct.pack('<4ihB2i',len(indices),0,count,0,bones,1,bones,27)
    for b in range(bones):data+=struct.pack('<2i',b,b)
    return data

def generate(gun,hands):
    gm,gv,gt=gun;hm,hv,ht=map(bytearray,hands)
    assert integer(gm,156)==45 and integer(hm,156)==43
    assert integer(hm,8)==integer(hv,8)==integer(ht,16)
    gunverts=vvd_vertices(gv);bare=vvd_vertices(hv)
    faces=[tri for mat,tri in triangles(gm,gt) if mat==0];ids=sorted({n for tri in faces for n in tri})
    assert len(ids)==990
    remap={n:i for i,n in enumerate(ids)};vertices=[];tangents=[];indices=[]
    for left in (True,False):
        start=len(vertices)
        def mapping(b):return b if left or b<5 else b+19
        for n in ids:
            source=gunverts[n];weights=unpack(source['record'],'<3f',0);num=source['record'][15];sourcebones=source['record'][12:15]
            position=[0.,0.,0.];normal=[0.,0.,0.];tangent=[0.,0.,0.]
            for j in range(num):
                bone=sourcebones[j];inv=inverse_bind(gm,bone);dest=rigid_inverse(inverse_bind(hm,mapping(bone)))
                for value,acc,translate in ((source['position'],position,True),(source['normal'],normal,False),(source['tangent'][:3],tangent,False)):
                    local=tuple(sum(inv[r][c]*value[c] for c in range(3))+(inv[r][3] if translate else 0) for r in range(3))
                    if left:local=(local[0],local[1],-local[2])
                    result=tuple(sum(dest[r][c]*local[c] for c in range(3))+(dest[r][3] if translate else 0) for r in range(3))
                    for c in range(3):acc[c]+=result[c]*weights[j]
            for v in (normal,tangent):
                length=math.sqrt(sum(x*x for x in v));assert length>1e-8
                for c in range(3):v[c]/=length
            bones=[mapping(b) for b in sourcebones]
            vertices.append(struct.pack('<3f4B3f3f2f',*weights,*bones,num,*position,*normal,*source['uv']))
            tangents.append(struct.pack('<4f',*tangent,source['tangent'][3]*(-1 if left else 1)))
        for tri in faces:
            mapped=[start+remap[i] for i in tri]
            if left:mapped[1],mapped[2]=mapped[2],mapped[1]
            indices.extend(mapped)
    n=len(vertices);model=mdl_model(hm);assert integer(hm,model+72)==1
    mesh=model+integer(hm,model+76);assert integer(hm,mesh)==0
    struct.pack_into('<i',hm,model+80,n);struct.pack_into('<2i',hm,mesh+8,n,0);struct.pack_into('<8i',hm,mesh+52,*([n]*8))
    newvvd=bytearray(hv[:64])+b''.join(vertices)+b''.join(tangents)
    struct.pack_into('<8i',newvvd,16,*([n]*8));struct.pack_into('<2i',newvvd,56,64,64+48*n)
    # One body/model/LOD/mesh/group, all 43 original bones available to hardware.
    newvtx=bytearray(ht[:36]);struct.pack_into('<i',newvtx,32,36)
    newvtx+=struct.pack('<2i',1,8)+struct.pack('<2i',1,8)+struct.pack('<2if',1,12,0)+struct.pack('<2iB',1,9,0)
    newvtx+=create_group(vertices,indices,43)
    material_list=len(newvtx);newvtx+=struct.pack('<2i',0,0);struct.pack_into('<i',newvtx,24,material_list)
    assert len(list(triangles(hm,newvtx)))==2*len(faces)
    assert len(vvd_vertices(newvvd))==n
    return tuple(map(bytes,(hm,newvvd,newvtx))),dict(vertices=n,triangles=len(indices)//3,bones=43,atlas='Chell v_portalgun/v_hands',mirrored_winding_and_tangents=True)

if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('inputs');p.add_argument('output');a=p.parse_args()
    result,receipt=generate(read_tuple(a.inputs,'v_portalgun'),read_tuple(a.inputs,'v_hands'))
    out=Path(a.output);assert not out.exists();out.mkdir(parents=True)
    for ext,data in zip(('mdl','vvd','dx90.vtx'),result):(out/('v_hands.'+ext)).write_bytes(data)
    receipt['output_sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir()}
    (out/'generation.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
