#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <cstdint>

namespace VrGraphics {
constexpr int Width=1200,Height=1060,Top=194,RowHeight=58,Rows=12;
struct Settings {
    int creatorCamera=0;float creatorFov=75,creatorDistance=2,creatorHeight=.5f,creatorOrbit=12,creatorStepCM=25,replaySpeed=1;
    bool creatorKeyboard=false;
    bool autoPalmStowed=true,propTransfer=true,propSurfacePose=true,gunSurfacePose=true,gunWorldDepth=true;
    bool gunLightOcclusion=true,portalGunLights=true;
    bool gunIndicatorLastPortal=true;float gunIndicatorWindowMM=22;
    float propTransferCM=22,gunFingerMarginMM=4,propSurfaceGapCM=1;
    float propSmoothMS=18,gunPalmGapMM=8;bool selfTouch=true;
    bool sandboxForces=false;float sandboxForceGain=1,sandboxThrowGain=1;
    bool swapPortalColors=false,gunGripPose=true,handPush=false,gunPush=false,gunWallContact=false,jiggle=false;
    int crosshair=0,crosshairColor=0,thumbPose=0;
    bool crosshairDepth=true,crosshairLastPortal=true;
    float crosshairMM=10,crosshairOpacity=90,crosshairDistance=3;
    float thumbGain=100,thumbStraighten=0,thumbExtension=0,gunGripCurl=65,supportGripCurl=55;
    float pushGain=1,pushMaxImpulse=20,pushRadiusCM=4,pushHz=30,gunContactRadiusCM=7;
    float handPushStiffness=3500,gunPushStiffness=4500,pressureMaxN=300;
    float portalHeadClearance=35;
    float handImpactGain=6,gunImpactGain=6,impactMaxNS=160,impactCooldownMS=100;
    float gunWallRadiusCM=4,gunWallLimitCM=90,gunWallRelease=100;
    bool portalContactBypass=true,palmSnap=true,propShapePose=true;
    float palmCarryCM=7,propPoseCurl=65,chestScale=100,hipScale=100;
    bool portalPoseCache=true;
    int portalRenderDepth=0;bool autoPortalGpuTiming=true;
    float jiggleJumpGain=100,jiggleSwingGain=20,jiggleTouchGain=100,jiggleMaxResponse=20;
    float jiggleStrength=30,jiggleHz=4,jiggleDamping=.8f,jiggleChest=100,jiggleHips=50;
    int scale=100,msaa=2,texture=0,filter=8,upscaler=0;
    bool blur=false,bloom=true,shadows=true,mirror=false,showFps=false;
    bool legacyEffects=false;
    bool frameTimingLog=true;
    bool detailedGpuTiming=false,mirrorReuseEye=false;
    int mirrorEye=0,mirrorFit=0;
    int eyeWidth=2016,upscaleWidth=2016,arms=1,moveDirection=0,aim=2;
    bool leftArm=true,rightArm=true,gun=true,body=true,hideUpper=true,fingerTracking=true;
    bool gripHand=true,twoHandAim=true;
    int twoHandPercent=100;
    float supportRadiusCM=14;
    bool gripHoldNear=true,showSupportHand=true,snapSupportHand=false,slideSupportGrip=false;
    bool firmSupportGrip=true;
    bool animatedSupportGrip=true;
    float supportCaptureCM=6,supportReleaseCM=30;
    bool grabHaptics=true,holsterHaptics=true,transferHaptics=true,holsterHover=true;
    float grabHapticStrength=.5f,hapticDurationMS=100,hapticFrequency=160;
    bool grabHover=true,bodyIK=false;
    float bodyArmScale=1.1f,bodyIKInfluence=1;
    bool bodyAutoReach=true,bodyPortalPose=true;

