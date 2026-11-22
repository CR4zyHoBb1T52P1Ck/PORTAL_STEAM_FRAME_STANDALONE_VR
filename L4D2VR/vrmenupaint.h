#pragma once
#include "graphicsmenupaint.h"
#include "vrmenuoptions.h"
namespace VrMenu {
enum Action {None,OpenPage,Edit,ResetPage,Back,Recenter,Resume,Benchmark,ResetPlacement,Restart,Preset,TestHaptic,ResetGunGrip,SetGrabPoint,ResetGrabPoint,StartTPose,ApplyTPose,CancelTPose,ApplyManualFit,ResetBodyFit,ResetHandFit,OriginalBodyShape,SetHolster,SetStandingHeight,CreatorCommand,SaveImageBaseline,RestoreImageBaseline,SceneBookmark};
struct Row {std::string label,value;Action action=None;int target=-1;};
inline std::vector<Row> Rows(const Settings& s,Page page) {
    std::vector<Row> rows;
    if(page==Home) {
        for(const auto& item:std::vector<std::pair<const char*,Page>>{{"Calibration, tuning and comfort",Calibration},{"Arms and body",Arms},{"World interactions (beta)",Interactions},{"Gun placement and holster",Gun},{"Movement, turning and comfort",LocomotionFit},{"Crosshair and trigger colors",Crosshair},{"Body shape and jiggle",BodyDynamics},{"Visuals",Visuals},{"Upscaling",Upscaling},{"Menu placement and feedback",Menu},{"Creator, cameras and replay",Creator}})
            rows.push_back({item.first,"Open  >",OpenPage,item.second});
        rows.push_back({"Resume game","Close all menus",Resume});
    } else {
        if(page==Creator){rows.push_back({"Screen effects","Open  >",OpenPage,Effects});rows.push_back({"Spectator / free camera","Open  >",OpenPage,CreatorCamera});rows.push_back({"Native demo recording / theatre","Open  >",OpenPage,Replay});}
        if(page==CreatorCamera){rows.push_back({"Move / turn free camera","Open  >",OpenPage,CreatorMove});rows.push_back({"Reset camera at headset","Reset",CreatorCommand,20});}
        if(page==CreatorMove){
            const char* labels[]={"Forward","Backward","Left","Right","Up","Down","Turn left","Turn right","Look up","Look down"};
            for(int i=0;i<10;++i)rows.push_back({labels[i],"Step",CreatorCommand,30+i});
        }
        if(page==Replay){
            for(auto item:std::vector<std::pair<const char*,int>>{{"Start a new demo recording",0},{"Stop recording (game keeps running)",1},{"Play latest take (leaves current level)",2},{"Pause replay",3},{"Resume replay",4},{"Apply playback speed",5},{"Open native demo timeline",6},{"Exit demo playback",7}})rows.push_back({item.first,"Select",CreatorCommand,item.second});
        }
        if(page==Calibration) {
            for(const auto& item:std::vector<std::pair<const char*,Page>>{{"Standing T-pose setup",TPose},{"Body measurements / manual setup",Measurements},{"Hand alignment",HandFit},{"Chest, hips, shape and jiggle",BodyDynamics},{"Body and wrist tuning",BodyTuning},{"Portal gun grab point",GrabPoint},{"Shoulder holster zones",HolsterZones},{"Movement, turning and comfort",LocomotionFit}})rows.push_back({item.first,"Open  >",OpenPage,item.second});
            rows.push_back({"Arm length calibration","Open  >",OpenPage,ArmLengths});rows.push_back({"Recenter headset and panels","Recenter",Recenter});rows.push_back({"Back",Title(Home),Back});return rows;
        }
        if(page==TPose) {
            rows.push_back({"Start standing T-pose calibration","Arm capture",StartTPose});
            rows.push_back({"Apply captured body fit","Save measured reach",ApplyTPose});
            rows.push_back({"Manual measurements instead","Open  >",OpenPage,Measurements});
            rows.push_back({"Cancel capture / discard result","Keep saved fit",CancelTPose});
            rows.push_back({"Reset body calibration","Use native body",ResetBodyFit});
        }
        if(page==Interactions)for(auto p:{WorldHands,PropGrab,Throwing,PhysicalContact})rows.push_back({Title(p),"Open  >",OpenPage,p});
        if(page==Fingers)rows.push_back({"Gun surface and visibility","Open  >",OpenPage,GunSurface});
        if(page==PhysicalContact)rows.push_back({"Extra sandbox force ranges","Open  >",OpenPage,Sandbox});
        if(page==PropGrab)rows.push_back({"Gun stowed / handoff routing","Open  >",OpenPage,PropRouting});
        if(page==Arms)rows.push_back({"Finger, thumb and gun poses","Open  >",OpenPage,Fingers});
        if(page==LocomotionFit)for(auto p:{Movement,Turning,Comfort,PortalTravel})rows.push_back({Title(p),"Open  >",OpenPage,p});
        for(int i:On(page)) {const auto& o=Options()[i];rows.push_back({o.name,Value(s,o),Edit,i});}
        if(page==BodyDynamics){rows.push_back({"Jiggle movement and touch tuning","Open  >",OpenPage,Jiggle});rows.push_back({"Arms, wrists and torso shape","Open  >",OpenPage,BodyShape});}
        if(page==BodyShape||page==Body)rows.push_back({"Chest, hips and jiggle","Open  >",OpenPage,BodyDynamics});
        if(page==Arms)rows.push_back({"Native body and IK","Open  >",OpenPage,Body});
        if(page==Arms)rows.push_back({"Hand alignment and wrist placement","Open  >",OpenPage,HandFit});
        if(page==HandFit){rows.push_back({"Left wrist placement","Open  >",OpenPage,LeftArm});rows.push_back({"Right wrist placement","Open  >",OpenPage,RightArm});rows.push_back({"Align hands to controller grips","Reset wrist offsets",ResetHandFit});}
        if(page==BodyShape)rows.push_back({"Original Chell proportions","All shape scales 100%",OriginalBodyShape});
        if(page==Measurements)rows.push_back({"Set standing height now","Stand upright",SetStandingHeight});
        if(page==Holster)rows.push_back({"Holstered gun model","Open  >",OpenPage,BackGun});
        if(page==Measurements)rows.push_back({"Apply manual measurements","Use fitted body",ApplyManualFit});
        if(page==HolsterZones){rows.push_back({"Fit left shoulder zone","Open  >",OpenPage,LeftHolster});rows.push_back({"Fit right shoulder zone","Open  >",OpenPage,RightHolster});}
        if(page==LeftHolster||page==RightHolster)rows.push_back({"Set zone from hand","Use matching physical hand",SetHolster,page==LeftHolster?0:1});
        if(page==Holster)rows.push_back({"Shoulder zone placement and size","Open  >",OpenPage,HolsterZones});
        if(page==Body)rows.push_back({"Body and wrist tuning","Open  >",OpenPage,BodyTuning});
        if(page==BodyTuning){rows.push_back({"Wrist and forearm roll","Open  >",OpenPage,ForearmRoll});rows.push_back({"Arm length calibration","Open  >",OpenPage,ArmLengths});}
        if(page==Jiggle)rows.push_back({"Localized skin qualification","Open  >",OpenPage,SkinQualification});
        if(page==ArmLengths)rows.push_back({"Hand / controller alignment","Open  >",OpenPage,HandFit});
        if(page==GrabPoint){rows.push_back({"Set portal gun grab point","Sample free hand",SetGrabPoint});rows.push_back({"Reset portal gun grab point","Use authored grip",ResetGrabPoint});}
        if(page==Holster)rows.push_back({"Reset gun alignment","Return to authored grip",ResetGunGrip});
        if(page==Feedback){rows.push_back({"Test left controller haptic","Pulse",TestHaptic,0});rows.push_back({"Test right controller haptic","Pulse",TestHaptic,1});}
        if(page==Gun){rows.push_back({"Portal gun grab point","Open  >",OpenPage,GrabPoint});rows.push_back({"Support grip and aiming","Open  >",OpenPage,Support});rows.push_back({"Shoulder holster","Open  >",OpenPage,Holster});rows.push_back({"Gun contact, lights and indicator","Open  >",OpenPage,GunSurface});}
        if(page==Support){rows.push_back({"Grip zone size and position","Open  >",OpenPage,GripZonePage});rows.push_back({"Supporting hand placement","Open  >",OpenPage,SupportHand});rows.push_back({"Weapon haptics","Open  >",OpenPage,Feedback});}
        if(page==Visuals){rows.push_back({"Portal-view performance","Open  >",OpenPage,PortalPerformance});rows.push_back({"Desktop mirror and recording","Open  >",OpenPage,Capture});}
        if(page==Creator)rows.push_back({"Desktop mirror and recording","Open  >",OpenPage,Capture});
        if(page==PortalPerformance){rows.push_back({"Native resolution, lighter MSAA","2016 / 2x / native",Preset,4});rows.push_back({"Balanced upscaling preset","1728 / 2x / 2016 edge",Preset,1});rows.push_back({"Compare image presets","Open  >",OpenPage,VisualComparison});}
        if(page==PortalPerformance||page==VisualComparison)rows.push_back({"Scene comparison bookmark","Arm / cancel",SceneBookmark});
        if(page==PhysicalContact){rows.push_back({"Sustained pressure controls","Open  >",OpenPage,PressureTuning});rows.push_back({"Hand / gun force controls","Open  >",OpenPage,ImpactTuning});rows.push_back({"Gun wall clearance controls","Open  >",OpenPage,GunCollision});}
        if(page==PropGrab)rows.push_back({"Palm carry and object hand pose","Open  >",OpenPage,PropPose});
        if(page==Upscaling){
            rows.push_back({"Native clarity preset","2016 / 4x / native",Preset,0});
            rows.push_back({"Balanced upscale preset","1728 / 2x / 2016 edge",Preset,1});
            rows.push_back({"Performance upscale preset","1536 / 2x / 2016 fast",Preset,2});
            rows.push_back({"High output preset","2016 / 2x / 2560 edge",Preset,3});
            rows.push_back({"Portal-heavy native preset","2016 / 2x / native",Preset,4});
        }
        if(page==Upscaling)rows.push_back({"Compare / restore image presets","Open  >",OpenPage,VisualComparison});
        if(page==VisualComparison){
            rows.push_back({"Save / replace image baseline","Keep current image settings",SaveImageBaseline});
            rows.push_back({"Restore saved image baseline","Resolution / MSAA / upscale",RestoreImageBaseline});
            rows.push_back({"Native reference","2016 / 2x / native",Preset,4});
            rows.push_back({"Balanced spatial upscale","1728 / 2x / 2016 edge",Preset,1});
            rows.push_back({"Performance spatial upscale","1536 / 2x / 2016 fast",Preset,2});
            rows.push_back({"Measure 30s; game stays running","Arm sample",Benchmark});
            rows.push_back({"GPU and portal measurements","Open  >",OpenPage,PortalPerformance});
        }
        if(page==Visuals||page==Upscaling||page==Effects||page==PortalPerformance||page==VisualComparison)rows.push_back({"Restart Portal with new settings","Save settings + relaunch",Restart});
        if(page==Comfort)rows.push_back({"Recenter headset","Recenter",Recenter});
        if(page==Menu)rows.push_back({"Reset menu placement","Bring panel in front",ResetPlacement});
        if(!On(page).empty())rows.push_back({"Reset this page","Restore defaults",ResetPage,int(page)});
        rows.push_back({"Back",Title(Parent(page)),Back});
    }
    return rows;
}
constexpr int Width=1200,Height=1060,Top=190,Step=58;
// Compatibility path for runtimes with a smaller raw-overlay transfer limit.
// Logical hit coordinates remain 1200 x 1060 at either texture size.
inline std::vector<uint32_t> HalfRaster(const std::vector<uint32_t>& source) {
    if(source.size()!=Width*Height)return {};
    std::vector<uint32_t> out(Width/2*Height/2);
    for(int y=0;y<Height/2;++y)for(int x=0;x<Width/2;++x) {
        const size_t i=y*2*Width+x*2;uint32_t pixel=0;
        for(int channel=0;channel<32;channel+=8) {
            unsigned sum=0;for(size_t offset:{size_t(0),size_t(1),size_t(Width),size_t(Width+1)})sum+=(source[i+offset]>>channel)&255;
            pixel|=((sum+2)/4)<<channel;
        }
        out[y*(Width/2)+x]=pixel;
    }
    return out;
}
inline int Hit(float x,float y,int count) {
    if(x<30||x>=Width-30||y<Top||y>=Top+count*Step)return -1;
    const int local=int(y)-Top;return local%Step<52 ? local/Step : -1;
}
inline std::vector<uint32_t> Paint(const Settings& s,Page page,int selected,const std::string& active,const std::string& status,const std::string& pipeline="") {
    BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),Width,-Height,1,32,BI_RGB};
    HDC dc=CreateCompatibleDC(nullptr);void* pixels=nullptr;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!dc||!bitmap||!pixels){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return {};}
    auto old=SelectObject(dc,bitmap);SetBkMode(dc,TRANSPARENT);
    auto fill=[&](RECT r,COLORREF c,int radius=0){HBRUSH b=CreateSolidBrush(c);auto previous=SelectObject(dc,b);auto pen=SelectObject(dc,GetStockObject(NULL_PEN));
        if(radius)RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);else FillRect(dc,&r,b);
        SelectObject(dc,pen);SelectObject(dc,previous);DeleteObject(b);};
    fill({0,0,Width,Height},RGB(16,22,30),28);
    HFONT font=CreateFontA(-27,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");auto oldFont=SelectObject(dc,font);
    auto text=[&](int x,int y,const std::string& value,COLORREF color=RGB(235,240,246)){SetTextColor(dc,color);TextOutA(dc,x,y,value.c_str(),int(value.size()));};
    text(60,25,std::string("PORTAL FRAME  /  ")+Title(page)+"  [Contact-51]",RGB(82,200,255));text(1130,25,"X",RGB(255,200,120));
    text(40,75,active);text(40,118,pipeline.empty()?"Grip + drag: move   |   Grip + stick: depth   |   Grip + D-pad: slide":pipeline);
    text(40,155,"Both stick clicks: bring settings here. VIEW: escape. Corners: resize.",RGB(166,189,210));
    const auto rows=Rows(s,page);
    for(int i=0;i<int(rows.size());++i){int y=Top+i*Step;
        fill({30,y,Width-30,y+52},i==selected&&s.menuHighlight?RGB(40,94,125):RGB(30,40,52),10);
        if(i==selected&&s.menuHighlight)fill({30,y,36,y+52},RGB(80,210,255),4);
        text(48,y+9,rows[i].label);
        if(rows[i].action==Edit){text(548,y+9,"-",RGB(95,210,255));text(1120,y+9,"+",RGB(95,210,255));}
        text(610,y+9,rows[i].value);
    }
    for(auto r:{RECT{4,4,48,12},RECT{4,4,12,48},RECT{Width-48,4,Width-4,12},RECT{Width-12,4,Width-4,48},
        RECT{4,Height-12,48,Height-4},RECT{4,Height-48,12,Height-4},RECT{Width-48,Height-12,Width-4,Height-4},RECT{Width-12,Height-48,Width-4,Height-4}})fill(r,RGB(75,195,255),4);
    text(40,920,"Point + trigger to select. D-pad navigates; left/right adjusts.");
    text(40,962,status,RGB(155,225,180));
    text(40,1006,"Restart applies image size, MSAA, textures and effects. Progress: last save.",RGB(255,204,120));
    GdiFlush();auto* src=static_cast<uint32_t*>(pixels);std::vector<uint32_t> output(src,src+Width*Height);
    for(auto& p:output)p=0xff000000u|((p&255)<<16)|(p&0xff00)|((p>>16)&255);
    SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return output;
}
}
