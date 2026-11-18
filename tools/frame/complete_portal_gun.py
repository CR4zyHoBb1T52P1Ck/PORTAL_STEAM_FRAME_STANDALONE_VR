"""Generate the missing lower housing from an owned Portal installation.

Keep every original bone, attachment and animation. Append one rigid housing
mesh to the existing viewmodel's MDL/VVD/VTX tuple. This script contains no
retail asset bytes and accepts only the verified single-model/LOD layout.
"""
import hashlib,json,math,struct
from pathlib import Path
from source_mesh import *

FIT=dict(mapping=(-2,-1,-3),scale=1.1080807133325283,offset=(-.39205538641233484,1.3147089796926283,3.139985691246853))
RETAIL_SHA256=(
    '156a5e01085fe6e82eb1be8aff609f33c3bb20c885b0212e02c0d0c0509615d5',
    '484dc66b96538ecef5e321fa3460da73ffa8b2ade291c6a9930a024ee24ff468',
    '79737aa67c7c1d5b89bce72ed312fc125760c539fdcf49fcadc6aea003a123ea',
    'ac6705a8fb0e7df00161ddbe3517bf8628890d09a841bb346b45c427aeec3849',
    'd7ab869fccfbb8fa2fab372c8d5add52b49ebb6739158adcf693108e484a15c7',
    '4ed8b8982e2bef6042ed6b88559f2c1cf3cbf076f2e8668e6d79b4683314a5eb')

def transform(p,normal=False):
    out=tuple(p[abs(m)-1]*(1 if m>0 else -1) for m in FIT['mapping'])
    return out if normal else tuple(out[c]*FIT['scale']+FIT['offset'][c] for c in range(3))

def triangle_distance_squared(p,tri):
    a,b,c=tri
    sub=lambda x,y:tuple(x[i]-y[i] for i in range(3))
    dot=lambda x,y:sum(x[i]*y[i] for i in range(3))
    ab,ac,ap=sub(b,a),sub(c,a),sub(p,a)
    d00,d01,d11=dot(ab,ab),dot(ab,ac),dot(ac,ac);den=d00*d11-d01*d01
    if den>1e-12:
        u=(d11*dot(ap,ab)-d01*dot(ap,ac))/den;v=(d00*dot(ap,ac)-d01*dot(ap,ab))/den
        if u>=0 and v>=0 and u+v<=1:return sum((ap[i]-u*ab[i]-v*ac[i])**2 for i in range(3))
    best=float('inf')
    for x,y in ((a,b),(b,c),(c,a)):
        edge,delta=sub(y,x),sub(p,x);length=dot(edge,edge);t=max(0,min(1,dot(delta,edge)/length)) if length else 0
        best=min(best,sum((delta[i]-edge[i]*t)**2 for i in range(3)))
    return best

def mask_overlapping_shell(mdl,vvd,vtx,replacement):
    """Remove rigid rear view-only faces; replace the whole region consistently.

    Native glass, hands, front mechanism and every animated gun part survive.
    Mixing only close triangles left intersecting one-way shell edges. The
    complete owned world-model rear now supplies the entire curved housing.
    """
    vertices=vvd_vertices(vvd);bind=inverse_bind(mdl,24);model=mdl_model(mdl);lod=vtx_lod(vtx);masked=0
    for mesh in range(integer(mdl,model+72)):
        mh=model+integer(mdl,model+76)+mesh*116
        if integer(mdl,mh)!=1:continue
        offset=integer(mdl,mh+12);vm=lod+integer(vtx,lod+4)+mesh*9
        for group in range(integer(vtx,vm)):
            sg=vm+integer(vtx,vm+4)+group*25
            num,vo,ni,io,ns,so=unpack(vtx,'<6i',sg)
            for strip in range(ns):
                sh=sg+so+strip*27;n,index=unpack(vtx,'<2i',sh)
                if not vtx[sh+18]&1:continue
                for j in range(0,n,3):
                    at=sg+io+(index+j)*2;ids=unpack(vtx,'<3H',at)
                    original=[offset+unpack(vtx,'<H',sg+vo+i*9+4)[0] for i in ids]
                    if not all(vertices[i]['record'][12:16]==bytes([24,0,0,1]) for i in original):continue
                    pts=[point(bind,vertices[i]['position']) for i in original]
                    if max(q[2] for q in pts)<17.8:
                        struct.pack_into('<3H',vtx,at,ids[0],ids[0],ids[0]);masked+=1
    assert 100<masked<2500
    return masked