    int gripZoneScale=100;
    float gripZoneLengthCM=44,gripZoneXCM=0,gripZoneYCM=0,gripZoneZCM=-2;
    float supportHandXCM=0,supportHandYCM=0,supportHandZCM=-3;
    float supportHandPitch=0,supportHandYaw=0,supportHandRoll=0;
    bool holster=true,transfer=true,holsterHands=true;
    int shoulderSlot=0;
    float shoulderRadiusCM=20,shoulderWidthCM=22,shoulderBackCM=15,shoulderDownCM=15;
    bool movement=true,turning=true,snap=false,leftHanded=false,support=true;
    bool seated=false,roomscale=true,collision=true,autoCalibration=true,vignette=false;
    bool menuTop=true,menuHaptics=true,menuLaser=true,menuGrab=true,menuFollow=false,menuHighlight=true;
    float leftX=0,leftY=0,leftZ=0,leftPitch=0,leftYaw=0,leftRoll=0;
    float rightX=0,rightY=0,rightZ=0,rightPitch=0,rightYaw=0,rightRoll=0;
    float gunX=0,gunY=0,gunZ=0,gunPitch=0,gunYaw=0,gunRoll=0;
    float grabXCM=-2,grabYCM=-3,grabZCM=-3.5f,fingerRange=150,wristTwist=50;
    bool naturalForearms=true;float leftElbowRoll=35,rightElbowRoll=15,leftForearmRoll=90,rightForearmRoll=65;
    float leftUpperLength=100,rightUpperLength=100,leftForearmLength=100,rightForearmLength=100;
    bool boundedForearmRoll=true;int skinTrialMode=0;float skinTrialMM=2;
    bool portalGunPose=true;
    bool bodyFit=false,controllerHands=false,bodyVolume=true,holsterReachTest=false;
    bool bodyUpright=true,backGun=true;
    bool rigidTorso=true,handContact=false,contactPose=true,contactHaptics=true,enhancedThrow=false;
    int propGrabMode=0;
    float torsoHeight=100,bodyClearanceCM=3,contactRadiusCM=3,contactHz=45,contactOverride=65;
    float handGrabReachCM=40,throwGain=1,throwMaxMPS=12,throwSpin=100;
    int handAnchor=1;
    float wristCuff=100,palmDepth=100;
    float backGunX=0,backGunY=0,backGunZ=0,backGunPitch=0,backGunYaw=0,backGunRoll=0;
    float bodyEyeCM=165,bodySpanCM=150,bodyWidthCM=40,bodyDropCM=23,bodyNeckBackCM=4,bodyYaw=0;
    float wristWidth=112,forearmWidth=110,upperArmWidth=105,handSize=100,torsoWidth=100;
    float holsterLeftX=0,holsterLeftY=0,holsterLeftZ=0,holsterLeftSize=100;
    float holsterRightX=0,holsterRightY=0,holsterRightZ=0,holsterRightSize=100;
    float moveSpeed=1,moveDeadzone=.15f,turnDegrees=150,turnDeadzone=.2f,snapAngle=45;
    float height=0,seatBoost=35,bodyBack=8,vignetteStrength=.6f;
    float menuWidth=1.3f,menuDepth=1.2f,hapticStrength=.35f;
    float upscaleSharpness=1.5f;
};
struct Size { uint32_t width,height; };
inline Size ExplicitTarget(int width,float cropX,float cropY) {
    width=std::clamp((width+7)/8*8,768,4096);
    return {uint32_t(width),uint32_t(std::clamp(std::ceil(width*double(cropX)/cropY/8)*8,768.,4096.))};
}
inline Size Target(uint32_t width,uint32_t height,float cropX,float cropY,int percent) {
    const auto dimension=[](double value) { return uint32_t(std::clamp(std::ceil(value/8)*8,512.,4096.)); };
    const double scale=std::clamp(percent,60,150)/100.;
    return {dimension(width*scale/std::clamp(cropX,.25f,1.f)),
            dimension(height*scale/std::clamp(cropY,.25f,1.f))};
}
template<size_t N> inline int Step(int value,int direction,const int (&values)[N]) {
    auto i=std::find(std::begin(values),std::end(values),value);
    int index=i==std::end(values) ? 0 : int(i-std::begin(values));
    return values[std::clamp(index+(direction<0?-1:1),0,int(N)-1)];
}
inline void Change(Settings& s,int row,int direction) {
    static const int scales[]={60,70,85,100,115,130,150},samples[]={0,2,4,8},textures[]={2,1,0},filters[]={1,2,4,8,16};
    switch(row) {
    case 0:s.scale=Step(s.scale,direction,scales);break;
    case 1:s.msaa=Step(s.msaa,direction,samples);break;
    case 2:s.texture=Step(s.texture,direction,textures);break;
    case 3:s.filter=Step(s.filter,direction,filters);break;
    case 4:s.blur=!s.blur;break;case 5:s.bloom=!s.bloom;break;
    case 6:s.shadows=!s.shadows;break;case 7:s.mirror=!s.mirror;break;
    case 8:s.showFps=!s.showFps;break;
    case 9:s.upscaler=1-s.upscaler;break;
    }
}
inline std::string Commands(const Settings& s) {
    return "mat_motion_blur_enabled "+std::to_string(s.legacyEffects&&s.blur)
        +";mat_disable_bloom "+std::to_string(!s.bloom)
        +";r_shadows "+std::to_string(s.shadows)+";r_shadowrendertotexture "+std::to_string(s.shadows)
        +";cl_showfps "+std::to_string(s.showFps)
        +(s.portalRenderDepth>0?";r_portal_stencil_depth "+std::to_string(std::clamp(s.portalRenderDepth,1,3)):"");
}
inline int Hit(int x,int y) {
    if(x<30 || x>=Width-30 || y<Top || y>=Top+Rows*RowHeight) return -1;
    if((y-Top)%RowHeight>=52) return -1;
    return (y-Top)/RowHeight;
}
inline std::string Value(const Settings& s,int row) {
    switch(row) {
    case 0:return std::to_string(s.scale)+"%  (restart)";
    case 1:return s.msaa ? std::to_string(s.msaa)+"x MSAA  (restart)" : "Off  (restart)";
    case 2:return s.texture==0 ? "High" : s.texture==1 ? "Medium" : "Low";
    case 3:return std::to_string(s.filter)+"x";
    case 4:return s.blur ? "On" : "Off";case 5:return s.bloom ? "On" : "Off";
    case 6:return s.shadows ? "On" : "Off";case 7:return s.mirror ? "On (extra scene render)" : "Off (faster)";
    case 8:return s.showFps ? "On" : "Off";
    case 9:return s.upscaler ? "Snapdragon GSR 1" : "Off / compositor scaling";
    case 10:return "Start 30-second sample";default:return "Return to pause menu";
    }
}
}
