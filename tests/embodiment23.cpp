#include <Windows.h>
#include <cassert>
#include <fstream>
#include <map>
#include <set>
#include <cstdio>
#include "vrmenupaint.h"
#include "reticle.h"
#include "overlaypixels.h"
#include "triggercolors.h"
#include "bodysecondary.h"
#include "worldpush.h"
#include "nativephysics.h"
#include "bodyik.h"
#include "guncontact.h"
#include "propholdinput.h"
#include "avatarpalette.h"
static void Near(Vector a,Vector b,float e=.002f){assert((a-b).LengthSqr()<e*e);}
static void Bitmap(const char* name,const std::vector<uint32_t>& pixels,int w,int h) {
    BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};file.bfType=0x4d42;file.bfOffBits=54;file.bfSize=54+w*h*4;
    info.biSize=40;info.biWidth=w;info.biHeight=-h;info.biPlanes=1;info.biBitCount=32;
    std::vector<uint32_t> bgra(pixels.size());assert(OverlayPixels::Copy(pixels.data(),w,h,bgra.data(),w*4));
    std::ofstream out(name,std::ios::binary);assert(out);out.write(reinterpret_cast<char*>(&file),14);out.write(reinterpret_cast<char*>(&info),40);out.write(reinterpret_cast<char*>(bgra.data()),bgra.size()*4);
}
int main(int argc,char** argv) {
    assert(argc==4);VrGraphics::Settings settings;assert(!settings.handPush&&!settings.gunPush&&!settings.jiggle&&!settings.crosshair);
    std::set<std::string> keys;
    for(const auto& option:VrMenu::Options()){assert(keys.insert(option.key).second);VrMenu::Set(settings,option,option.high);}
    const auto saved=VrMenu::Save(settings,"KeepOwnerSetting=17\n");std::istringstream lines(saved);std::map<std::string,std::string> values;std::string line;
    while(std::getline(lines,line)){auto at=line.find('=');if(at!=std::string::npos)values[line.substr(0,at)]=line.substr(at+1);}
    VrGraphics::Settings restored;VrMenu::Load(restored,values);assert(values["KeepOwnerSetting"]=="17");
    for(const auto& option:VrMenu::Options())assert(std::abs(VrMenu::Get(settings,option)-VrMenu::Get(restored,option))<.001);
    for(int page=0;page<VrMenu::PageCount;++page){const auto rows=VrMenu::Rows(settings,VrMenu::Page(page));assert(rows.size()<=12);assert(rows.back().action==VrMenu::Back||rows.back().action==VrMenu::Resume);}
    for(auto page:{VrMenu::Home,VrMenu::Crosshair,VrMenu::Fingers,VrMenu::Jiggle,VrMenu::PhysicalContact,VrMenu::ImpactTuning,VrMenu::GunCollision,VrMenu::PropPose,VrMenu::BodyShape}) {
        const auto pixels=VrMenu::Paint(settings,page,0,"EMBODIMENT-23 CPU MENU PREVIEW","Physical trigger colors are independent of gun ownership.","Unreleased test build; no game running.");
        const auto name=std::string(argv[3])+"/menu-"+std::to_string(int(page))+".bmp";Bitmap(name.c_str(),pixels,VrMenu::Width,VrMenu::Height);
    }
    for(bool left:{false,true})for(bool swapped:{false,true})for(bool primary:{false,true}) {
        const auto physical=TriggerColors::Physical(primary,swapped);const auto legacy=TriggerColors::LegacyPrimary(physical,left);
        assert((legacy!=left)==(physical==1));assert(physical==(primary!=swapped?1:0));
    }
    for(int mode=1;mode<=3;++mode)for(int color=0;color<5;++color)for(float alpha:{.1f,.5f,1.f}) {
        const auto pixels=Reticle::Pixels(mode,color,alpha);assert(pixels.size()==128*128&&pixels[0]==0);
        bool any=false;for(auto pixel:pixels){assert((pixel>>24)<=uint32_t(alpha*255));any|=pixel!=0;}assert(any);
        assert((mode==1)==(pixels[64*128+64]!=0));
    }
    const auto cyan=Reticle::Pixels(1,1,1)[64*128+64];assert((cyan&255)==0x40&&((cyan>>16)&255)==0xff);
    int optical=0;
    for(float yaw:{-175.f,0.f,175.f})for(float pitch:{-60.f,0.f,60.f})for(float roll:{-45.f,0.f,45.f}) {
        Vector f,r,u;QAngle::AngleVectors({pitch,yaw,roll},&f,&r,&u);Near(Reticle::TrackingOffset((f*2+r*.4f+u*.2f)*43.2f,f,r,u,43.2f),{.4f,.2f,-2});++optical;
    }
    float peak=0;
    for(float hz:{2.f,4.f,10.f})for(float damping:{.2f,.8f,2.f})for(float fps:{45.f,72.f,90.f}) {
        BodySecondary::Spring spring;
        for(int i=0;i<900;++i){float t=i/fps;const auto response=spring.Step({sin(t*13)*6.f,cos(t*9)*5.f,70},1+t,43.2f,hz,damping,1);assert(std::isfinite(response)&&std::abs(response)<=.025001f);peak=std::max(peak,std::abs(response));
            auto bone=PortalPose::Frame({10,20,30},{10,20,30});const auto before=PortalPose::Position(bone);BodySecondary::Spring::Volume(bone,response);Near(PortalPose::Position(bone),before);}
        assert(spring.Step({10000,3000,8000},100,43.2f,hz,damping,1)==0);
    }
    assert(peak>.001f);
    WorldPush::Sweep sweep;Vector start,velocity;assert(!sweep.Sample({10,0,0},1,43.2f,start,velocity));
    assert(sweep.Sample({12,0,0},1.033,43.2f,start,velocity));const auto impulse=WorldPush::Impulse(velocity,{-1,0,0},43.2f,3,10,true);assert(impulse.x>0&&impulse.LengthSqr()<=powf(43.2f*10+.01f,2));
    Near(WorldPush::Impulse(velocity,{1,0,0},43.2f,3,10,true),{0,0,0});assert(!sweep.Sample({10000,0,0},1.066,43.2f,start,velocity));
    // Player travel, including a portal/funnel displacement, is subtracted
    // before sampling. A stationary relative controller must not push anything.
    WorldPush::Sweep travel;for(int i=0;i<600;++i){Vector eye{float(i*100),float(-i*200),float(i*50)},hand=eye+Vector(10,5,3);if(travel.Sample(hand-eye,1+i/30.,43.2f,start,velocity))Near(velocity,{0,0,0});}
    WorldPush::Cooldown cooldown;int entity=0;assert(cooldown.Accept(&entity,1)&&!cooldown.Accept(&entity,1.1)&&cooldown.Accept(&entity,1.21));
    auto module=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(module&&NativePhysics::ContactProfile());FreeLibrary(module);
    std::ifstream input(argv[2],std::ios::binary);std::vector<unsigned char> mdl{std::istreambuf_iterator<char>(input),{}};BodyIK::Rig rig;assert(rig.Read(mdl.data(),mdl.size()));
    matrix3x4_t posed[BodyIK::Max];int poses=0;
    for(int hand=0;hand<2;++hand)for(float extension:{0.f,.7f,1.f})for(float curl:{0.f,.5f,1.f}) {
        std::memcpy(posed,rig.bind,sizeof(posed));float curls[5]={curl,.5f,.5f,.5f,.5f};const auto wrist=PortalPose::Position(posed[rig.arm[hand][2]]);
        rig.Curl(posed,hand,curls,true,1.5f,.65f,extension);Near(wrist,PortalPose::Position(posed[rig.arm[hand][2]]));
        for(int finger=0;finger<5;++finger)for(int segment=0;segment<3;++segment){const int b=rig.fingers[hand][finger][segment],parent=segment?rig.fingers[hand][finger][segment-1]:rig.arm[hand][2];
            const float before=sqrtf((PortalPose::Position(rig.bind[b])-PortalPose::Position(rig.bind[parent])).LengthSqr()),after=sqrtf((PortalPose::Position(posed[b])-PortalPose::Position(posed[parent])).LengthSqr());assert(std::abs(before-after)<.001f);}
        ++poses;
    }
    // A fast controller can cross a wall plane without invalidating contact.
    int wallCases=0;
    for(float yaw:{-175.f,-90.f,0.f,90.f,175.f})for(float depth:{.1f,1.f,10.f,35.f,80.f}) {
        Vector n;QAngle::AngleVectors({0,yaw,0},&n,nullptr,nullptr);Vector surface{30,20,10},tip=surface-n*depth;
        const auto correction=GunContact::Correction(tip,surface,n,.2f,100);
        assert(TwoHandAim::Dot(tip+correction-surface,n)>=.19f);++wallCases;
    }
    Near(GunContact::Correction({0,0,0},{1,2,3},{0,0,0},.1f,100),{0,0,0});
    assert(GunContact::Correction({1000,0,0},{0,0,0},{-1,0,0},.2f,40).LengthSqr()<=1600.01f);
    // Native pickup remains single-owner; both controller edges acquire and
    // only the actual owner release can drop/throw. Pause is never a throw.
    for(int owner=0;owner<2;++owner) {
        PropHold::Input input;bool grip[2]{},tracked[2]={true,true},eligible[2]={true,true};
        grip[owner]=true;input.Step(true,grip,tracked,eligible);assert(input.hand==owner&&input.use);input.Attach();
        grip[1-owner]=true;input.Step(true,grip,tracked,eligible);assert(input.hand==owner&&!input.release);
        grip[owner]=false;input.Step(true,grip,tracked,eligible);assert(input.release&&input.throwRelease);
        input.Suspend();assert(!input.throwRelease);input.Detached();
        PropInteraction::Carry palm;auto hand=PortalPose::Frame({100,200,300},{20,owner?90.f:-90.f,35});
        auto object=PortalPose::Frame({80,250,320},{10,15,20});
        const auto target=palm.Target((void*)1,(void*)2,1,owner,1,hand,object,43.2f,3.024f);
        assert(std::abs(sqrtf((PortalPose::Position(target)-PortalPose::Position(hand)).LengthSqr())-3.024f)<.001f);
    }
    // Skin changes are visible but cannot shift any skeleton joint or controller.
    int skinChanges=0;
    for(float chest:{.75f,1.f,1.5f,2.f})for(float hip:{.75f,1.f,1.5f,2.f}) {
        std::memcpy(posed,rig.bind,sizeof(posed));rig.TorsoVolume(posed,chest,hip);
        for(int i=0;i<rig.count;++i)Near(PortalPose::Position(posed[i]),PortalPose::Position(rig.bind[i]));
        if(chest!=1){float change=0;for(int c=0;c<3;++c)change+=(TwoHandAim::Axis(posed[rig.chest],c)-TwoHandAim::Axis(rig.bind[rig.chest],c)).LengthSqr();assert(change>.001f);}
        ++skinChanges;
    }
    const Vector punch{43.2f*3,0,0},normal{-1,0,0};
    const auto old=WorldPush::Impulse(punch,normal,43.2f,1,20,false);
    const auto stronger=WorldPush::Impulse(punch,normal,43.2f,6,160,false);
    assert(stronger.LengthSqr()>old.LengthSqr()*30);
    const auto cap=WorldPush::Impulse(punch*3,normal,43.2f,90,160,true);assert(cap.LengthSqr()<=powf(43.2f*160+.01f,2));
    // The portal copy must retain the same non-rigid skin basis, not rebuild
    // an unscaled native chest/hips. Check real weighted Chell vertices too.
    std::memcpy(posed,rig.bind,sizeof(posed));rig.TorsoVolume(posed,1.5f,1.4f);
    AvatarPalette::Snapshot palette;palette.Store(1,mdl.data(),rig.bind[0],posed,rig.count);
    matrix3x4_t portal[BodyIK::Max];const auto delta=PortalPose::Frame({100,200,300},{30,90,40});
    palette.Copy(portal,HandPose::Concat(delta,rig.bind[0]),false);
    for(int i=0;i<rig.count;++i)for(int c=0;c<3;++c)Near(TwoHandAim::Axis(portal[i],c),TwoHandAim::Axis(HandPose::Concat(delta,posed[i]),c));
    const auto vvdPath=std::string(argv[2]).substr(0,std::string(argv[2]).size()-4)+".vvd";
    std::ifstream vertexFile(vvdPath,std::ios::binary);assert(vertexFile);std::vector<unsigned char> vertices{std::istreambuf_iterator<char>(vertexFile),{}};
    int vertexCount=0,vertexStart=0;memcpy(&vertexCount,vertices.data()+16,4);memcpy(&vertexStart,vertices.data()+56,4);
    assert(vertexStart>=64&&vertexCount>20000&&size_t(vertexStart)+size_t(vertexCount)*48<=vertices.size());
    int changedVertices=0;float skinPeak=0;
    auto point=[](const matrix3x4_t& m,Vector p){Vector out;for(int r=0;r<3;++r)out[r]=m[r][0]*p.x+m[r][1]*p.y+m[r][2]*p.z+m[r][3];return out;};
    for(int i=0;i<vertexCount;++i){const auto* record=vertices.data()+vertexStart+i*48;Vector position;memcpy(&position,record+16,12);Vector after{0,0,0};
        for(int j=0;j<record[15];++j){float weight;memcpy(&weight,record+j*4,4);const auto b=record[12+j];assert(b<rig.count);after+=point(HandPose::Concat(posed[b],HandPose::InverseRigid(rig.bind[b])),position)*weight;}
        const float difference=sqrtf((after-position).LengthSqr());if(difference>.05f)++changedVertices;skinPeak=std::max(skinPeak,difference);
    }
    assert(changedVertices>500&&skinPeak>.5f);
    printf("Shared portal skin scale verified: %d real Chell vertices change; peak %.3f units; all joints fixed.\n",changedVertices,skinPeak);
    printf("Embodiment-23: %d rotated/deep wall contacts; either-hand native ownership/release and palm snap; %d fixed-joint torso scale cases; stronger bounded impacts; all menus fit. CPU only.\n",wallCases,skinChanges);
    printf("Polish-22: menu/config bounds and previews; physical/legacy trigger routes; 45 reticle rasters/%d optical poses; 24300 damped frames; 600 travel-isolation samples; native physics profile; %d actual Chell thumb/grip poses preserve joints. CPU only.\n",optical,poses);
}
