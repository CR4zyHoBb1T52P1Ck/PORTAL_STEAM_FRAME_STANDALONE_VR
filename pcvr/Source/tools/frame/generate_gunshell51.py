"""Build private CPU-only opaque-gun BVHs from the owner's existing gun tuple.

No rendered model/texture is changed. Generated triangle data stays private;
release installers must regenerate it from the owner's assets.
"""
import hashlib,json,struct
from pathlib import Path
from source_mesh import read_tuple,integer,string,triangles,vvd_vertices,inverse_bind,rigid_inverse,point

def concat(a,b):return tuple(tuple(sum(a[r][k]*b[k][c] for k in range(3))+(a[r][3] if c==3 else 0) for c in range(4)) for r in range(3))
def generate(source):
 m,v,t=source;assert integer(m,4) in (46,48) and integer(m,156)==45
 assert integer(m,8)==integer(v,8)==integer(t,16),'Model tuple checksum mismatch'
 names=[string(m,integer(m,208)+i*64+integer(m,integer(m,208)+i*64)) for i in range(integer(m,204))]
 assert names==['v_hands','v_portalgun','v_portalgun_glass','portal_frame_gun_housing'],'Unexpected material routing'
 vertices=vvd_vertices(v);groups={};skipped=0
 for material,ids in triangles(m,t):
  if material not in (1,3):continue # hands and GLASS never become blockers
  bones={vertices[i]['record'][12] for i in ids}
  if len(bones)!=1 or any(vertices[i]['record'][15]!=1 for i in ids):skipped+=1;continue
  bone=next(iter(bones));assert 24<=bone<45
  local=inverse_bind(m,bone);tri=tuple(point(local,vertices[i]['position']) for i in ids)
  # Store the same float32 positions the CPU reader tests, before BVH bounds.
  tri=tuple(struct.unpack('<3f',struct.pack('<3f',*p)) for p in tri)
  groups.setdefault(bone,[]).append(tri)
 nodes=[];ordered=[];parts=[];depth=0
 def tree(tris,level=1):
  nonlocal depth
  depth=max(depth,level);index=len(nodes);nodes.append(None)
  low=[min(p[c] for tri in tris for p in tri)-.0001 for c in range(3)];high=[max(p[c] for tri in tris for p in tri)+.0001 for c in range(3)]
  if len(tris)<=6:
   first=len(ordered);ordered.extend(tris);nodes[index]=(*low,*high,0,0,first,len(tris))
  else:
   axis=max(range(3),key=lambda c:high[c]-low[c]);tris=sorted(tris,key=lambda tri:sum(p[axis] for p in tri));mid=len(tris)//2
   left=tree(tris[:mid],level+1);right=tree(tris[mid:],level+1);nodes[index]=(*low,*high,left,right,0,0)
  return index
 for bone,tris in sorted(groups.items()):
  relative=concat(inverse_bind(m,24),rigid_inverse(inverse_bind(m,bone)))
  parts.append((bone,tree(tris),tuple(x for row in relative for x in row)))
 assert len(parts)<=21 and len(nodes)<32768 and len(ordered)<20000 and depth<32
 data=struct.pack('<8sIi3I',b'PVRSH51\0',1,integer(m,8),len(parts),len(nodes),len(ordered))
 for bone,root,bind in parts:data+=struct.pack('<2I12f',bone,root,*bind)
 for node in nodes:data+=struct.pack('<6f4I',*node)
 for tri in ordered:data+=struct.pack('<9f',*(x for p in tri for x in p))
 return data,dict(parts=len(parts),nodes=len(nodes),opaque_triangles=len(ordered),skipped_blended_triangles=skipped,excluded_materials=['hands','glass'],depth=depth,model_checksum=integer(m,8),input_sha256=[hashlib.sha256(x).hexdigest() for x in source],bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),rendered_models_changed=False)

if __name__=='__main__':
 import argparse
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('input');parser.add_argument('output');args=parser.parse_args()
 result,receipt=generate(read_tuple(Path(args.input),'v_portalgun'));dest=Path(args.output);assert not dest.exists();dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(result);dest.with_suffix('.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
