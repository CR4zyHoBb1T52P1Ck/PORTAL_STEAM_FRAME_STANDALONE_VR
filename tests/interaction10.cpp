#include <Windows.h>
#include <cassert>
#include <cstdio>
#include "gripzone.h"
#include "weaponhandling.h"
#include "optionalgungrip.h"
#include "nativefit.h"
#include "portalpose.h"
#include "vrmenuoptions.h"
static void Near(float a,float b,float tolerance=.002f){assert(std::abs(a-b)<tolerance);}
static void Near(const matrix3x4_t& a,const matrix3x4_t& b){for(int r=0;r<3;++r)for(int c=0;c<4;++c)Near(a[r][c],b[r][c]);}
static Vector Transform(const matrix3x4_t& m,const Vector& p){return TwoHandAim::Position(HandPose::Concat(m,GripZone::Point(p)));}
int main() {
    unsigned zones=0,poses=0;
    for(float pitch:{-85.f,-30.f,0.f,35.f,85.f})for(float yaw:{-170.f,0.f,60.f,170.f})for(float roll:{-175.f,0.f,80.f,175.f})
    for(float scale:{.5f,1.f,2.5f})for(float z:{-30.f,-10.f,0.f,10.f,30.f})for(float x:{0.f,2.f,15.f}) {
        const auto gun=PortalPose::Frame({110,-75,95},{pitch,yaw,roll});
        GripZone::Shape shape;shape.length=19*scale;shape.radius=6*scale;
        const Vector local=shape.center+Vector(x,1,z);
        const auto hit=GripZone::Closest(gun,Transform(gun,local),shape);assert(hit.valid);
        const float clamped=std::clamp(z,-shape.length*.5f,shape.length*.5f);
        Near(hit.point.z,shape.center.z+clamped);
        Near(hit.distance,sqrtf(x*x+1+(z-clamped)*(z-clamped)));
        ++zones;
    }
    auto identity=GripZone::Point({0,0,0});GripZone::Shape shape;
    assert(!GripZone::Closest(identity,{NAN,0,0},shape).valid);
    auto broken=identity;broken[0][0]=NAN;assert(!GripZone::Closest(broken,{0,0,0},shape).valid);
    shape.radius=0;assert(!GripZone::Closest(identity,{0,0,0},shape).valid);
    // Calibration must preserve the full submitted gun pose at handoff. The new
    // support anchor at the old primary hand must also leave the two-hand solve
    // unchanged, including under a portal-space rigid transform.
    for(float pitch:{-80.f,-20.f,0.f,75.f})for(float yaw:{-170.f,-45.f,0.f,130.f})for(float roll:{-150.f,0.f,95.f})
    for(float influence:{0.f,.4f,1.f}) {
        const auto old=PortalPose::Frame({100,-35,80},{pitch,yaw,roll});
        const auto newHand=PortalPose::Frame({105,-40,78},{-pitch,yaw+30,roll-75});
        const auto calibrated=HandPose::Concat(newHand,WeaponHandling::Rebase(newHand,old));Near(calibrated,old);
        const Vector primary={105,-40,78},other={100,-35,80};
        const auto anchor=HandPose::Concat(HandPose::InverseRigid(calibrated),GripZone::Point(other));
        Near(TwoHandAim::Solve(calibrated,primary,other,anchor,true,influence,2),old);
        const auto parent=PortalPose::Frame({-50,77,10},{25,-70,30});
        const auto transformed=HandPose::Concat(HandPose::Concat(parent,newHand),WeaponHandling::Rebase(HandPose::Concat(parent,newHand),HandPose::Concat(parent,old)));
        Near(transformed,HandPose::Concat(parent,old));++poses;
    }
    for(int start:{0,1})for(int cycle=0;cycle<1000;++cycle) {
        WeaponHandling::State s;s.hand=start;
        bool grip[2]{},tracked[2]={true,true},shoulder[2]{};
        assert(s.Update(grip,tracked,shoulder,false,true,true)==WeaponHandling::None);
        shoulder[start]=true;grip[start]=true;
        assert(s.Update(grip,tracked,shoulder,false,true,true)==WeaponHandling::Stow&&s.stowed);
        assert(s.fireBlocked);s.Triggers(true,false);assert(s.fireBlocked);s.Triggers(false,false);assert(!s.fireBlocked);
        assert(s.Update(grip,tracked,shoulder,false,true,true)==WeaponHandling::None&&s.stowed);
        grip[start]=false;s.Update(grip,tracked,shoulder,false,true,true);
        shoulder[1-start]=true;grip[1-start]=true;
        assert(s.Update(grip,tracked,shoulder,false,true,true)==WeaponHandling::Draw&&!s.stowed&&s.hand==1-start);
        // Support blocks shoulder stow. Release old primary while new primary
        // is squeezed transfers ownership; a released support hand stays released.
        grip[start]=true;assert(s.Update(grip,tracked,shoulder,true,true,true)==WeaponHandling::None);
        grip[1-start]=false;
        assert(s.Update(grip,tracked,shoulder,true,true,true)==WeaponHandling::Transfer&&s.hand==start&&!s.transferSupport);
        s.Suspend();assert(s.blocked[0]&&s.blocked[1]&&!s.transferSupport);
        assert(s.Update(grip,tracked,shoulder,false,true,true)==WeaponHandling::None);
        grip[0]=grip[1]=false;s.Update(grip,tracked,shoulder,false,true,true);assert(!s.blocked[0]&&!s.blocked[1]);
        // Tracking loss with held grip must not re-acquire until a tracked
        // neutral sample, even if input reports false while tracking is lost.
        tracked[start]=false;grip[start]=true;s.Update(grip,tracked,shoulder,false,true,true);assert(s.blocked[start]);
        grip[start]=false;s.Update(grip,tracked,shoulder,false,true,true);assert(s.blocked[start]);
        tracked[start]=true;grip[start]=true;assert(s.Update(grip,tracked,shoulder,false,true,true)==WeaponHandling::None);
        grip[start]=false;s.Update(grip,tracked,shoulder,false,true,true);assert(!s.blocked[start]);
        grip[start]=true;assert(s.Update(grip,tracked,shoulder,false,false,false)==WeaponHandling::None);
    }
    OptionalGunGrip::State support;
    assert(!support.Update(true,true,true,true,20,6,true));
    assert(support.Update(true,true,true,true,5,6,true)); // already-held easy grab
    assert(!support.Update(true,false,true,true,5,6,true));
    support={};assert(!support.Update(true,true,true,true,20,6));assert(!support.Update(true,true,true,true,5,6)); // optional fresh-only
    Near(WeaponHandling::Shoulder({0,0,80},{1,0,0},{0,-1,0},0,10,6,5).y,10);
    Near(WeaponHandling::Shoulder({0,0,80},{1,0,0},{0,-1,0},1,10,6,5).y,-10);
    for(int arms:{0,1,2})for(bool left:{false,true})for(bool right:{false,true}) {
        matrix3x4_t bones[43];for(int i=0;i<43;++i)bones[i]=GripZone::Point({float(i),5,10});
        const auto leftWrist=bones[8],rightWrist=bones[27];NativeFit::BothHandsVisibility(bones,arms,left,right);
        if(left)Near(bones[8],leftWrist);else Near(bones[8][0][0],0);
        if(right)Near(bones[27],rightWrist);else Near(bones[27][0][0],0);
        Near(bones[0][0][0],0);
        if(arms!=0){Near(bones[5][0][0],0);Near(bones[24][0][0],0);}
    }
    for(int preset=0;preset<4;++preset) {
        VrGraphics::Settings s;s.gunX=17;s.grabHaptics=false;s.leftHanded=true;
        VrMenu::ApplyPreset(s,preset);assert(s.gunX==17&&!s.grabHaptics&&s.leftHanded);
        assert(s.eyeWidth<=s.upscaleWidth);assert(s.msaa==(preset==0?4:2));
        assert(s.upscaler==(preset==0?0:preset==2?1:2));
    }
    assert(VrMenu::Parent(VrMenu::Support)==VrMenu::Gun&&VrMenu::Parent(VrMenu::Feedback)==VrMenu::Support);
    printf("PASS: %u capsule geometry cases, %u transfer pose/portal-space cases, 2000 stow/draw/transfer/focus/tracking cycles, independent hand visibility\n",zones,poses);
}
