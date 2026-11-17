#include <Windows.h>
#include <cassert>
#include <fstream>
#include <vector>
#include "twohandaim.h"
#include "optionalgungrip.h"
#include "bindposecache.h"
#include "nativefit.h"
#include "portalpose.h"
#include "gunattachments.h"
#include "gunray.h"
static void Near(float a,float b,float e=.004f){assert(std::abs(a-b)<e);}
static void Near(const Vector& a,const Vector& b,float e=.004f){assert((a-b).LengthSqr()<e*e);}
static void Near(const matrix3x4_t& a,const matrix3x4_t& b){for(int r=0;r<3;++r)for(int c=0;c<4;++c)Near(a[r][c],b[r][c]);}
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)),{});assert(data.size()>248);
    int bones,offset,size,checksum;memcpy(&bones,data.data()+156,4);memcpy(&offset,data.data()+160,4);memcpy(&size,data.data()+76,4);memcpy(&checksum,data.data()+8,4);assert(bones==45);
    BindPoseCache::Entry cache;
    const auto* bind=cache.Get(data.data(),size,bones,offset,checksum);assert(bind&&cache.rebuilds==1);
    for(int i=0;i<10000;++i)assert(cache.Get(data.data(),size,bones,offset,checksum)==bind);
    assert(cache.rebuilds==1&&!cache.Get(data.data(),offset+1,bones,offset,checksum));
    assert(cache.Get(data.data(),size,bones,offset,checksum+1)&&cache.rebuilds==2);
    matrix3x4_t socket;assert(OptionalGunGrip::ReadSocket(data.data(),data.size(),socket,true));
    assert(!OptionalGunGrip::ReadSocket(data.data(),data.size(),socket)); // no paw socket in retail
    assert(OptionalGunGrip::ReadSocket(data.data(),data.size(),socket,true));
    Near(TwoHandAim::Position(socket),{0,-2.8f,12});
    GunAttachments::Model model;assert(model.Read(data.data(),data.size(),bind,true));
    const auto support=HandPose::Concat(model.gunFromController,socket);
    unsigned cases=0;
    for(float pitch:{-85.f,-30.f,0.f,35.f,85.f})for(float yaw:{-170.f,-40.f,0.f,60.f,170.f})
    for(float roll:{-175.f,-60.f,0.f,80.f,175.f})for(float strength:{0.f,.5f,1.f})
    for(Vector delta:{Vector(0,0,0),Vector(3,0,2),Vector(-4,5,-1),Vector(8,-5,3)}) {
        const auto raw=PortalPose::Frame({11,-37,90},{pitch,yaw,roll});
        const auto pivot=TwoHandAim::Position(raw)+TwoHandAim::Axis(raw,2)*2.5f;
        const auto predicted=TwoHandAim::Position(HandPose::Concat(raw,support));
        const auto offhand=predicted+delta;
        const auto actual=TwoHandAim::Solve(raw,pivot,offhand,support,true,strength,2.5f);
        for(int r=0;r<3;++r)for(int c=0;c<4;++c)assert(std::isfinite(actual[r][c]));
        const auto pivotLocal=HandPose::Concat(HandPose::InverseRigid(raw),HandPose::Frame({1,0,0},{0,1,0},{0,0,1},pivot));
        Near(TwoHandAim::Position(HandPose::Concat(actual,pivotLocal)),pivot);
        for(int a=0;a<3;++a)for(int b=0;b<3;++b)Near(TwoHandAim::Dot(TwoHandAim::Axis(actual,a),TwoHandAim::Axis(actual,b)),a==b?1.f:0.f,.001f);
        if(strength==0||delta.LengthSqr()==0)Near(actual,raw);
        if(strength==1) {
            auto a=TwoHandAim::Position(HandPose::Concat(actual,support))-pivot,b=offhand-pivot;
            assert(TwoHandAim::Unit(a)&&TwoHandAim::Unit(b));Near(a,b);
        }
        const auto gun=HandPose::Concat(actual,model.gunFromController);
        matrix3x4_t native[45];for(int i=0;i<45;++i)native[i]=HandPose::Concat(raw,bind[i]);
        matrix3x4_t resolved;assert(model.Resolve(1,actual,native,resolved));
        const auto aimBarrel=HandPose::Concat(actual,model.barrelFromController);
        Near(resolved,aimBarrel);
        Vector drawOrigin,drawDirection,aimOrigin,aimDirection;
        assert(GunRay::FromBarrel(resolved,pivot,drawOrigin,drawDirection));
        assert(GunRay::FromBarrel(aimBarrel,pivot,aimOrigin,aimDirection));
        Near(drawOrigin,aimOrigin);Near(drawDirection,aimDirection);
        const auto wrist=TwoHandAim::GripWrist(bind[8],bind[24],gun);
        Near(HandPose::Concat(HandPose::InverseRigid(gun),wrist),HandPose::Concat(HandPose::InverseRigid(bind[24]),bind[8]));
        const auto parent=PortalPose::Frame({-50,12,5},{35,-65,12});
        const auto world=TwoHandAim::Solve(HandPose::Concat(parent,raw),TwoHandAim::Position(HandPose::Concat(parent,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},pivot))),
            TwoHandAim::Position(HandPose::Concat(parent,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},offhand))),support,true,strength,2.5f);
        Near(world,HandPose::Concat(parent,actual));++cases;
    }
    auto raw=PortalPose::Frame({0,0,0},{0,0,0});
    for(Vector offhand:{Vector(0,0,0),Vector(NAN,0,0)})Near(TwoHandAim::Solve(raw,{0,0,0},offhand,support,true,1,2),raw);
    Near(TwoHandAim::Solve(raw,{0,0,0},{10,0,0},support,false,1,2),raw);
    // Opposing grip directions and zero-length hands cannot generate NaNs.
    auto old=TwoHandAim::Position(HandPose::Concat(raw,support));const auto opposite=TwoHandAim::Solve(raw,{0,0,0},-old,support,true,1,2);
    Near(TwoHandAim::Position(HandPose::Concat(opposite,support)),-old);
    OptionalGunGrip::State state;assert(!state.Update(true,true,true,false,0,6));
    assert(state.Update(true,true,true,true,0,6));assert(!state.Update(true,false,true,true,0,6));
    assert(!state.Update(true,true,true,true,0,6)); // tracking recovery with held grip must not recapture
    state.Update(true,true,true,false,0,6);assert(state.Update(true,true,true,true,0,6));
    state.active=false;state.wasPressed=true;assert(!state.Update(true,true,true,true,0,6)); // menu return
    for(int arms:{0,1,2})for(bool left:{false,true})for(bool shown:{false,true}) {
        VrGraphics::Settings s;s.arms=arms;s.leftArm=s.rightArm=shown;
        matrix3x4_t palette[43];for(int i=0;i<43;++i)palette[i]=HandPose::Frame({1,0,0},{0,1,0},{0,0,1},{0,0,0});
        NativeFit::Visibility(palette,43,false,left,s);
        bool any=false;for(int i:left?std::vector<int>{8,9,10,11}:std::vector<int>{27,28,29,30})any|=palette[i][0][0]!=0;
        assert(NativeFit::HandModelVisible(left,s)==any);
    }
    printf("PASS: %u two-hand poses, grip/pivot invariance, portal/parent transforms, influence, native socket, release guards, hidden-hand culling, 10000 bind-cache hits\n",cases);
}
