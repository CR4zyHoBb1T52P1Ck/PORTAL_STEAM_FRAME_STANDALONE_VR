#include <Windows.h>
#include <cassert>
#include <fstream>
#include <vector>
#include <cstdio>
#include "bodyik.h"
#include "modelpath.h"
#include "modeldrawstate.h"
#include "portalpose.h"
static Vector Point(const matrix3x4_t& m,Vector p){return {m[0][0]*p.x+m[0][1]*p.y+m[0][2]*p.z+m[0][3],m[1][0]*p.x+m[1][1]*p.y+m[1][2]*p.z+m[1][3],m[2][0]*p.x+m[2][1]*p.y+m[2][2]*p.z+m[2][3]};}
static std::vector<unsigned char> Read(const char* name){std::ifstream f(name,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    assert(argc==4);const auto data=Read(argv[1]),world=Read(argv[2]);BodyIK::Rig rig;assert(rig.Read(data.data(),data.size()));
    const auto eyeBind=HandPose::Concat(rig.bind[rig.eyeBone],rig.eyeLocal);
    const float facing=TwoHandAim::Dot(TwoHandAim::Axis(eyeBind,0),TwoHandAim::Axis(rig.bodyBindFrame,0));
    printf("Native torso / eyes facing dot=%f\n",facing);assert(facing>.7f);
    assert(ModelPath::Equal(reinterpret_cast<const char*>(world.data()+12),"weapons/w_portalgun.mdl"));
    assert(ModelPath::Equal("MODELS\\Weapons\\W_PortalGun.mdl","models/weapons/w_portalgun.mdl"));
    assert(!ModelPath::Equal("weapons/w_portalgun.mdl.extra","weapons/w_portalgun.mdl"));
    matrix3x4_t first{},second{};int h=0,r=0,hardware=0;ModelDrawState::State state{&h,&hardware,&r,&first,17,0,23,2},copy;
    assert(ModelDrawState::Copy(&state,copy,&h,&r,second));assert(copy.model==&second&&state.model==&first&&copy.hardware==state.hardware&&copy.decals==17&&copy.flags==23&&copy.lod==2);
    assert(!ModelDrawState::Copy(&state,copy,&r,&r,second));assert(!ModelDrawState::Copy(&state,copy,&h,&h,second));
    const auto model=HandPose::Frame({1,0,0},{0,0,1},{0,-1,0},{100,200,0});matrix3x4_t base[128],posed[128];
    for(int i=0;i<rig.count;++i)base[i]=HandPose::Concat(model,rig.bind[i]);
    unsigned poses=0;float maxError=0;
    for(float pitch:{-60.f,0.f,60.f})for(float roll:{-150.f,0.f,150.f})for(float size:{.76f,1.f,1.3f})for(float wrist:{1.f,1.6f}) {
        std::memcpy(posed,base,rig.count*48);const auto eye=rig.Eyes(base);
        const float reach=rig.FitUpperBody(posed,eye,{0,-1,0},43.2f,166,42,24,4,0,1,0,true,true,size,1);
        matrix3x4_t hands[2];
        for(int h=0;h<2;++h){Vector f,r,u;QAngle::AngleVectors({pitch,h?30.f:-30.f,roll},&f,&r,&u);hands[h]=HandPose::Frame(f,h?-r:r,h?-u:u,eye+Vector(h?-14.f:14.f,-12,-15));}
        const bool grip[2]{};const float closed[5]={1,1,1,1,1};const float* curls[2]={closed,closed};
        assert(rig.SolveNative(posed,TwoHandAim::Position(posed[rig.head]),hands,{0,-1,0},reach,1,true,curls,grip,nullptr,1,.5f,true,size,wrist,1));
        rig.Volume(posed,1.05f,1.24f,wrist,size,1,3.5f);
        for(int h=0;h<2;++h){const auto palm=Point(posed[rig.arm[h][2]],rig.gripLocal[h]);const float error=sqrtf((palm-TwoHandAim::Position(hands[h])).LengthSqr());maxError=std::max(maxError,error);assert(error<.08f);}
        for(int i=0;i<rig.count;++i)for(int a=0;a<3;++a)for(int b=0;b<4;++b)assert(std::isfinite(posed[i][a][b]));++poses;
    }
    // A source eye-height mismatch must not invent a permanent crouch. Zero
    // measured physical crouch leaves the hip at its native animated height.
    std::memcpy(posed,base,rig.count*48);const auto hips=TwoHandAim::Position(posed[rig.hips]);auto low=rig.Eyes(base);low.z-=18;
    rig.FitUpperBody(posed,low,{0,-1,0},43.2f,166,42,24,4,0,1,0);
    assert((TwoHandAim::Position(posed[rig.hips])-hips).LengthSqr()<1e-6f);
    std::ofstream out(argv[3],std::ios::binary);out.write(reinterpret_cast<char*>(&rig.count),4);out.write(reinterpret_cast<char*>(posed),rig.count*48);out.write(reinterpret_cast<char*>(&posed[rig.arm[0][2]]),48);out.write(reinterpret_cast<char*>(&posed[rig.arm[1][2]]),48);
    printf("PASS: actual retail backslash world-gun path; %u palm/grip poses across wrist width/hand size/roll, max palm error=%f Source units; 350%% cuff finite; no synthetic standing crouch\n",poses,maxError);
}
