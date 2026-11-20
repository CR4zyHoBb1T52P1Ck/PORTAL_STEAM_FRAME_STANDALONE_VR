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
    for(auto page:{VrMenu::Home,VrMenu::Crosshair,VrMenu::Fingers,VrMenu::Jiggle,VrMenu::PhysicalContact}) {
        const auto pixels=VrMenu::Paint(settings,page,0,"POLISH-22 CPU MENU PREVIEW","Physical trigger colors are independent of gun ownership.","Unreleased test build; no game running.");
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
    printf("Polish-22: menu/config bounds and previews; physical/legacy trigger routes; 45 reticle rasters/%d optical poses; 24300 damped frames; 600 travel-isolation samples; native physics profile; %d actual Chell thumb/grip poses preserve joints. CPU only.\n",optical,poses);
}
