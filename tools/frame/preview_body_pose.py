"""CPU-only orthographic skin-weight preview of an owned Chell and test palette.

Usage: preview_body_pose.py chell.mdl chell.vvd pose.bin output.bmp
No game, window, graphics device, asset editing or third-party dependency.
"""
import struct,sys,math
from pathlib import Path
from source_mesh import integer,unpack,inverse_bind,point,vvd_vertices,string

def preview(mdl_path,vvd_path,pose_path,out):
    mdl=Path(mdl_path).read_bytes();vvd=Path(vvd_path).read_bytes();raw=Path(pose_path).read_bytes()
    count=integer(raw,0);assert count==integer(mdl,156) and len(raw)==4+(count+2)*48
    matrices=[tuple(tuple(unpack(raw,'<12f',4+i*48)[r*4+c] for c in range(4)) for r in range(3)) for i in range(count+2)]
    parents=[integer(mdl,integer(mdl,160)+216*i+4) for i in range(count)]
    names={string(mdl,(at:=integer(mdl,160)+216*i)+integer(mdl,at)):i for i in range(count)}
    def below(bone,root):
        for _ in range(count):
            if bone==root:return True
            bone=parents[bone]
            if bone<0:return False
        return False
    skinned=[]
    for vertex in vvd_vertices(vvd):
        weights=unpack(vertex['record'],'<3f',0);ids=vertex['record'][12:15];n=vertex['record'][15];p=[0.,0.,0.]
        for j in range(n):
            bind_local=point(inverse_bind(mdl,ids[j]),vertex['position']);posed=point(matrices[ids[j]],bind_local)
            for c in range(3):p[c]+=posed[c]*weights[j]
        left=sum(weights[j] for j in range(n) if below(ids[j],names['bicep_L']))
        right=sum(weights[j] for j in range(n) if below(ids[j],names['bicep_R']))
        color=(30,130,210) if left>.5 else (230,125,35) if right>.5 else (130,140,150)
        skinned.append((p,color))
    width,height=1200,600;pixels=bytearray(bytes((247,245,242))*width*height)
    def pixel(x,y,color):
        if 0<=x<width and 0<=y<height:pixels[(y*width+x)*3:(y*width+x)*3+3]=bytes(reversed(color))
    axes=((0,2),(1,2),(0,1));panels=[]
    for panel,(a,b) in enumerate(axes):
        xs=[p[a] for p,_ in skinned];ys=[p[b] for p,_ in skinned]
        scale=min(350/max(max(xs)-min(xs),1),520/max(max(ys)-min(ys),1));cx=(min(xs)+max(xs))/2;cy=(min(ys)+max(ys))/2
        def project(p):return round(panel*400+200+(p[a]-cx)*scale),round(300-(p[b]-cy)*scale)
        panels.append(project)
        for p,color in skinned:
            x,y=project(p);pixel(x,y,color)
        for matrix in matrices[count:]:
            x,y=project([matrix[r][3] for r in range(3)])
            for d in range(-7,8):pixel(x+d,y,(10,190,70));pixel(x,y+d,(10,190,70))
        for y in range(height):pixel(panel*400,y,(30,30,30))
    header=struct.pack('<2sIHHI',b'BM',54+len(pixels),0,0,54)+struct.pack('<IiiHHIIiiII',40,width,-height,1,24,0,len(pixels),2835,2835,0,0)
    Path(out).write_bytes(header+pixels)
    print('CPU native skin preview: front / side / top; left blue, right orange, wrists green; vertices',len(skinned))

if __name__=='__main__':preview(*sys.argv[1:])