def complete(view,world):
    assert tuple(hashlib.sha256(x).hexdigest() for x in (*view,*world))==RETAIL_SHA256,'Unsupported retail gun assets; preserve them unchanged'
    mdl,vvd,vtx=map(bytearray,view);wm,wv,wt=world
    assert integer(mdl,4)==46 and integer(mdl,156)==45
    assert string(mdl,12).lower()=='weapons/v_portalgun.mdl'
    assert integer(wm,156)==1 and integer(wm,4)==44
    assert integer(mdl,8)==integer(vvd,8)==integer(vtx,16)
    old=list(vvd_vertices(vvd));wvlist=vvd_vertices(wv)
    retail_triangle_count=len(list(triangles(mdl,vtx)))
    # World housing uses native UVs/material. Exclude moving claws/front barrel;
    # retain lower rear faces that the first-person mesh leaves open.
    selected=[];outer_count=0;liner_count=0;world_count=len(wvlist)
    for material,tri in triangles(wm,wt):
        assert material==0
        positions=[transform(wvlist[i]['position']) for i in tri]
        normals=[transform(wvlist[i]['normal'],True) for i in tri]
        center=tuple(sum(p[c] for p in positions)/3 for c in range(3))
        if center[2]<18:
            selected.append(tri);outer_count+=1
            # Back faces of a closed curved shell must remain opaque from
            # extreme VR viewing angles too. Inset reversed triangles form
            # an inner lining without coplanar z-fighting.
            selected.append(tuple(i+world_count for i in reversed(tri)));liner_count+=1
    ids=sorted({i for tri in selected for i in tri});remap={n:i for i,n in enumerate(ids)}
    assert 100<len(selected)<2500 and len(ids)<65536
    replacement=[]
    for tri in selected:
        face=[]
        for i in tri:
            p=transform(wvlist[i%world_count]['position'])
            if i>=world_count:
                normal=transform(wvlist[i%world_count]['normal'],True);p=tuple(p[c]-.12*normal[c] for c in range(3))
            face.append(p)
        replacement.append(face)
    masked=mask_overlapping_shell(mdl,vvd,vtx,replacement)
    original_triangles=list(triangles(mdl,vtx))
    assert len(original_triangles)==retail_triangle_count-masked
    base_to_model=rigid_inverse(inverse_bind(mdl,24))
    def rotate(p):return tuple(sum(base_to_model[r][c]*p[c] for c in range(3)) for r in range(3))
    added=[];tangents=[]
    for i in ids:
        v=wvlist[i%world_count];base=transform(v['position']);surface_normal=transform(v['normal'],True)
        if i>=world_count:base=tuple(base[c]-.12*surface_normal[c] for c in range(3))
        position=point(base_to_model,base);normal=rotate(surface_normal)
        record=struct.pack('<3f4B3f3f2f',1,0,0,24,0,0,1,*position,*normal,*v['uv']);assert len(record)==48
        added.append(record)
        tangent=rotate(transform(v['tangent'][:3],True));tangents.append(struct.pack('<4f',*tangent,v['tangent'][3]))
    # Retain every original vertex/tangent byte, then append the new mesh.
    n=len(old);nv=n+len(ids);vertices=integer(vvd,56);tangent=integer(vvd,60)
    newvvd=bytearray(vvd[:64])+vvd[vertices:vertices+n*48]+b''.join(added)+vvd[tangent:tangent+n*16]+b''.join(tangents)
    struct.pack_into('<8i',newvvd,16,*([nv]*8));struct.pack_into('<2i',newvvd,56,64,64+nv*48)
    def align(data):
        while len(data)%4:data.append(0)
    # Rebuild only the small material/skin/mesh header arrays at the tail.
    nt=integer(mdl,204);assert nt==3 and integer(mdl,220)==3 and integer(mdl,224)==1
    old_texture=integer(mdl,208);old_skin=integer(mdl,228)
    model=mdl_model(mdl);nm=integer(mdl,model+72);assert nm==3 and integer(mdl,model+80)==n
    old_mesh=model+integer(mdl,model+76)
    align(mdl);texture_start=len(mdl)
    mdl+=b''.join(mdl[old_texture+64*i:old_texture+64*(i+1)] for i in range(nt))+bytes(64)
    names=[string(mdl,old_texture+64*i+integer(mdl,old_texture+64*i)) for i in range(nt)]
    names.append('portal_frame_gun_housing')
    for i,name in enumerate(names):
        at=texture_start+64*i;struct.pack_into('<i',mdl,at,len(mdl)-at);mdl+=name.encode()+b'\0'
    struct.pack_into('<2i',mdl,204,4,texture_start)
    align(mdl);skin_start=len(mdl);mdl+=mdl[old_skin:old_skin+6]+struct.pack('<H',3)
    struct.pack_into('<i',mdl,220,4);struct.pack_into('<i',mdl,228,skin_start)
    align(mdl);new_mesh=len(mdl)
    for i in range(nm):
        original=old_mesh+116*i;at=len(mdl);record=bytearray(mdl[original:original+116])
        struct.pack_into('<i',record,4,model-at)
        if integer(record,16):struct.pack_into('<i',record,20,original+integer(record,20)-at)
        mdl+=record
    record=bytearray(116);struct.pack_into('<4i',record,0,3,model-(new_mesh+116*nm),len(ids),n)
    struct.pack_into('<i',record,32,3);struct.pack_into('<8i',record,52,*([len(ids)]*8));mdl+=record
    struct.pack_into('<2i',mdl,model+72,4,new_mesh-model);struct.pack_into('<i',mdl,model+80,nv)
    struct.pack_into('<i',mdl,76,len(mdl))
    # Preserve old strips and mesh headers. The new group has a single rigid
    # hardware-bone palette entry pointing at the existing gun-base bone24.
    lod=vtx_lod(vtx);old_vm=lod+integer(vtx,lod+4);align(vtx);new_vm=len(vtx)
    for i in range(nm):
        at=old_vm+9*i;count,off=unpack(vtx,'<2i',at)
        vtx+=struct.pack('<2iB',count,at+off-(new_vm+9*i),vtx[at+8])
    vtx+=struct.pack('<2iB',1,9,0)
    sg=len(vtx);indices=[remap[i] for tri in selected for i in tri];count=len(ids)
    vertex_offset=25;index_offset=25+9*count;strip_offset=index_offset+2*len(indices)
    vtx+=struct.pack('<6iB',count,vertex_offset,len(indices),index_offset,1,strip_offset,2)
    for i in range(count):vtx+=struct.pack('<4BH3B',0,0,0,1,i,0,0,0)
    vtx+=struct.pack('<'+str(len(indices))+'H',*indices)
    vtx+=struct.pack('<4ihB2i',len(indices),0,count,0,1,1,1,27)+struct.pack('<2i',0,24)
    struct.pack_into('<2i',vtx,lod,4,new_vm-lod)
    # Reparse the output and prove all authored primitives survived unchanged.
    final=list(triangles(mdl,vtx));assert final[:len(original_triangles)]==original_triangles
    assert len(final)==len(original_triangles)+len(selected)
    parsed=vvd_vertices(newvvd);assert len(parsed)==nv
    for i in range(n):assert parsed[i]['record']==old[i]['record'] and parsed[i]['tangent']==old[i]['tangent']
    for offset in (156,160,180,184,188,192,240,244):assert integer(mdl,offset)==integer(view[0],offset)
    return tuple(map(bytes,(mdl,newvvd,vtx))),dict(original_vertices=n,added_vertices=count,original_triangles=len(original_triangles),masked_legacy_shell_triangles=masked,retail_triangles=retail_triangle_count,added_triangles=len(selected),outer_triangles=outer_count,liner_triangles=liner_count,liner_inset=.12,rigid_rear_replacement=True,bones=45,animation_and_attachment_offsets_preserved=True,fit=FIT)

def main():
    import argparse
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('inputs');parser.add_argument('output');args=parser.parse_args()
    folder=Path(args.inputs);output=Path(args.output);assert not output.exists(),'Preserve prior output'
    view=read_tuple(folder,'v_portalgun');world=read_tuple(folder,'w_portalgun')
    result,receipt=complete(view,world);output.mkdir(parents=True)
    for ext,data in zip(('mdl','vvd','dx90.vtx'),result):(output/('v_portalgun.'+ext)).write_bytes(data)
    receipt['input_sha256']={name+'.'+ext:hashlib.sha256(data).hexdigest() for name,files in [('v_portalgun',view),('w_portalgun',world)] for ext,data in zip(('mdl','vvd','dx90.vtx'),files)}
    receipt['output_sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir()}
    (output/'generation.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
if __name__=='__main__':main()
