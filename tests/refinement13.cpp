#include <Windows.h>
#include <cassert>
#include <fstream>
#include <vector>
#include <cstdio>
#include "bodyik.h"
#include "gungrab.h"
#include "menuimage.h"
#include "menuraster.h"
static void Near(float a,float b,float e=.006f){assert(std::abs(a-b)<e);}
static void Near(Vector a,Vector b,float e=.01f){assert((a-b).LengthSqr()<e*e);}
static Vector Point(const matrix3x4_t& m,Vector p){return TwoHandAim::Position(HandPose::Concat(m,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},p)));}
static std::vector<unsigned char> Read(const char* path){std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
struct Vertex {float weights[3];unsigned char bones[3],count;Vector p,normal;float uv[2];};
static Vector Skin(const BodyIK::Rig& rig,const matrix3x4_t* pose,const Vertex& v){Vector p(0,0,0);for(int j=0;j<v.count;++j)p+=Point(HandPose::Concat(pose[v.bones[j]],HandPose::InverseRigid(rig.bind[v.bones[j]])),v.p)*v.weights[j];return p;}
int main(int argc,char** argv) {
    assert(argc==4);const auto mdl=Read(argv[1]),vvd=Read(argv[2]);BodyIK::Rig rig;assert(rig.Read(mdl.data(),mdl.size()));
    matrix3x4_t open[128],closed[128],half[128];const float zero[5]{},one[5]={1,1,1,1,1},mid[5]={.5f,.5f,.5f,.5f,.5f};
    std::memcpy(open,rig.bind,rig.count*48);std::memcpy(closed,rig.bind,rig.count*48);std::memcpy(half,rig.bind,rig.count*48);
    for(int h=0;h<2;++h) {
        rig.Curl(open,h,zero,false);rig.Curl(closed,h,one,false);rig.Curl(half,h,mid,false);
        for(int f=0;f<5;++f)for(int s=0;s<3;++s) {
            const int b=rig.fingers[h][f][s],p=rig.parents[b];
            Near(TwoHandAim::Position(open[b]),TwoHandAim::Position(rig.bind[b]));
            const float length=sqrtf((TwoHandAim::Position(rig.bind[b])-TwoHandAim::Position(rig.bind[p])).LengthSqr());
            Near(sqrtf((TwoHandAim::Position(closed[b])-TwoHandAim::Position(closed[p])).LengthSqr()),length);
        }
        for(int f=1;f<5;++f) {
            const int b=rig.fingers[h][f][2],p=rig.fingers[h][f][1];
            const auto tip=TwoHandAim::Position(rig.bind[b])+(TwoHandAim::Position(rig.bind[b])-TwoHandAim::Position(rig.bind[p]))*.7f;
            const auto full=Point(HandPose::Concat(closed[b],HandPose::InverseRigid(rig.bind[b])),tip);
            const auto center=TwoHandAim::Position(rig.bind[rig.arm[h][2]]);
            const float ratio=sqrtf((full-center).LengthSqr()/(tip-center).LengthSqr());
            printf("Fist h=%d digit=%d tip-to-wrist ratio=%f\n",h,f,ratio);assert(ratio<.7f);
        }
    }
    // Actual native skin: a 180-degree wrist roll used to blend opposing
    // matrices to zero radius. The intermediate twist bone must retain volume.
    int n,start;std::memcpy(&n,vvd.data()+16,4);std::memcpy(&start,vvd.data()+56,4);assert(n>20000);
    unsigned checked=0;double oldRadius=0,newRadius=0;
    for(int h=0;h<2;++h) {
        const int wrist=rig.arm[h][2],twist=rig.wristTwist[h];assert(twist>=0);
        auto axis=TwoHandAim::Position(rig.bind[wrist])-TwoHandAim::Position(rig.bind[rig.arm[h][1]]);assert(TwoHandAim::Unit(axis));
        const auto rotation=HandPose::RotateAroundAxis(axis,3.14159265358979323846f);
        auto desired=HandPose::Concat(rotation,rig.bind[wrist]);for(int r=0;r<3;++r)desired[r][3]=rig.bind[wrist][r][3];
        std::memcpy(open,rig.bind,rig.count*48);std::memcpy(closed,rig.bind,rig.count*48);
        rig.ReanchorBranch(open,wrist,desired);rig.ReanchorBranch(closed,wrist,desired);
        PoseTwist::Distribute(closed[twist],rig.bind[wrist],desired,axis,.5f);
        Near(TwoHandAim::Position(closed[twist]),TwoHandAim::Position(rig.bind[twist]));
        const auto center=TwoHandAim::Position(rig.bind[wrist]);
        for(int i=0;i<n;++i) {
            Vertex v;std::memcpy(&v,vvd.data()+start+i*48,48);bool only=true;float w=0,t=0;
            for(int j=0;j<v.count;++j)if(v.weights[j]>.0001f){only&=v.bones[j]==wrist||v.bones[j]==twist;if(v.bones[j]==wrist)w+=v.weights[j];if(v.bones[j]==twist)t+=v.weights[j];}
            if(!only||w<.15f||t<.15f)continue;
            auto radial=[&](Vector p){p-=center;p-=axis*TwoHandAim::Dot(p,axis);return sqrtf(p.LengthSqr());};
            const float rest=radial(v.p);if(rest<.01f)continue;
            oldRadius+=radial(Skin(rig,open,v))/rest;newRadius+=radial(Skin(rig,closed,v))/rest;++checked;
        }
    }
    printf("Wrist weighted vertices=%u radius old=%f new=%f\n",checked,oldRadius/std::max(checked,1u),newRadius/std::max(checked,1u));assert(checked>10&&newRadius>oldRadius*1.2);
    unsigned captures=0;
    for(int hand=0;hand<2;++hand)for(float yaw:{-170.f,0.f,125.f})for(float roll:{-150.f,0.f,170.f})for(float scale:{30.f,43.2f,70.f}) {
        Vector f,r,u;QAngle::AngleVectors({35,yaw,roll},&f,&r,&u);const auto gun=HandPose::Frame(-r,u,f,{100,200,70});
        const Vector offset(3,-2,5);const auto calibrated=GunGrab::Apply(gun,offset,scale);
        const auto touch=Point(calibrated,Vector(hand?2.f:-2.f,3,4)),holding=TwoHandAim::Position(gun);Vector next;
        assert(GunGrab::Capture(calibrated,holding,touch,scale,offset,next));
        const auto moved=GunGrab::Apply(gun,next,scale);Near(Point(moved,Vector(hand?2.f:-2.f,3,4)),holding);
        Vector invalid;assert(!GunGrab::Capture(gun,holding,holding+Vector(1000,0,0),scale,offset,invalid));
        const auto portal=HandPose::Concat(HandPose::RotateAroundAxis({0,0,1},yaw*.0174532925f),HandPose::Frame({1,0,0},{0,1,0},{0,0,1},{200,30,70}));
        const auto world=HandPose::Concat(moved,GunGrab::WorldFit());
        const auto actual=GunGrab::InPass(gun,HandPose::Concat(portal,gun),world),expected=HandPose::Concat(portal,world);
        for(int a=0;a<3;++a)for(int b=0;b<4;++b)Near(actual[a][b],expected[a][b],.01f);++captures;
    }
    // Lossless file transport round-trip, including packet boundaries/runs,
    // color/alpha, full panel dimensions, native Wine path conversion.
    std::vector<uint32_t> pixels(1200*1060,0xff302010);for(size_t i=0;i<pixels.size();i+=7)pixels[i]=uint32_t(i)*0x01943219;
    const auto tga=MenuImage::Tga(pixels,1200,1060);std::vector<uint32_t> decoded;size_t at=18;
    while(at<tga.size()){const unsigned packet=tga[at++],count=(packet&127)+1;auto read=[&](){assert(at+4<=tga.size());const uint32_t b=tga[at++],g=tga[at++],r=tga[at++],a=tga[at++];return r|(g<<8)|(b<<16)|(a<<24);};if(packet&128){const auto p=read();decoded.insert(decoded.end(),count,p);}else for(unsigned i=0;i<count;++i)decoded.push_back(read());}
    assert(decoded==pixels);assert(MenuImage::Tga({},1200,1060).empty());assert(MenuImage::NativePath("Z:\\home\\steamos\\menu.tga")=="/home/steamos/menu.tga");assert(MenuImage::NativePath("C:\\test.tga")=="C:\\test.tga");
    MenuRaster::Swap swap;assert(swap.Begin(100,0));swap.Failed(100);for(unsigned i=101;i<600;++i)assert(!swap.Begin(i,0));assert(swap.Begin(600,0));assert(swap.Loaded()&&swap.AcceptsClicks(0));
    assert(swap.Begin(1000,1));swap.Failed(1000);assert(swap.ready&&swap.shownPage==0&&!swap.AcceptsClicks(1));assert(swap.Begin(1500,1));assert(swap.Loaded()&&swap.AcceptsClicks(1));
    // Write actual closed-fist palette for the private CPU skin preview.
    std::memcpy(closed,rig.bind,rig.count*48);for(int h=0;h<2;++h)rig.Curl(closed,h,one,false);
    std::ofstream pose(argv[3],std::ios::binary);assert(pose);pose.write(reinterpret_cast<const char*>(&rig.count),4);pose.write(reinterpret_cast<const char*>(closed),rig.count*48);pose.write(reinterpret_cast<const char*>(&closed[rig.arm[0][2]]),48);pose.write(reinterpret_cast<const char*>(&closed[rig.arm[1][2]]),48);
    printf("PASS: native complete fists, preserved bone lengths, native weighted wrist-roll volume; %u either-hand calibration/portal cases; full-resolution lossless menu image/recovery checks\n",captures);
}
