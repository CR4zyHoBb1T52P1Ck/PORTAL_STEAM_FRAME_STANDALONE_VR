#include <Windows.h>
#include <cassert>
#include <fstream>
#include <cstdio>
#include "vrmenupaint.h"
#include "propholdinput.h"
#include "propinteraction.h"
#include "bodyik.h"
#include "fingersurface.h"
#include "worldpush.h"
#include "creatorcamera.h"
#include "overlaypixels.h"
static void Near(Vector a,Vector b,float e=.003f){assert((a-b).LengthSqr()<e*e);}
static void Preview(const char* folder,VrMenu::Page page,const VrGraphics::Settings& settings){
    const auto pixels=VrMenu::Paint(settings,page,0,"INTERACTION-26 CPU PREVIEW","Unreleased test build; no game or GPU test running.","Native world demos do not capture injected VR body/finger motion.");
    BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};file.bfType=0x4d42;file.bfOffBits=54;file.bfSize=54+VrMenu::Width*VrMenu::Height*4;
    info.biSize=40;info.biWidth=VrMenu::Width;info.biHeight=-VrMenu::Height;info.biPlanes=1;info.biBitCount=32;
    std::vector<uint32_t> bgra(pixels.size());assert(OverlayPixels::Copy(pixels.data(),VrMenu::Width,VrMenu::Height,bgra.data(),VrMenu::Width*4));
    const auto path=std::string(folder)+"/menu-"+std::to_string(int(page))+".bmp";std::ofstream out(path,std::ios::binary);assert(out);out.write(reinterpret_cast<char*>(&file),14);out.write(reinterpret_cast<char*>(&info),40);out.write(reinterpret_cast<char*>(bgra.data()),bgra.size()*4);
}
int main(int argc,char** argv){
    assert(argc==2||argc==3);VrGraphics::Settings settings;
    assert(settings.autoPalmStowed&&settings.propTransfer&&settings.propSurfacePose&&settings.gunSurfacePose&&!settings.sandboxForces);
    assert(settings.grabXCM==-2&&settings.grabYCM==-3&&settings.grabZCM==-3.5f);
    for(int p=0;p<VrMenu::PageCount;++p){const auto rows=VrMenu::Rows(settings,VrMenu::Page(p));if(rows.size()>12)printf("Overflow %s rows=%u\n",VrMenu::Title(VrMenu::Page(p)),unsigned(rows.size()));assert(rows.size()<=12);assert(!rows.empty());}
    if(argc==3)for(auto p:{VrMenu::Home,VrMenu::PropRouting,VrMenu::GunSurface,VrMenu::Sandbox,VrMenu::Creator,VrMenu::CreatorCamera,VrMenu::Replay})Preview(argv[2],p,settings);
    // A simultaneous giver release / receiver squeeze transfers ownership
    // without a native detach, IN_USE gap or throw from the old hand.
    for(int first=0;first<2;++first)for(bool simultaneous:{false,true}){
        PropHold::Input input;bool grips[2]{},tracked[2]={true,true},eligible[2]={true,true};
        input.Step(true,grips,tracked,eligible);grips[first]=true;input.Step(true,grips,tracked,eligible);assert(input.hand==first);input.Attach();
        grips[1-first]=true;if(simultaneous)grips[first]=false;input.Step(true,grips,tracked,eligible,true);
        assert(input.hand==1-first&&input.attached&&input.use&&!input.release&&!input.throwRelease);
        grips[first]=false;input.Step(true,grips,tracked,eligible,true);assert(!input.release);
        grips[1-first]=false;input.Step(true,grips,tracked,eligible,true);assert(input.release&&input.throwRelease);
    }
    // Capture an actual surface, not a fixed centre distance. A large prop's
    // near surface remains at the palm across rotations, and a blocked native
    // object determines the visible hand instead of penetrating a wall.
    int captures=0;
    for(float yaw:{-170.f,0.f,170.f})for(float roll:{-80.f,0.f,80.f})for(float radius:{1.f,8.f,20.f}){
        const auto hand=PortalPose::Frame({100,200,300},{20,yaw,roll});const auto object=HandPose::Concat(hand,PortalPose::Frame({radius+7,0,0},{0,30,10}));
        const auto surface=HandPose::Concat(hand,PortalPose::Frame({7,0,0},{0,0,0}));
        PropInteraction::Carry carry;carry.Target((void*)1,(void*)2,1,0,1,hand,object,43.2f,-1);carry.Surface(PortalPose::Position(surface),hand,object,0);
        const auto target=carry.Target((void*)1,(void*)2,1,0,1.03,hand,object,43.2f,-1);
        const auto contactLocal=HandPose::Concat(HandPose::InverseRigid(object),surface);
        Near(PortalPose::Position(HandPose::Concat(target,contactLocal)),PortalPose::Position(hand));
        const auto visual=HandPose::Concat(target,carry.palmFromObject);Near(PortalPose::Position(visual),PortalPose::Position(hand));++captures;
    }
    // Equal one-second sustained pressure across query rates, no velocity gate.
    float reference=0;
    for(float hz:{15.f,30.f,60.f}){Vector impulse{0,0,0};for(int n=0;n<int(hz);++n)impulse+=WorldPush::Pressure({9,0,0},{10,0,0},{-1,0,0},{0,0,0},2,43.2f,3500,300,1/hz);
        const float total=sqrtf(impulse.LengthSqr());assert(total>0);if(reference)assert(std::abs(reference-total)<.05f);reference=total;}
    Near(WorldPush::Pressure({0,0,0},{0,0,0},{0,0,0},{0,0,0},2,43.2f,3500,300,.03f),{0,0,0});
    const auto capped=WorldPush::Pressure({20,0,0},{0,0,0},{-1,0,0},{100,0,0},2,43.2f,120000,4000,.1f);assert(sqrtf(capped.LengthSqr())<=43.2f*400.01f);
    // Real Chell rig: finger surface solving preserves every segment length,
    // both wrists, all non-hand bones and controller/gun transforms.
    std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> mdl{std::istreambuf_iterator<char>(file),{}};BodyIK::Rig rig;assert(rig.Read(mdl.data(),mdl.size()));
    matrix3x4_t bones[BodyIK::Max];std::memcpy(bones,rig.bind,sizeof(bones));
    int fingerCases=0;
    for(int h=0;h<2;++h)for(float curl:{0.f,.5f,1.f}){
        std::memcpy(bones,rig.bind,sizeof(bones));float values[5]={curl,curl,curl,curl,curl};FingerSurface::Shape shape;shape.gun=false;
        shape.frame=PortalPose::Frame(PortalPose::Position(bones[rig.fingers[h][2][1]]),{0,0,0});shape.radii={2,2,2};
        const auto wrist=bones[rig.arm[h][2]];rig.SurfaceFingers(bones,h,values,1.5f,.65f,.7f,shape,.1f);
        assert(!memcmp(&wrist,&bones[rig.arm[h][2]],sizeof(wrist)));
        for(int f=0;f<5;++f)for(int s=0;s<3;++s){const int b=rig.fingers[h][f][s],parent=s?rig.fingers[h][f][s-1]:rig.arm[h][2];
            const float before=sqrtf((PortalPose::Position(rig.bind[b])-PortalPose::Position(rig.bind[parent])).LengthSqr());
            const float after=sqrtf((PortalPose::Position(bones[b])-PortalPose::Position(bones[parent])).LengthSqr());assert(std::abs(before-after)<.005f);}
        ++fingerCases;
    }
    // A fully curled fingertip can emerge on the far side of a surface.
    // Stop at first contact along the curl path, not just at the final pose.
    matrix3x4_t synthetic[4];synthetic[0]=PortalPose::Frame({0,0,0},{0,0,0});
    const int chain[3]={1,2,3};matrix3x4_t local[3];Vector axes[3]={{0,0,1},{0,0,1},{0,0,1}};
    for(int i=0;i<3;++i)local[i]=PortalPose::Frame({1,0,0},{0,0,0});
    FingerSurface::Shape obstruction;obstruction.frame=PortalPose::Frame({1.6f,.7f,0},{0,0,0});obstruction.radii={.4f,.4f,.4f};
    const float stopped=FingerSurface::Clamp(synthetic,0,chain,local,axes,2,1,0,obstruction,.05f);assert(stopped<.9f&&stopped>=0);
    for(int i=1;i<=3;++i){const auto start=PortalPose::Position(synthetic[i]);const auto end=i<3?PortalPose::Position(synthetic[i+1]):start+(start-PortalPose::Position(synthetic[2]))*.65f;
        for(float t:{.15f,.5f,.85f,1.f})assert(obstruction.Distance(start+(end-start)*t)>.049f);}
    CreatorCamera::State camera;Vector head{10,20,30},out;QAngle eye{0,90,0},angle;camera.Reset(head,eye);
    camera.View(1,head,eye,43.2f,2,.5f,12,1,out,angle);assert(std::abs(sqrtf((out-head).LengthSqr())-sqrtf(powf(86.4f,2)+powf(21.6f,2)))<.001f);
    camera.Move(1,0,0,43.2f,.25f);camera.View(3,head,eye,43.2f,2,.5f,12,2,out,angle);Near(out,head+Vector(0,10.8f,0));
    assert(CreatorCamera::TakeName("portalvr_take_123_456"));assert(!CreatorCamera::TakeName("portalvr_take_bad;quit"));assert(!CreatorCamera::TakeName("../../autoexec"));
    printf("Interaction26 CPU: all %d menus fit; both-hand release/transfer; %d surface captures; rate-independent pressure; %d actual Chell contact poses; independent camera and safe take identifiers. No game/GPU launched.\n",VrMenu::PageCount,captures,fingerCases);
}
