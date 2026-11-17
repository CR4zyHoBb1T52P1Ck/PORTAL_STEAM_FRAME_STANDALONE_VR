#include <Windows.h>
#include <cassert>
#include <fstream>
#include <vector>
#include <cstdio>
#include "bodyik.h"
#include "weaponpolish.h"
#include "weaponhandling.h"
#include "gripzone.h"
#include "studiomaterial.h"
#include "portalpose.h"
static void Near(float a,float b,float e=.003f){assert(std::abs(a-b)<e);}
static void Near(Vector a,Vector b,float e=.006f){assert((a-b).LengthSqr()<e*e);}
static void Near(const matrix3x4_t& a,const matrix3x4_t& b){for(int r=0;r<3;++r)for(int c=0;c<4;++c)Near(a[r][c],b[r][c]);}
static std::vector<unsigned char> Read(const char* path){std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv) {
    assert(argc==4);auto hands=Read(argv[1]),body=Read(argv[2]);
    const auto material=StudioMaterial::First(hands.data(),hands.size());
    assert(material=="models/weapons/v_models/v_hands/v_hands");
    const auto path=std::string(argv[3])+"/"+material+".vmt";auto vmt=Read(path.c_str());
    const std::string text(vmt.begin(),vmt.end());assert(text.find("models/weapons/v_models/v_portalgun/v_hands")!=std::string::npos);
    assert(StudioMaterial::First(hands.data(),100).empty());
    unsigned poses=0;
    const auto identity=GripZone::Point({0,0,0});
    for(float pitch:{-85.f,-30.f,0.f,90.f,180.f})for(float yaw:{-180.f,-90.f,0.f,90.f,180.f})for(float roll:{-180.f,0.f,180.f}) {
        const auto old=PortalPose::Frame({100,15,-80},{pitch,yaw,roll});
        const auto newHand=PortalPose::Frame({110,20,-75},{-pitch,yaw+45,roll-30});
        const auto calibration=WeaponHandling::Rebase(newHand,old);
        Near(HandPose::Concat(newHand,WeaponPolish::Settle(calibration,0)),old);
        Near(WeaponPolish::Settle(calibration,1),identity);
        float previous=INFINITY;
        for(int i=0;i<=20;++i) {
            const auto settled=WeaponPolish::Settle(calibration,i/20.f);
            const float distance=TwoHandAim::Position(settled).LengthSqr();assert(distance<=previous+.01f);previous=distance;
            for(int a=0;a<3;++a)for(int b=0;b<3;++b)Near(TwoHandAim::Dot(TwoHandAim::Axis(settled,a),TwoHandAim::Axis(settled,b)),a==b?1.f:0.f);
            for(int r=0;r<3;++r)for(int c=0;c<4;++c)assert(std::isfinite(settled[r][c]));++poses;
        }
    }
    WeaponPolish::Proximity hover;
    assert(hover.Update(true,5,6,1000));assert(!hover.Update(true,6.1f,6,1200));assert(hover.inside);
    assert(!hover.Update(true,8,6,1300));assert(!hover.inside);
    assert(!hover.Update(true,5,6,1400));assert(!hover.Update(false,5,6,1600));
    assert(hover.Update(true,5,6,1700));assert(!hover.Update(true,NAN,6,1800));
    unsigned limbs=0;
    for(float x:{-40.f,-15.f,0.f,1.f,15.f,40.f})for(float y:{-15.f,0.f,15.f})for(float z:{-20.f,0.f,20.f}) {
        const auto joint=BodyIK::TwoBone({0,0,0},{x,y,z},10,12,{0,0,-1});assert(joint.valid);
        Near(sqrtf(joint.elbow.LengthSqr()),10);Near(sqrtf((joint.end-joint.elbow).LengthSqr()),12);++limbs;
    }
    assert(!BodyIK::TwoBone({0,0,0},{NAN,0,0},10,10,{0,0,1}).valid);
    BodyIK::Rig rig;assert(rig.Read(body.data(),body.size())&&rig.count==68&&rig.eyeBone>=0);
    assert(rig.arm[0][2]==28&&rig.arm[1][2]==49);
    int offset;std::memcpy(&offset,body.data()+160,4);
    matrix3x4_t bind[128],world[128],posed[128];
    const auto transform=HandPose::Frame({1,0,0},{0,0,1},{0,-1,0},{100,200,0});
    for(int i=0;i<rig.count;++i){matrix3x4_t inv;std::memcpy(&inv,body.data()+offset+i*216+96,48);bind[i]=HandPose::InverseRigid(inv);world[i]=HandPose::Concat(transform,bind[i]);}
    unsigned bodies=0;
    for(float crouch:{0.f,-5.f,-12.f,-22.f})for(float reach:{.8f,1.f,1.1f,1.5f})for(float influence:{0.f,.5f,1.f}) {
        std::memcpy(posed,world,rig.count*sizeof(matrix3x4_t));
        const Vector target[2]={TwoHandAim::Position(world[28])+Vector(-2,-3,3),TwoHandAim::Position(world[49])+Vector(2,-4,3)};
        const Vector feet[2]={TwoHandAim::Position(world[5]),TwoHandAim::Position(world[13])};
        auto head=TwoHandAim::Position(world[rig.head]);head.z+=crouch;
        assert(rig.Solve(posed,head,target,{0,-1,0},reach,influence));
        Near(TwoHandAim::Position(posed[5]),feet[0]);Near(TwoHandAim::Position(posed[13]),feet[1]);
        if(influence==0)assert(!std::memcmp(posed,world,rig.count*sizeof(matrix3x4_t)));
        for(int i=0;i<rig.count;++i)for(int r=0;r<3;++r)for(int c=0;c<4;++c)assert(std::isfinite(posed[i][r][c]));
        for(int h=0;h<2;++h)for(int j=0;j<2;++j) {
            const int a=rig.arm[h][j],b=rig.arm[h][j+1];
            const float actual=sqrtf((TwoHandAim::Position(posed[b])-TwoHandAim::Position(posed[a])).LengthSqr());
            const float length=sqrtf((TwoHandAim::Position(world[b])-TwoHandAim::Position(world[a])).LengthSqr())*(influence==0?1:reach);
            Near(actual,length,.005f);
        }
        ++bodies;
    }
    printf("PASS: actual model material resolves to installed VMT; %u handoff settle poses; hover hysteresis; %u limb cases; %u native Chell IK poses with fixed animated feet and finite, preserved limb lengths\n",poses,limbs,bodies);
}
