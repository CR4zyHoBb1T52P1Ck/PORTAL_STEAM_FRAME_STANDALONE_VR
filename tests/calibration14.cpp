#include <Windows.h>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
#include "portalpose.h"
#include "bodycalibration.h"
#include "bodyik.h"
#include "holsterfit.h"
#include "gungrab.h"
static void Near(float a,float b,float e=.01f){assert(std::abs(a-b)<e);}
static void Near(Vector a,Vector b,float e=.01f){assert((a-b).LengthSqr()<e*e);}
static Vector Point(const matrix3x4_t& m,Vector p){return {m[0][0]*p.x+m[0][1]*p.y+m[0][2]*p.z+m[0][3],m[1][0]*p.x+m[1][1]*p.y+m[1][2]*p.z+m[1][3],m[2][0]*p.x+m[2][1]*p.y+m[2][2]*p.z+m[2][3]};}
int main(int argc,char** argv) {
    assert(argc==4);std::ifstream f(argv[1],std::ios::binary);std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)),{});BodyIK::Rig rig;assert(rig.Read(data.data(),data.size()));
    std::ifstream mesh(argv[3],std::ios::binary);std::vector<unsigned char> vvd((std::istreambuf_iterator<char>(mesh)),{});assert(vvd.size()>64);
    struct Vertex {float weights[3];unsigned char bones[3],count;Vector p,normal;float uv[2];};
    int vertices,start;std::memcpy(&vertices,vvd.data()+16,4);std::memcpy(&start,vvd.data()+56,4);assert(vertices>20000&&size_t(start)+vertices*48<=vvd.size());
    unsigned weighted=0;
    unsigned captures=0,poses=0,zones=0;
    for(float height:{1.3f,1.65f,2.f})for(float span:{1.2f,1.5f,1.9f})for(float yaw:{-170.f,0.f,110.f}) {
        const auto frame=PortalPose::Frame({10,-20,0},{0,yaw,0});const Vector head=Point(frame,{0,0,height}),left=Point(frame,{0,span*.5f,height-.23f}),right=Point(frame,{0,-span*.5f,height-.23f});
        const auto forward=TwoHandAim::Axis(frame,0);BodyCalibration::Measurements m;
        assert(BodyCalibration::Measure(head,left,right,forward,m));Near(m.eyeCM,height*100);Near(m.spanCM,span*100,.02f);Near(m.dropCM,23);
        BodyCalibration::Capture capture;capture.Start();
        for(int i=0;i<100;++i)assert(!capture.Step(.02f,head,left,right,forward,true,true,true)); // held click cannot auto-capture
        assert(!capture.Step(.02f,head,left,right,forward,true,false,false));
        bool captured=false;for(int i=0;i<55;++i)captured|=capture.Step(.02f,head,left,right,forward,true,true,true);
        assert(captured&&capture.review&&!capture.active);Near(capture.measured.eyeCM,m.eyeCM);++captures;
        capture.Start();capture.Step(.02f,head,left,right,forward,true,false,false);
        for(int i=0;i<20;++i)assert(!capture.Step(.02f,head,left,right,forward,true,true,true));
        assert(!capture.Step(.02f,head,left,right,forward,false,true,true));assert(capture.elapsed==0);
        capture.Cancel();assert(!capture.active&&!capture.review);
        assert(!BodyCalibration::Measure(head,Point(frame,{.7f,.1f,height-.2f}),Point(frame,{.7f,-.1f,height-.2f}),forward,m));
    }
    const auto model=HandPose::Frame({1,0,0},{0,0,1},{0,-1,0},{100,200,0});
    matrix3x4_t native[128],fitted[128],posed[128],thick[128];for(int i=0;i<rig.count;++i)native[i]=HandPose::Concat(model,rig.bind[i]);
    for(float span:{120.f,150.f,190.f})for(float width:{30.f,40.f,55.f})for(float crouch:{0.f,-12.f}) {
        std::memcpy(fitted,native,rig.count*48);auto eyes=rig.Eyes(native);eyes.z+=crouch;
        const float reach=rig.FitUpperBody(fitted,eyes,{0,-1,0},43.2f,span,width,23,4,0,1);
        const auto center=eyes-Vector(0,-1,0)*(4*.432f)-Vector(0,0,23*.432f);
        Near((TwoHandAim::Position(fitted[rig.arm[0][0]])+TwoHandAim::Position(fitted[rig.arm[1][0]]))*.5f,center);
        Near(sqrtf((TwoHandAim::Position(fitted[rig.arm[0][0]])-TwoHandAim::Position(fitted[rig.arm[1][0]])).LengthSqr()),width*.432f);
        matrix3x4_t hands[2];for(int h=0;h<2;++h)hands[h]=HandPose::Frame({0,-1,0},{1,0,0},{0,0,1},center+Vector(h?-width*.216f:width*.216f,-12,-3));
        const bool gripping[2]{};const float curl[5]={1,1,1,1,1};const float* curls[2]={curl,curl};
        std::memcpy(posed,fitted,rig.count*48);assert(rig.SolveNative(posed,TwoHandAim::Position(posed[rig.head]),hands,{0,-1,0},reach,1,true,curls,gripping));
        for(int h=0;h<2;++h){Near(TwoHandAim::Position(posed[rig.arm[h][2]]),TwoHandAim::Position(hands[h]));Near(TwoHandAim::Position(posed[rig.leg[h][2]]),TwoHandAim::Position(native[rig.leg[h][2]]));}
        std::memcpy(thick,posed,rig.count*48);rig.Volume(thick,1.05f,1.1f,1.12f,1.02f,1.05f);
        for(int h=0;h<2;++h) {
            const int w=rig.arm[h][2];Near(TwoHandAim::Position(thick[w]),TwoHandAim::Position(posed[w]));
            const auto origin=TwoHandAim::Position(posed[w]);Vector axis=origin-TwoHandAim::Position(posed[rig.arm[h][1]]);assert(TwoHandAim::Unit(axis));
            auto transverse=CrossProduct(axis,{0,0,1});if(!TwoHandAim::Unit(transverse)){transverse={1,0,0};}
            const Vector testPoint=origin+transverse;
            const auto skin=HandPose::Concat(thick[w],HandPose::InverseRigid(posed[w]));
            Near(sqrtf((Point(skin,testPoint)-origin).LengthSqr()),1.12f*1.02f);
            // Entire closed hand uses one affine map, preserving the fist
            // instead of stretching/repositioning each digit independently.
            for(int digit=0;digit<5;++digit)for(int s=0;s<3;++s) {
                const int b=rig.fingers[h][digit][s];Near(Point(skin,TwoHandAim::Position(posed[b])),TwoHandAim::Position(thick[b]),.025f);
            }
        }
        for(int i=0;i<rig.count;++i)for(int a=0;a<3;++a)for(int b=0;b<4;++b)assert(std::isfinite(thick[i][a][b]));++poses;
        for(int i=0;i<vertices;++i) {
            Vertex v;std::memcpy(&v,vvd.data()+start+i*48,48);assert(v.count>=1&&v.count<=3);Vector p(0,0,0),q(0,0,0);
            for(int j=0;j<v.count;++j){const int b=v.bones[j];assert(b<rig.count);const auto inverse=HandPose::InverseRigid(rig.bind[b]);p+=Point(HandPose::Concat(posed[b],inverse),v.p)*v.weights[j];q+=Point(HandPose::Concat(thick[b],inverse),v.p)*v.weights[j];}
            assert(TwoHandAim::Finite(q)&&(p-q).LengthSqr()<25);++weighted;
        }
    }
    for(int side=0;side<2;++side)for(float yaw:{-170.f,0.f,135.f})for(float scale:{30.f,43.2f,70.f}) {
        const auto frame=PortalPose::Frame({123,80,45},{0,yaw,0});const auto forward=TwoHandAim::Axis(frame,0),right=-TwoHandAim::Axis(frame,1);const auto head=TwoHandAim::Position(frame);
        const Vector offset(-8,side?6.f:-6.f,4);
        const auto base=HolsterFit::Center(head,forward,right,side,22,15,15,{0,0,0},scale/100);
        const auto desired=HolsterFit::Center(head,forward,right,side,22,15,15,offset,scale/100);Vector captured;
        assert(HolsterFit::Capture(desired,base,forward,right,scale/100,captured));Near(captured,offset);
        Near(HolsterFit::Center(head,forward,right,side,22,15,15,captured,scale/100),desired);
        assert(!HolsterFit::Capture(desired+Vector(1000,0,0),base,forward,right,scale/100,captured));++zones;
        // NULL-palette world draws use the model transform supplied by Source.
        const auto linked=PortalPose::Frame({-200,300,50},{0,180,0});
        const auto pass=HandPose::Concat(linked,frame),tracked=HandPose::Concat(frame,GunGrab::WorldFit());
        const auto result=GunGrab::InPass(frame,pass,tracked);
        for(Vector p:{Vector(0,0,0),Vector(2,3,7),Vector(-5,1,17)})Near(Point(result,p),Point(linked,Point(tracked,p)),.025f);
    }
    // Preview after body skin controls, CPU only.
    std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<char*>(&rig.count),4);out.write(reinterpret_cast<char*>(thick),rig.count*48);
    out.write(reinterpret_cast<char*>(&thick[rig.arm[0][2]]),48);out.write(reinterpret_cast<char*>(&thick[rig.arm[1][2]]),48);
    printf("PASS: %u stable T-pose captures plus held/invalid/cancel guards; %u native fitted-body/closed-fist volume poses and %u weighted mesh checks; %u independent holster samples and NULL-palette portal model cases\n",captures,poses,weighted,zones);
}
