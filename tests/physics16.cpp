#include <Windows.h>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
#include "bodyik.h"
#include "nativephysics.h"
#include "propinteraction.h"
#include "handcontact.h"
static std::vector<unsigned char> Read(const char* path){std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
static void Near(Vector a,Vector b,float e=.02f){assert((a-b).LengthSqr()<e*e);}
static Vector Point(const matrix3x4_t& m,Vector p){return {m[0][0]*p.x+m[0][1]*p.y+m[0][2]*p.z+m[0][3],m[1][0]*p.x+m[1][1]*p.y+m[1][2]*p.z+m[1][3],m[2][0]*p.x+m[2][1]*p.y+m[2][2]*p.z+m[2][3]};}
struct Vertex {float weights[3];unsigned char bones[3],count;Vector p,normal;float uv[2];};
int main(int argc,char** argv){
    assert(argc==5);HMODULE server=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(server);
    const auto binding=NativePhysics::Binding::Resolve(reinterpret_cast<uintptr_t>(server));
    assert(binding.setTarget&&binding.detach&&binding.getAttached&&binding.entries);FreeLibrary(server);
    assert(!NativePhysics::Binding::Resolve(0).getAttached);
    uintptr_t entries[4096*4]{};uintptr_t rows=reinterpret_cast<uintptr_t>(entries);int dummy=5;
    auto fake=binding;fake.entries=&rows;entries[17*4+1]=reinterpret_cast<uintptr_t>(&dummy);entries[17*4+2]=23;
    assert(fake.Entity((23<<12)|17)==&dummy&&!fake.Entity((24<<12)|17)&&!fake.Entity(0xffffffff));
    PropInteraction::Motion motion;Vector linear,angular;
    for(float hz:{45.f,72.f,90.f,120.f})for(float yaw:{-175.f,0.f,175.f}) {
        motion.Reset();for(int i=0;i<15;++i){float t=i/hz;motion.Push(t,PortalPose::Frame({100+170*t,-80*t,60*t},{0,yaw+180*t,0}),43.2f);}
        assert(motion.Velocity(14/hz,linear,angular));Near(linear,{170,-80,60},.05f);Near(angular,{0,0,180},.1f);
        assert(!motion.Velocity(14/hz+.2,linear,angular));
        motion.Push(14/hz+.01,PortalPose::Frame({5000,0,0},{0,0,0}),43.2f);assert(motion.count==1&&!motion.Velocity(14/hz+.01,linear,angular));
    }
    Near(PropInteraction::CenterVelocity({1,2,3},{0,0,90},{0,0,0},{10,0,0}),{1,2+15.707963f,3});
    Near(PropInteraction::Limit({30,40,0},10),{6,8,0});
    PropInteraction::Carry carry;const auto initial=PortalPose::Frame({20,10,5},{30,50,70}),hand=PortalPose::Frame({10,5,0},{15,-35,20});
    auto target=carry.Target((void*)1,(void*)2,1,0,0,hand,initial,43.2f);
    for(int r=0;r<3;++r)for(int c=0;c<4;++c)assert(std::abs(target[r][c]-initial[r][c])<.001f);
    const auto delta=PortalPose::Frame({40,-30,12},{-30,120,70});
    target=carry.Target((void*)1,(void*)2,1,0,.02,HandPose::Concat(delta,hand),initial,43.2f);
    for(Vector p:{Vector(0,0,0),Vector(3,2,-7)})Near(Point(target,p),Point(delta,Point(initial,p)));
    target=carry.Target((void*)1,(void*)2,1,1,.04,hand,initial,43.2f);assert(carry.motion.count==1);Near(PortalPose::Position(target),PortalPose::Position(initial));
    float curl[5]={0,0,0,0,0};assert(!HandContact::Override(curl,1.5f,.65f));curl[1]=1;assert(!HandContact::Override(curl,1.5f,.65f));curl[2]=.8f;assert(HandContact::Override(curl,1.5f,.65f));
    for(Vector n:{Vector(0,0,1),Vector(0,0,-1),Vector(1,0,0),Vector(0,-1,0)}) {
        auto posed=HandContact::Surface(hand,n,{1,2,3},true);Near(TwoHandAim::Axis(posed,1),n);Near(TwoHandAim::Position(posed),{1,2,3});
        for(int a=0;a<3;++a)for(int b=0;b<3;++b)assert(std::abs(TwoHandAim::Dot(TwoHandAim::Axis(posed,a),TwoHandAim::Axis(posed,b))-(a==b?1.f:0.f))<.001f);
    }
    const auto data=Read(argv[2]),vvd=Read(argv[3]);BodyIK::Rig rig;assert(rig.Read(data.data(),data.size()));
    int vertices,start;memcpy(&vertices,vvd.data()+16,4);memcpy(&start,vvd.data()+56,4);assert(vertices>20000&&start+vertices*48<=int(vvd.size()));
    const auto model=HandPose::Frame({1,0,0},{0,0,1},{0,-1,0},{100,200,0});matrix3x4_t base[128],posed[128],saved[128];
    for(int i=0;i<rig.count;++i)base[i]=HandPose::Concat(model,rig.bind[i]);
    unsigned cases=0,weighted=0;float worst=0;const auto eye=rig.Eyes(base);
    // Jump changes native animation and eye altitude. Every torso skin vertex
    // must retain its camera-relative location, without an animated pelvis tug.
    for(float length:{.8f,1.f,1.2f})for(float drop:{16.f,23.f,35.f})for(float jump:{0.f,6.f,20.f}) {
        memcpy(posed,base,rig.count*48);const Vector movement{0,0,jump};
        for(int i=0;i<rig.count;++i)for(int r=0;r<3;++r)posed[i][r][3]+=movement[r];
        // Deliberately corrupt animated stomach/chest/hip offsets: the fitted
        // rigid body must rebuild from bind proportions, not inherit this warp.
        for(int b:{rig.hips,rig.spine,rig.chest})posed[b][2][3]+=jump*.4f;
        const float reach=rig.FitUpperBody(posed,eye+movement,{0,-1,0},43.2f,166,42,drop,4,0,1,0,true,true,1,1,true,length,3);
        matrix3x4_t hands[2];for(int h=0;h<2;++h)hands[h]=PortalPose::Frame(eye+movement+Vector(h?-12.f:12.f,-15,-14),{0,-90,0});
        const bool grip[2]{};const float closed[5]={1,1,1,1,1};const float* curls[2]={closed,closed};
        const auto headPose=PortalPose::Frame(eye+movement,{0,-90,0});
        assert(rig.SolveNative(posed,TwoHandAim::Position(posed[rig.head]),hands,{0,-1,0},reach,1,true,curls,grip,&headPose,1,.5f,true,1,1,1,true));
        rig.Volume(posed,1.05f,1.1f,1.12f,1,1,1.2f);
        if(jump==0)memcpy(saved,posed,rig.count*48);
        for(int b:{rig.hips,rig.spine,rig.chest})Near(TwoHandAim::Position(posed[b])-movement,TwoHandAim::Position(saved[b]),.003f);
        for(int i=0;i<vertices;++i){Vertex v;memcpy(&v,vvd.data()+start+i*48,48);bool torso=true;Vector p{0,0,0},q{0,0,0};
            for(int j=0;j<v.count;++j){int b=v.bones[j];assert(b<rig.count);torso&=b==rig.hips||(rig.below[rig.spine][b]&&!rig.below[rig.arm[0][0]][b]&&!rig.below[rig.arm[1][0]][b]&&!rig.below[rig.neck][b]);auto inverse=HandPose::InverseRigid(rig.bind[b]);p+=Point(HandPose::Concat(posed[b],inverse),v.p)*v.weights[j];q+=Point(HandPose::Concat(saved[b],inverse),v.p)*v.weights[j];}
            assert(TwoHandAim::Finite(p));if(torso){const float error=sqrtf((p-movement-q).LengthSqr());worst=std::max(worst,error);assert(error<.01f);}++weighted;
        }
        ++cases;
    }
    std::ofstream output(argv[4],std::ios::binary);output.write(reinterpret_cast<char*>(&rig.count),4);output.write(reinterpret_cast<char*>(posed),rig.count*48);output.write(reinterpret_cast<char*>(&posed[rig.arm[0][2]]),48);output.write(reinterpret_cast<char*>(&posed[rig.arm[1][2]]),48);
    printf("PASS: installed Portal server ABI and serial handles; 12 throw-rate/Euler-wrap/stale/teleport cases; rotating carry/hand switch; contact normals/manual-pose priority; %u rigid torso/jump poses, %u weighted skin checks, max camera-relative torso drift=%f units\n",cases,weighted,worst);
}
