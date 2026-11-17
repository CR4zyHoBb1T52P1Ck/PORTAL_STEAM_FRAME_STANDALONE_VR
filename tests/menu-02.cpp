#include <Windows.h>
#include <cassert>
#include <fstream>
#include <map>
#include <set>
#include "vrmenupaint.h"
#include "menupose.h"
#include "nativefit.h"
#include "portalpose.h"
#include "menuraster.h"
#include "renderpolicy.h"
#include "framestats.h"
#include "vrfocus.h"
#include "vrexternal.h"
static void Near(float a,float b,float tolerance=.0002f){assert(std::abs(a-b)<tolerance);}
static void Near(const Vector& a,const Vector& b){assert((a-b).LengthSqr()<.000001f);}
static std::map<std::string,std::string> ConfigMap(const std::string& text) {
    std::map<std::string,std::string> result;std::istringstream lines(text);std::string line;
    while(std::getline(lines,line)){auto equal=line.find('=');if(equal!=std::string::npos)result[line.substr(0,equal)]=line.substr(equal+1);}
    return result;
}
static void ConfigLifecycle(const char* path) {
    bool valid=false;
    assert(VrSettings::ConfigValue<uint32_t>(" false # previously saved",9,&valid)==0&&valid);
    assert(VrSettings::ConfigValue<uint32_t>("TRUE",9,&valid)==1&&valid);
    for(auto text:{"", "garbage", "truejunk", "nan", "inf", "1e9999", "3 trailing"}) {
        assert(VrSettings::ConfigValue<float>(text,17,&valid)==17&&!valid);
    }
    for(auto text:{"-1","4294967296","1.5"})assert(VrSettings::ConfigValue<uint32_t>(text,7,&valid)==7&&!valid);
    assert(VrSettings::ConfigValue<bool>("2",true,&valid)&&!valid);
    assert(!VrSettings::ConfigValue<bool>("0",true,&valid)&&valid);
    assert(VrSettings::ConfigValue<float>(" -2.5 # offset",0,&valid)==-2.5f&&valid);
    std::ifstream input(path,std::ios::binary);assert(input);const std::string failure((std::istreambuf_iterator<char>(input)),{});
    auto parsed=ConfigMap(failure);assert(parsed.count("RenderWindow"));
    bool oldRejected=false;try{std::stol(parsed.at("RenderWindow"));}catch(const std::invalid_argument&){oldRejected=true;}assert(oldRejected);
    assert(VrSettings::ConfigValue<uint32_t>(parsed.at("RenderWindow"),9,&valid)==0&&valid);
    VrGraphics::Settings current;VrMenu::Load(current,parsed);
    unsigned reloads=0;
    for(bool mirror:{false,true,false}) {
        current.mirror=mirror;
        auto saved=VrMenu::Save(current,failure);auto values=ConfigMap(saved);
        assert(std::stol(values.at("RenderWindow"))==int(mirror));
        assert(VrSettings::ConfigValue<uint32_t>(values.at("RenderWindow"),9,&valid)==uint32_t(mirror)&&valid);
        for(const auto& option:VrMenu::Options()) {
            std::visit([&](auto member){using T=std::decay_t<decltype(current.*member)>;
                const auto value=VrSettings::ConfigValue<T>(values.at(option.key),T{},&valid);
                assert(valid);Near(float(value),float(current.*member),.011f);},option.member);
            ++reloads;
        }
        VrGraphics::Settings next;VrMenu::Load(next,values);
        for(const auto& option:VrMenu::Options())Near(VrMenu::Get(next,option),VrMenu::Get(current,option),.011f);
        // A grab release saves every setting even when no visual option changed.
        saved=VrMenu::Save(next,saved);assert(std::stol(ConfigMap(saved).at("RenderWindow"))==int(mirror));
    }
    printf("PASS: exact saved RenderWindow=false failure reproduced; %u typed save/reload checks; malformed settings recover\n",reloads);
}
static void Bitmap(const char* base,int page,std::vector<uint32_t> pixels) {
    for(auto& p:pixels)p=(p&0xff00ff00)|((p&255)<<16)|((p>>16)&255);
    BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+unsigned(pixels.size()*4);
    BITMAPINFOHEADER info{sizeof(BITMAPINFOHEADER),VrMenu::Width,-VrMenu::Height,1,32,BI_RGB};
    std::ofstream out(std::string(base)+"-"+std::to_string(page)+".bmp",std::ios::binary);
    out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&info),sizeof(info));out.write(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);
}
int main(int argc,char** argv) {
    assert(argc==3||argc==4);if(argc==4)ConfigLifecycle(argv[3]);VrGraphics::Settings settings;
    std::ofstream defaults(std::string(argv[2])+"-defaults.cfg");defaults<<VrMenu::Save(settings,"");defaults.close();
    std::set<std::string> keys;std::map<std::string,std::string> input;
    unsigned optionCases=0,rayCases=0;
    for(const auto& o:VrMenu::Options()) {
        assert(keys.insert(o.key).second);auto edited=settings;
        for(int i=0;i<1000;++i)VrMenu::Change(edited,o,1);
        assert(VrMenu::Get(edited,o)<=o.high);
        for(int i=0;i<2000;++i)VrMenu::Change(edited,o,-1);
        assert(VrMenu::Get(edited,o)>=o.low);
        const auto before=VrMenu::Get(edited,o);VrMenu::Set(edited,o,NAN);Near(before,VrMenu::Get(edited,o));
        input[o.key]=VrMenu::Number(VrMenu::Get(settings,o));++optionCases;
    }
    auto restored=settings;restored.gunX=29;VrMenu::Load(restored,input);
    for(const auto& o:VrMenu::Options())Near(VrMenu::Get(settings,o),VrMenu::Get(restored,o),.011f);
    const auto saved=VrMenu::Save(settings,"Unrelated=value # keep\r\nGunForwardCM=12\r\nGunForwardCM=-3\r\n");
    assert(saved.find("Unrelated=value # keep\r\n")!=std::string::npos);
    assert(saved.find("GunForwardCM=12")==std::string::npos && saved.find("GunForwardCM=-3")==std::string::npos);
    for(int page=0;page<int(VrMenu::PageCount);++page) {
        auto rows=VrMenu::Rows(settings,VrMenu::Page(page));assert(!rows.empty()&&rows.size()<=12);
        for(int row=0;row<int(rows.size());++row){assert(VrMenu::Hit(50,VrMenu::Top+row*VrMenu::Step+10,int(rows.size()))==row);assert(VrMenu::Hit(50,VrMenu::Top+row*VrMenu::Step+55,int(rows.size()))==-1);}
        Bitmap(argv[2],page,VrMenu::Paint(settings,VrMenu::Page(page),0,"Scene 2016 x 2064 | View 1732 x 1739 | MSAA 2x","Saved and applied."));
    }
    for(float yaw:{-170.f,-80.f,0.f,80.f,170.f})for(float depth:{.4f,1.2f,4.f}) {
        const auto head=PortalPose::Frame({.3f,1.7f,-.2f},{0,yaw,0});
        auto panel=MenuPose::Front(head,depth);const auto center=MenuPose::Position(panel);
        for(float x:{-.49f,-.2f,0.f,.2f,.49f})for(float y:{-.49f,0.f,.49f}) {
            const auto point=center+MenuPose::Axis(panel,0)*(x*1.3f)+MenuPose::Axis(panel,1)*(y*1.3f*1060/1200);
            const auto source=MenuPose::Position(head);const auto delta=point-source;const auto direction=delta*(1/std::sqrt(delta.LengthSqr()));
            const auto hit=MenuPose::Intersect(panel,1.3f,1060.f/1200,source,direction,1200,1060);
            assert(hit.hit);Near(hit.x,(x+.5f)*1200,.003f);Near(hit.y,(.5f-y)*1060,.003f);Near(hit.point,point);++rayCases;
            assert(!MenuPose::Intersect(panel,1.3f,1060.f/1200,source,-direction,1200,1060).hit);
        }
        auto previous=PortalPose::Frame({.1f,1.3f,-.5f},{10,15,-6});
        const auto grabLocal=HandPose::Concat(HandPose::InverseRigid(previous),panel);
        for(int i=0;i<1000;++i){auto hand=PortalPose::Frame({.1f+i*.0002f,1.3f,-.5f},{10+i*.01f,15+i*.02f,-6});panel=MenuPose::Drag(panel,previous,hand);previous=hand;}
        const auto actual=HandPose::Concat(HandPose::InverseRigid(previous),panel);
        for(int r=0;r<3;++r)for(int c=0;c<4;++c)Near(actual[r][c],grabLocal[r][c],.002f);
        MenuPose::Depth(panel,MenuPose::Position(head),-100);Near(std::sqrt((MenuPose::Position(panel)-MenuPose::Position(head)).LengthSqr()),.4f);
        MenuPose::Depth(panel,MenuPose::Position(head),100);Near(std::sqrt((MenuPose::Position(panel)-MenuPose::Position(head)).LengthSqr()),4);
    }
    assert(MenuPose::Corner(20,20,1200,1060)==0 && MenuPose::Corner(1180,1040,1200,1060)==3);
    unsigned companionCases=0;
    for(float yaw:{-170.f,-80.f,0.f,80.f,170.f})for(float depth:{.4f,1.2f,4.f})for(float width:{.5f,1.3f,2.5f})for(bool focus:{false,true}) {
        const auto head=PortalPose::Frame({.3f,1.7f,-.2f},{0,yaw,0});
        const auto source=MenuPose::Position(head);
        matrix3x4_t panels[2]={MenuPose::Beside(head,depth,width,0,focus),MenuPose::Beside(head,depth,width,1,focus)};
        for(int target=0;target<2;++target)for(float x:{-.49f,0.f,.49f})for(float y:{-.49f,0.f,.49f}) {
            const auto point=MenuPose::Position(panels[target])+MenuPose::Axis(panels[target],0)*(x*width)+MenuPose::Axis(panels[target],1)*(y*width*1060/1200);
            Vector direction=point-source;VectorNormalize(direction);
            MenuPose::Hit hits[2];for(int p=0;p<2;++p)hits[p]=MenuPose::Intersect(panels[p],width,1060.f/1200,source,direction,1200,1060);
            assert(hits[target].hit);assert(MenuPose::Nearest(hits[0],hits[1])==target);++companionCases;
        }
        // Moving settings does not mutate the native panel's transform.
        const auto original=panels[0];const auto hand=HandPose::Frame({1,0,0},{0,1,0},{0,0,1},{0,0,0});
        auto moved=hand;MenuPose::Translate(moved,{.2f,.1f,-.3f});panels[1]=MenuPose::Drag(panels[1],hand,moved);
        assert(!memcmp(&original,&panels[0],sizeof(original)));
        MenuPose::Depth(panels[1],source,.5f);assert(!memcmp(&original,&panels[0],sizeof(original)));
        if(focus) {
            Vector direction=MenuPose::Position(MenuPose::Front(head,1))-source;direction.y=0;VectorNormalize(direction);
            const auto centered=MenuPose::Beside(head,depth,width,1,true);
            assert(MenuPose::Intersect(centered,width,1060.f/1200,source,direction,1200,1060).hit);
        }
    }
    MenuPose::Hit miss,nearHit,farHit;nearHit.hit=farHit.hit=true;nearHit.distance=1;farHit.distance=2;
    assert(MenuPose::Nearest(miss,miss)==-1&&MenuPose::Nearest(nearHit,farHit)==0&&MenuPose::Nearest(farHit,nearHit)==1&&MenuPose::Nearest(nearHit,nearHit)==1);
    MenuPose::StickChord chord;
    assert(!chord.Update(false,false)&&!chord.Update(true,false)&&chord.Update(true,true));
    for(int i=0;i<1000;++i)assert(!chord.Update(true,true));
    assert(!chord.Update(false,true)&&!chord.Update(true,true)); // no bounce reopen
    assert(!chord.Update(false,false)&&!chord.Update(false,true)&&chord.Update(true,true));
    std::vector<uint32_t> raster(VrMenu::Width*VrMenu::Height,0xff204060);raster[0]=0xff6080a0;
    const auto half=VrMenu::HalfRaster(raster);assert(half.size()==VrMenu::Width*VrMenu::Height/4);
    assert(half[0]==0xff305070&&half[1]==0xff204060&&half.back()==0xff204060);
    assert(VrMenu::HalfRaster({}).empty());
    MenuRaster::Swap swap;
    assert(!swap.ready&&!swap.AcceptsClicks(0));
    assert(swap.Begin(100,0)&&!swap.Begin(101,1)&&swap.front==0&&!swap.ready);
    assert(swap.Loaded()&&swap.front==1&&swap.AcceptsClicks(0));
    assert(!swap.Begin(120,0)&&swap.Begin(195,0));
    assert(swap.front==1&&swap.ready);swap.Failed();assert(swap.front==1&&swap.ready);
    assert(swap.Begin(200,1)&&!swap.AcceptsClicks(1)&&swap.AcceptsClicks(0));
    assert(swap.Loaded()&&swap.front==0&&swap.AcceptsClicks(1));
    for(int i=0;i<10000;++i) {
        const int old=swap.front;assert(swap.Begin(300+i*100,1));
        assert(swap.front==old&&swap.ready);assert(swap.Loaded()&&swap.front!=old&&swap.ready);
    }
    assert(RenderPolicy::UseEyeTarget(false,nullptr));
    for(const char* name:{"_rt_FullFrameFB","_rt_FullFrameFB1"})assert(RenderPolicy::UseEyeTarget(false,name)&&!RenderPolicy::UseEyeTarget(true,name));
    for(const char* name:{"_rt_ShadowDepthTexture","_rt_WaterReflection","_rt_Camera","_rt_SmallFB1"})assert(!RenderPolicy::UseEyeTarget(false,name));
    struct View {bool m_bDoBloomAndToneMapping=true;int other=42;} view;
    RenderPolicy::PrepareEye(view,false);assert(!view.m_bDoBloomAndToneMapping&&view.other==42);
    RenderPolicy::PrepareEye(view,true);assert(view.m_bDoBloomAndToneMapping&&view.other==42);
    assert(RenderPolicy::Visible(2016,.006427f,.865672f)==1732);
    for(bool legacy:{false,true}) {
        auto choices=settings;choices.legacyEffects=legacy;
        const auto commands=VrGraphics::Commands(choices);
        // Mode switching saves/reloads the map. Reapplying on map entry must
        // never request another mode switch (the observed reload loop).
        for(auto forbidden:{"mat_hdr_level","mat_setvideomode","mat_dxlevel","mat_picmip","mat_forceaniso","mat_vsync","mat_viewport","_restart","quit"})assert(commands.find(forbidden)==std::string::npos);
    }
    // All independent system-focus loss signals must yield overlays/input.
    for(bool dashboard:{false,true})for(bool available:{false,true})for(bool pause:{false,true})for(bool scene:{false,true}) {
        const bool expected=!dashboard&&available&&!pause&&scene;
        assert(VrFocus::OwnsInput(dashboard,available,pause,scene)==expected);
    }
    VrFocus::Gate focus;
    struct ExternalMock {
        std::string events;bool fail=false;
        int BeginTextureSubmission(){events+="prepare/drain/lock;";return fail?-1:0;}
        int EndTextureSubmission(){events+="unlock/restore;";return 0;}
        int LockVulkanQueue(){events+="queue-lock;";return fail?-1:0;}
        int UnlockVulkanQueue(){events+="queue-unlock;";return 0;}
    } external;
    {
        VrExternal::Guard<ExternalMock,true> guard(&external);assert(guard);
        external.events+="submit;";
    }
    assert(external.events=="prepare/drain/lock;submit;unlock/restore;");external.events.clear();
    try {VrExternal::Guard<ExternalMock,false> guard(&external);assert(guard);throw 42;}catch(int){}
    assert(external.events=="queue-lock;queue-unlock;");external.events.clear();external.fail=true;
    {VrExternal::Guard<ExternalMock,true> guard(&external);assert(!guard);}
    assert(external.events=="prepare/drain/lock;");
    {VrExternal::Guard<ExternalMock,false> guard(nullptr);assert(!guard);}
    for(int i=0;i<1000;++i) {
        assert(focus.Allowed());focus.Focus(false);assert(!focus.Allowed());
        focus.Sample(true,false);assert(!focus.Allowed());
        focus.Focus(true);focus.Sample(false,false);assert(!focus.Allowed());
        focus.Sample(true,true);assert(!focus.Allowed());
        focus.Sample(true,false);assert(focus.Allowed());
    }
    for(bool canRender:{false,true})for(bool eyes:{false,true})for(bool background:{false,true}) {
        auto kind=VrFocus::SelectSubmission(canRender,eyes,background);
        assert(kind==(!canRender?VrFocus::Submission::None:eyes?VrFocus::Submission::Eyes:background?VrFocus::Submission::Background:VrFocus::Submission::None));
    }
    for(const auto& option:VrMenu::Options())if(std::string(option.key)=="TextureQuality"||std::string(option.key)=="AnisotropicFiltering")assert(option.restart);
    printf("PASS: system focus matrix, 1000 held-input return cycles, scene submission policy, startup-only resource controls\n");
    FrameStats::Window timing;timing.Add(10,2,5);timing.Add(20,4,10);timing.Add(30,6,15);
    timing.Add(NAN,0,0);timing.Add(-1,0,0);
    assert(timing.frames.size()==3);Near(timing.Mean(),20);Near(timing.P95(),20);
    Near(float(timing.eye),12);Near(float(timing.wait),30);timing.Clear();assert(timing.frames.empty()&&timing.Mean()==0);
    const auto home=VrMenu::Rows(settings,VrMenu::Home);
    for(auto page:{VrMenu::Visuals,VrMenu::Upscaling,VrMenu::Effects}) {
        auto rows=VrMenu::Rows(settings,page);assert(std::count_if(rows.begin(),rows.end(),[](auto row){return row.action==VrMenu::Restart;})==1);
    }
    printf("PASS: 10000 hidden-buffer swaps preserve front; native HDR target selection; shadow/reflection preservation; restart menu entries\n");
    printf("PASS: %u companion-panel routing cases, independent panel moves, centered shortcut and held/bouncing stick chord\n",companionCases);
    Near(NativeFit::Axis(.1f,.2f),0);Near(NativeFit::Axis(1,.2f),1);Near(NativeFit::Axis(-1,.2f),-1);
    Near(NativeFit::FrameDelta(16000),50);Near(NativeFit::FrameDelta(16),16);Near(NativeFit::FrameDelta(-10),0);Near(NativeFit::FrameDelta(NAN),0);
    std::ifstream modelFile(argv[1],std::ios::binary);std::vector<unsigned char> model((std::istreambuf_iterator<char>(modelFile)),{});assert(model.size()>248);
    int count,offset;memcpy(&count,model.data()+156,4);memcpy(&offset,model.data()+160,4);assert(count==43);
    matrix3x4_t bind[43];for(int i=0;i<43;++i){matrix3x4_t inverse;memcpy(&inverse,model.data()+offset+i*216+96,sizeof(inverse));bind[i]=HandPose::InverseRigid(inverse);}
    for(bool left:{false,true}) {
        const int wrist=left?8:27,thumb=left?23:42;
        const auto oldFrame=HandPose::ControllerHandFrame({1,0,0},{0,-1,0},{0,0,1},{0,0,0},left);
        const auto newFrame=NativeFit::Arm({1,0,0},{0,-1,0},{0,0,1},{0,0,0},left,0,0,0);
        const auto oldThumb=HandPose::Reanchor(bind[thumb],bind[wrist],oldFrame),newThumb=HandPose::Reanchor(bind[thumb],bind[wrist],newFrame);
        printf("%s native thumb oldZ=%f newZ=%f\n",left?"Left":"Right",oldThumb[2][3],newThumb[2][3]);
        assert(oldThumb[2][3]<0 && newThumb[2][3]>0);
        matrix3x4_t posed[43];for(int i=0;i<43;++i)posed[i]=HandPose::Reanchor(bind[i],bind[wrist],newFrame);
        auto original=posed[wrist];settings.arms=1;NativeFit::Visibility(posed,43,false,left,settings);
        assert(!memcmp(&original,&posed[wrist],sizeof(original)));
    }
    Near(NativeFit::Offset({0,0,0},{1,0,0},{0,-1,0},{0,0,1},10,5,-2,43.2f),{4.32f,-2.16f,-.864f});
    printf("PASS: %u settings round trips/clamps, %u stereo-menu ray cases, 15000 drag steps, native thumb-up checks and 18 CPU menu pages\n",optionCases,rayCases);
}
