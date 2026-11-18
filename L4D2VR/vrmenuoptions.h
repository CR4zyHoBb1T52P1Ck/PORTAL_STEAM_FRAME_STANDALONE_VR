#pragma once
#include "graphicssettings.h"
#include "vrsettings.h"
#include <vector>
#include <variant>
#include <sstream>
#include <iomanip>
namespace VrMenu {
using Settings=VrGraphics::Settings;
enum Page {Home,Arms,LeftArm,RightArm,Gun,Movement,Turning,Comfort,Visuals,Upscaling,Menu,Effects,Support,GripZonePage,SupportHand,Holster,Feedback,Body,GrabPoint,BodyTuning,Calibration,TPose,Measurements,HandFit,BodyShape,HolsterZones,LeftHolster,RightHolster,LocomotionFit,BackGun,Interactions,WorldHands,PropGrab,Throwing,Crosshair,Fingers,Jiggle,PhysicalContact,ImpactTuning,GunCollision,PropPose,PageCount};
inline const char* Title(Page p) {
    static const char* titles[]={"VR SETTINGS","ARMS AND BODY","LEFT ARM POSITION","RIGHT ARM POSITION","PORTAL GUN","MOVEMENT","TURNING","COMFORT","VISUALS","UPSCALING","MENU AND FEEDBACK","SCREEN EFFECTS","SUPPORT GRIP","GRIP ZONE","SUPPORT HAND","SHOULDER HOLSTER","WEAPON HAPTICS","BODY IK","GUN GRAB POINT","BODY TUNING","CALIBRATION AND FIT","STANDING T-POSE","BODY MEASUREMENTS","HAND ALIGNMENT","BODY SHAPE","HOLSTER ZONES","LEFT HOLSTER","RIGHT HOLSTER","MOVEMENT AND COMFORT","HOLSTERED GUN","WORLD INTERACTIONS","HAND CONTACT","PROP GRAB","THROWING"};
    if(p==Crosshair)return "CROSSHAIR AND COLORS";if(p==Fingers)return "FINGERS AND GUN POSES";
    if(p==ImpactTuning)return "HAND AND GUN FORCE";if(p==GunCollision)return "GUN COLLISION AND CLEARANCE";if(p==PropPose)return "PALM CARRY AND POSE";
    if(p==Jiggle)return "COSMETIC JIGGLE";if(p==PhysicalContact)return "PHYSICAL CONTACT (BETA)";
    return titles[int(p)];
}
inline Page Parent(Page p) {
    if(p==GripZonePage||p==SupportHand||p==Feedback)return Support;
    if(p==Support||p==Holster)return Gun;
    if(p==Body)return Arms;
    if(p==GrabPoint)return Gun;
    if(p==BodyTuning)return Body;
    if(p==TPose||p==Measurements||p==HandFit||p==BodyShape||p==HolsterZones||p==LocomotionFit)return Calibration;
    if(p==LeftHolster||p==RightHolster)return HolsterZones;
    if(p==BackGun)return Holster;
    if(p==WorldHands||p==PropGrab||p==Throwing)return Interactions;
    if(p==ImpactTuning||p==GunCollision)return PhysicalContact;if(p==PropPose)return PropGrab;
    if(p==PhysicalContact)return Interactions;if(p==Fingers)return Arms;
    return Home;
}
using Member=std::variant<int Settings::*,float Settings::*,bool Settings::*>;
struct Option {
    const char* key;const char* name;Page page;Member member;
    float low=0,high=1,step=1;const char* unit="";
    std::vector<std::pair<float,const char*>> choices{};bool restart=false;
};
inline const std::vector<Option>& Options() {
    static const std::vector<Option> all={
        {"PortalTriggerColorsSwapped","Physical triggers: right / left",Crosshair,&Settings::swapPortalColors,0,1,1,"",{{0,"Blue / orange"},{1,"Orange / blue"}}},
        {"VRCrosshair","World-space crosshair",Crosshair,&Settings::crosshair,0,3,1,"",{{0,"Off"},{1,"Dot"},{2,"Cross"},{3,"Ring"}}},
        {"VRCrosshairColor","Crosshair color",Crosshair,&Settings::crosshairColor,0,4,1,"",{{0,"White"},{1,"Cyan"},{2,"Orange"},{3,"Green"},{4,"Magenta"}}},
        {"VRCrosshairSizeMM","Crosshair size",Crosshair,&Settings::crosshairMM,2,40,2," mm"},
        {"VRCrosshairOpacity","Crosshair opacity",Crosshair,&Settings::crosshairOpacity,10,100,5,"%"},
        {"VRCrosshairSurfaceDepth","Follow aim surface depth",Crosshair,&Settings::crosshairDepth},
        {"VRCrosshairDistance","Fallback / fixed distance",Crosshair,&Settings::crosshairDistance,.5f,15,.5f," m"},
        {"ThumbPoseMode","Thumb pose",Fingers,&Settings::thumbPose,0,1,1,"",{{0,"Tracked"},{1,"Thumbs up"}}},
        {"ThumbCurlGain","Thumb curl range",Fingers,&Settings::thumbGain,25,200,5,"%"},
        {"ThumbStraighten","Thumb tracking straighten",Fingers,&Settings::thumbStraighten,0,80,5,"%"},
        {"ThumbExtension","Extra thumb extension",Fingers,&Settings::thumbExtension,0,100,5,"%"},
        {"GunSurfaceHandPose","Pose fingers around gun",Fingers,&Settings::gunGripPose},
        {"GunHoldingCurl","Holding-hand grip curl",Fingers,&Settings::gunGripCurl,0,100,5,"%"},
        {"GunSupportingCurl","Supporting-hand grip curl",Fingers,&Settings::supportGripCurl,0,100,5,"%"},
        {"VisualBodyJiggle","Cosmetic jiggle (no world forces)",Jiggle,&Settings::jiggle},
        {"JiggleStrength","Motion strength",Jiggle,&Settings::jiggleStrength,0,400,10,"%"},
        {"JiggleFrequency","Spring frequency",Jiggle,&Settings::jiggleHz,2,10,.5f," Hz"},
        {"JiggleDamping","Spring damping",Jiggle,&Settings::jiggleDamping,.2f,2,.1f},
        {"JiggleChest","Chest influence",Jiggle,&Settings::jiggleChest,0,100,5,"%"},
        {"JiggleHips","Hip influence",Jiggle,&Settings::jiggleHips,0,100,5,"%"},
        {"HandImpactGain","Hand impact multiplier",ImpactTuning,&Settings::handImpactGain,1,30,1,"x"},
        {"GunImpactGain","Gun impact multiplier",ImpactTuning,&Settings::gunImpactGain,1,30,1,"x"},
        {"ImpactMaxNS","Impact impulse safety cap",ImpactTuning,&Settings::impactMaxNS,10,600,10," Ns"},
        {"ImpactCooldownMS","Same-object impact cooldown",ImpactTuning,&Settings::impactCooldownMS,60,400,20," ms"},
        {"GunWallRadiusCM","Gun wall hull radius",GunCollision,&Settings::gunWallRadiusCM,1,8,.5f," cm"},
        {"GunWallLimitCM","Maximum visual displacement",GunCollision,&Settings::gunWallLimitCM,25,120,5," cm"},
        {"GunWallRelease","Collision release response",GunCollision,&Settings::gunWallRelease,10,100,5,"%"},
        {"PortalContactBypass","Clear contact in portal environment",GunCollision,&Settings::portalContactBypass},
        {"PalmSnapCarry","Snap hand-held prop toward palm",PropPose,&Settings::palmSnap},
        {"PalmCarryOffsetCM","Palm to object center",PropPose,&Settings::palmCarryCM,0,25,1," cm"},
        {"PropShapeHandPose","Approximate held-object hand pose",PropPose,&Settings::propShapePose},
        {"PropPoseCurl","Held-object finger curl",PropPose,&Settings::propPoseCurl,0,100,5,"%"},
        {"ChestScalePercent","Chest width / depth",BodyShape,&Settings::chestScale,75,200,5,"%"},
        {"HipScalePercent","Hip width / depth",BodyShape,&Settings::hipScale,75,200,5,"%"},
        {"PhysicalHandPush","Hands push movable objects",PhysicalContact,&Settings::handPush},
        {"PhysicalGunPush","Gun pushes movable objects",PhysicalContact,&Settings::gunPush},
        {"GunWallContact","Gun wall contact / haptics",PhysicalContact,&Settings::gunWallContact},
        {"ContactPushStrength","Overall impulse multiplier",ImpactTuning,&Settings::pushGain,.1f,3,.1f,"x"},
        {"PhysicalHandRadiusCM","Hand contact radius",PhysicalContact,&Settings::pushRadiusCM,2,8,1," cm"},
        {"PhysicalGunRadiusCM","Gun contact radius",PhysicalContact,&Settings::gunContactRadiusCM,3,12,1," cm"},
        {"PhysicalContactHz","Contact query budget",PhysicalContact,&Settings::pushHz,15,60,5," Hz"},
        {"RigidFittedTorso","Rigid fitted torso",BodyTuning,&Settings::rigidTorso},
        {"TorsoHeightPercent","Fixed torso length",BodyTuning,&Settings::torsoHeight,80,120,2,"%"},
        {"BodyCameraClearanceCM","Torso camera clearance",BodyTuning,&Settings::bodyClearanceCM,0,15,.5f," cm"},
        {"WorldHandContact","Visual hand collision",WorldHands,&Settings::handContact},
        {"WorldHandContactPose","Flat palm at surfaces",WorldHands,&Settings::contactPose},
        {"WorldHandContactHaptics","Touch haptics",WorldHands,&Settings::contactHaptics},
        {"WorldHandRadiusCM","Palm collision radius",WorldHands,&Settings::contactRadiusCM,1,7,.5f," cm"},
        {"WorldHandContactHz","Contact update budget",WorldHands,&Settings::contactHz,20,90,5," Hz"},
        {"WorldHandPoseOverride","Squeeze overrides flat palm",WorldHands,&Settings::contactOverride,20,100,5,"%"},
        {"PropGrabMode","Carry method",PropGrab,&Settings::propGrabMode,0,1,1,"",{{0,"Native float grab"},{1,"Palm-held native physics"}}},
        {"HandGrabReachCM","Hand grab reach",PropGrab,&Settings::handGrabReachCM,10,100,5," cm"},
        {"ControllerThrows","Controller release velocity",Throwing,&Settings::enhancedThrow},
        {"ControllerThrowGain","Throw speed multiplier",Throwing,&Settings::throwGain,.25f,2,.05f,"x"},
        {"ControllerThrowMaxMPS","Maximum release speed",Throwing,&Settings::throwMaxMPS,2,20,1," m/s"},
        {"ControllerThrowSpin","Controller spin influence",Throwing,&Settings::throwSpin,0,200,10,"%"},
        {"BodyCalibrationEnabled","Use fitted body",Measurements,&Settings::bodyFit},
        {"BodyEyeHeightCM","Eye height (reference)",Measurements,&Settings::bodyEyeCM,90,220,1," cm"},
        {"BodyControllerSpanCM","Controller-to-controller span",Measurements,&Settings::bodySpanCM,80,240,1," cm"},
        {"BodyShoulderWidthCM","Shoulder width (estimated)",Measurements,&Settings::bodyWidthCM,20,65,1," cm"},
        {"BodyShoulderDropCM","Eye to shoulder drop",Measurements,&Settings::bodyDropCM,8,50,.5f," cm"},
        {"BodyNeckBackCM","Shoulders behind head",Measurements,&Settings::bodyNeckBackCM,-10,20,.5f," cm"},
        {"BodyYawOffset","Torso heading adjustment",Measurements,&Settings::bodyYaw,-60,60,5," deg"},
        {"ControllerAlignedHands","Fit hands to controller grips",HandFit,&Settings::controllerHands},
        {"ControllerHandAnchor","Controller anchor",HandFit,&Settings::handAnchor,0,1,1,"",{{0,"Wrist joint"},{1,"Palm / grip center"}}},
        {"ControllerPalmDepth","Palm anchor depth",HandFit,&Settings::palmDepth,50,150,5,"%"},
        {"BodyUprightTorso","Upright fitted torso",BodyTuning,&Settings::bodyUpright},
        {"WristCuffInflation","Wrist cuff inflation",BodyShape,&Settings::wristCuff,100,350,10,"%"},
        {"ShowHolsteredGun","Show holstered gun in portals",BackGun,&Settings::backGun},
        {"BackGunForwardCM","Forward / backward",BackGun,&Settings::backGunX,-40,40,1," cm"},
        {"BackGunRightCM","Right / left",BackGun,&Settings::backGunY,-40,40,1," cm"},
        {"BackGunUpCM","Up / down",BackGun,&Settings::backGunZ,-40,40,1," cm"},
        {"BackGunPitch","Pitch",BackGun,&Settings::backGunPitch,-180,180,5," deg"},
        {"BackGunYaw","Yaw",BackGun,&Settings::backGunYaw,-180,180,5," deg"},
        {"BackGunRoll","Roll",BackGun,&Settings::backGunRoll,-180,180,5," deg"},
        {"BodyVolumeEnabled","Body shape controls",BodyShape,&Settings::bodyVolume},
        {"WristWidthPercent","Wrist / palm width",BodyShape,&Settings::wristWidth,80,160,2,"%"},
        {"ForearmWidthPercent","Forearm thickness",BodyShape,&Settings::forearmWidth,80,160,2,"%"},
        {"UpperArmWidthPercent","Upper arm thickness",BodyShape,&Settings::upperArmWidth,80,160,2,"%"},
        {"HandSizePercent","Hand size",BodyShape,&Settings::handSize,75,135,2,"%"},
        {"TorsoWidthPercent","Torso thickness",BodyShape,&Settings::torsoWidth,80,140,2,"%"},
        {"HolsterReachTest","Test shoulder reach with haptics",HolsterZones,&Settings::holsterReachTest},
        {"LeftHolsterForwardCM","Forward / backward",LeftHolster,&Settings::holsterLeftX,-60,60,1," cm"},
        {"LeftHolsterRightCM","Right / left",LeftHolster,&Settings::holsterLeftY,-60,60,1," cm"},
        {"LeftHolsterUpCM","Up / down",LeftHolster,&Settings::holsterLeftZ,-60,60,1," cm"},
        {"LeftHolsterSizePercent","Zone radius scale",LeftHolster,&Settings::holsterLeftSize,25,250,5,"%"},
        {"RightHolsterForwardCM","Forward / backward",RightHolster,&Settings::holsterRightX,-60,60,1," cm"},
        {"RightHolsterRightCM","Right / left",RightHolster,&Settings::holsterRightY,-60,60,1," cm"},
        {"RightHolsterUpCM","Up / down",RightHolster,&Settings::holsterRightZ,-60,60,1," cm"},
        {"RightHolsterSizePercent","Zone radius scale",RightHolster,&Settings::holsterRightSize,25,250,5,"%"},
        {"ArmVisibility","Tracked arms",Arms,&Settings::arms,0,2,1,"",{{0,"Full arms"},{1,"Hands only"},{2,"Gun only"}}},
        {"ShowLeftArm","Show left hand / arm",Arms,&Settings::leftArm},
        {"ShowRightArm","Show right hand / arm",Arms,&Settings::rightArm},
        {"FingerTracking","Finger animation",Arms,&Settings::fingerTracking},
        {"FingerCurlRange","Finger curl range",Arms,&Settings::fingerRange,50,250,10,"%"},
        {"GunGrabRightCM","Grab offset right / left",GrabPoint,&Settings::grabXCM,-50,50,.5f," cm"},
        {"GunGrabUpCM","Grab offset up / down",GrabPoint,&Settings::grabYCM,-50,50,.5f," cm"},
        {"GunGrabForwardCM","Grab offset forward / backward",GrabPoint,&Settings::grabZCM,-50,50,.5f," cm"},
        {"BodyWristTwist","Forearm twist distribution",BodyTuning,&Settings::wristTwist,0,100,5,"%"},
        {"MatchPortalGunPose","Match tracked gun in portals",Body,&Settings::portalGunPose},
        {"FiringHandOnGrip","Keep firing hand on gun",Arms,&Settings::gripHand},
        {"SupportGripRadiusCM","Grab radius",GripZonePage,&Settings::supportRadiusCM,5,28,1," cm"},
        {"FirstPersonBody","Show native Chell body",Body,&Settings::body},
        {"FirstPersonBodyHideUpper","Hide head / untracked arms",Body,&Settings::hideUpper},
        {"FirstPersonBodyBackOffset","Legacy body backward offset",Body,&Settings::bodyBack,0,24,1," units"},
        {"LeftArmForwardCM","Forward / backward",LeftArm,&Settings::leftX,-25,25,.5f," cm"},
        {"LeftArmRightCM","Right / left",LeftArm,&Settings::leftY,-25,25,.5f," cm"},
        {"LeftArmUpCM","Up / down",LeftArm,&Settings::leftZ,-25,25,.5f," cm"},
        {"LeftArmPitch","Pitch",LeftArm,&Settings::leftPitch,-180,180,5," deg"},
        {"LeftArmYaw","Yaw",LeftArm,&Settings::leftYaw,-180,180,5," deg"},
        {"LeftArmRoll","Roll",LeftArm,&Settings::leftRoll,-180,180,5," deg"},
        {"RightArmForwardCM","Forward / backward",RightArm,&Settings::rightX,-25,25,.5f," cm"},
        {"RightArmRightCM","Right / left",RightArm,&Settings::rightY,-25,25,.5f," cm"},
        {"RightArmUpCM","Up / down",RightArm,&Settings::rightZ,-25,25,.5f," cm"},
        {"RightArmPitch","Pitch",RightArm,&Settings::rightPitch,-180,180,5," deg"},
        {"RightArmYaw","Yaw",RightArm,&Settings::rightYaw,-180,180,5," deg"},
        {"RightArmRoll","Roll",RightArm,&Settings::rightRoll,-180,180,5," deg"},
        {"ShowPortalGun","Show portal gun",Gun,&Settings::gun},
        {"LeftHandGunGrip","Off-hand support grip",Support,&Settings::support},
        {"TwoHandedAiming","Two-handed aiming",Support,&Settings::twoHandAim},
        {"TwoHandedInfluence","Off-hand aim influence",Support,&Settings::twoHandPercent,0,100,10,"%"},
        {"GunForwardCM","Forward / backward",Gun,&Settings::gunX,-30,30,.5f," cm"},
        {"GunRightCM","Right / left",Gun,&Settings::gunY,-30,30,.5f," cm"},
        {"GunUpCM","Up / down",Gun,&Settings::gunZ,-30,30,.5f," cm"},
        {"GunPitch","Pitch",Gun,&Settings::gunPitch,-180,180,5," deg"},
        {"GunYaw","Yaw",Gun,&Settings::gunYaw,-180,180,5," deg"},
        {"GunRoll","Roll",Gun,&Settings::gunRoll,-180,180,5," deg"},
        {"GripHoldNear","Grab while already squeezing",Support,&Settings::gripHoldNear},
        {"ShowSupportingHand","Always show supporting hand",Support,&Settings::showSupportHand},
        {"SnapSupportHand","Snap hand to grip anchor",Support,&Settings::snapSupportHand},
        {"SlideSupportGrip","Slide grip along gun",Support,&Settings::slideSupportGrip},
        {"GripZoneScale","Grip zone scale",GripZonePage,&Settings::gripZoneScale,50,250,10,"%"},
        {"GripZoneLengthCM","Underside coverage length",GripZonePage,&Settings::gripZoneLengthCM,10,65,2," cm"},
        {"GripZoneRightCM","Zone right / left",GripZonePage,&Settings::gripZoneXCM,-25,25,.5f," cm"},
        {"GripZoneUpCM","Zone up / down",GripZonePage,&Settings::gripZoneYCM,-25,25,.5f," cm"},
        {"GripZoneForwardCM","Zone forward / backward",GripZonePage,&Settings::gripZoneZCM,-30,30,.5f," cm"},
        {"SupportHandForwardCM","Hand forward / backward",SupportHand,&Settings::supportHandXCM,-25,25,.5f," cm"},
        {"SupportHandRightCM","Hand right / left",SupportHand,&Settings::supportHandYCM,-25,25,.5f," cm"},
        {"SupportHandUpCM","Hand up / down",SupportHand,&Settings::supportHandZCM,-25,25,.5f," cm"},
        {"SupportHandPitch","Hand pitch",SupportHand,&Settings::supportHandPitch,-180,180,5," deg"},
        {"SupportHandYaw","Hand yaw",SupportHand,&Settings::supportHandYaw,-180,180,5," deg"},
        {"SupportHandRoll","Hand roll",SupportHand,&Settings::supportHandRoll,-180,180,5," deg"},
        {"ShoulderHolster","Shoulder holster",Holster,&Settings::holster},
        {"GunHandTransfer","Hand-to-hand transfer",Holster,&Settings::transfer},
        {"ShoulderSlot","Shoulder slot",Holster,&Settings::shoulderSlot,0,3,1,"",{{0,"Either shoulder"},{1,"Gun-side shoulder"},{2,"Left shoulder"},{3,"Right shoulder"}}},
        {"ShoulderRadiusCM","Shoulder grab radius",HolsterZones,&Settings::shoulderRadiusCM,8,35,1," cm"},
        {"ShoulderWidthCM","Shoulder side offset",HolsterZones,&Settings::shoulderWidthCM,8,40,1," cm"},
        {"ShoulderBackCM","Shoulder backward offset",HolsterZones,&Settings::shoulderBackCM,0,35,1," cm"},
        {"ShoulderDownCM","Shoulder downward offset",HolsterZones,&Settings::shoulderDownCM,-10,35,1," cm"},
        {"HolsterShowHands","Show hands while stowed",Holster,&Settings::holsterHands},
        {"WeaponGrabHaptics","Support / portal grab haptic",Feedback,&Settings::grabHaptics},
        {"WeaponHapticStrength","Weapon haptic strength",Feedback,&Settings::grabHapticStrength,0,1,.05f},
        {"HolsterHaptics","Stow / draw haptic",Feedback,&Settings::holsterHaptics},
        {"TransferHaptics","Hand transfer haptic",Feedback,&Settings::transferHaptics},
        {"HolsterHoverHaptics","Shoulder proximity haptic",Feedback,&Settings::holsterHover},
        {"GrabProximityHaptics","Gun proximity haptic",Feedback,&Settings::grabHover},
        {"WeaponHapticDurationMS","Pulse duration",Feedback,&Settings::hapticDurationMS,30,250,10," ms"},
        {"WeaponHapticFrequency","Pulse frequency",Feedback,&Settings::hapticFrequency,80,240,20," Hz"},
        {"BodyIK","Full native Chell body IK",Body,&Settings::bodyIK},
        {"BodyIKArmScale","Body arm reach scale",BodyTuning,&Settings::bodyArmScale,.8f,1.5f,.05f,"x"},
        {"BodyIKInfluence","Body IK influence",BodyTuning,&Settings::bodyIKInfluence,0,1,.1f},
        {"BodyIKAutoReach","Reach controller exactly",BodyTuning,&Settings::bodyAutoReach},
        {"BodyIKPortalPose","Match portal avatar pose",Body,&Settings::bodyPortalPose},
        {"MovementEnabled","Thumbstick movement",Movement,&Settings::movement},
        {"MovementSpeed","Movement speed",Movement,&Settings::moveSpeed,.1f,2,.1f,"x"},
        {"MovementDeadzone","Movement deadzone",Movement,&Settings::moveDeadzone,0,.6f,.05f},
        {"MovementDirection","Movement follows",Movement,&Settings::moveDirection,0,1,1,"",{{0,"Head"},{1,"Movement controller"}}},
        {"LeftHanded","Left-handed controls",Movement,&Settings::leftHanded},
        {"TurningEnabled","Thumbstick turning",Turning,&Settings::turning},
        {"SnapTurning","Turning mode",Turning,&Settings::snap,0,1,1,"",{{0,"Smooth"},{1,"Snap"}}},
        {"SnapTurnAngle","Snap angle",Turning,&Settings::snapAngle,15,90,15," deg"},
        {"SmoothTurnDegrees","Smooth turn speed",Turning,&Settings::turnDegrees,30,360,15," deg/s"},
        {"TurnDeadzone","Turning deadzone",Turning,&Settings::turnDeadzone,.1f,.7f,.05f},
        {"SeatedMode","Seated height boost",Comfort,&Settings::seated},
        {"SeatedBoostCM","Seated boost amount",Comfort,&Settings::seatBoost,0,70,2.5f," cm"},
        {"HeightOffsetCM","Additional height offset",Comfort,&Settings::height,-60,60,2.5f," cm"},
        {"6DOF","Physical leaning",Comfort,&Settings::roomscale},
        {"CameraCollision","Camera wall collision",Comfort,&Settings::collision},
        {"AutoCalibration","Automatic body alignment",Comfort,&Settings::autoCalibration},
        {"ComfortVignette","Movement vignette",Comfort,&Settings::vignette},
        {"VignetteStrength","Vignette strength",Comfort,&Settings::vignetteStrength,.1f,1,.1f},
        {"AimMode","Native aim guide",Crosshair,&Settings::aim,0,2,1,"",{{0,"Off"},{1,"Native HUD"},{2,"Laser"}}},
        {"EyeRenderWidth","Eye image width",Visuals,&Settings::eyeWidth,1024,3200,128," px",{{1024,"1024 px"},{1280,"1280 px"},{1536,"1536 px"},{1728,"1728 px"},{2016,"2016 px"},{2304,"2304 px"},{2560,"2560 px"},{2912,"2912 px"},{3200,"3200 px"}},true},
        {"AntiAliasing","Edge anti-aliasing",Visuals,&Settings::msaa,0,8,2,"",{{0,"Off"},{2,"2x MSAA"},{4,"4x MSAA"},{8,"8x MSAA"}},true},
        {"TextureQuality","Texture quality",Visuals,&Settings::texture,0,2,1,"",{{2,"Low"},{1,"Medium"},{0,"High"}},true},
        {"AnisotropicFiltering","Anisotropic filtering",Visuals,&Settings::filter,1,16,1,"",{{1,"1x"},{2,"2x"},{4,"4x"},{8,"8x"},{16,"16x"}},true},
        {"LegacyScreenEffects","Legacy HDR / screen effects",Effects,&Settings::legacyEffects,0,1,1,"",{},true},
        {"MotionBlur","Motion blur",Effects,&Settings::blur},
        {"Bloom","Bloom",Effects,&Settings::bloom},
        {"DynamicShadows","Dynamic shadows",Visuals,&Settings::shadows},
        {"RenderWindow","Desktop mirror scene",Visuals,&Settings::mirror},
        {"ShowFPS","FPS display",Visuals,&Settings::showFps},
        {"FrameTimingLog","Frame timing log",Visuals,&Settings::frameTimingLog},
        {"SpatialUpscaler","Spatial upscaling",Upscaling,&Settings::upscaler,0,3,1,"",{{0,"Off"},{1,"SGSR fast"},{2,"SGSR edge quality"},{3,"Bilinear reference"}}},
        {"UpscaleOutputWidth","Upscaled image width",Upscaling,&Settings::upscaleWidth,1728,3200,128," px",{{1728,"1728 px"},{2016,"2016 px"},{2304,"2304 px"},{2560,"2560 px"},{2912,"2912 px"},{3200,"3200 px"}},true},
        {"UpscaleSharpness","Upscale sharpness",Upscaling,&Settings::upscaleSharpness,1,2,.1f},
        {"MenuAlwaysOnTop","Foreground menu priority",Menu,&Settings::menuTop},
        {"MenuLaser","Menu laser pointers",Menu,&Settings::menuLaser},
        {"MenuHighlights","Selection highlights",Menu,&Settings::menuHighlight},
        {"MenuHaptics","Hover / click haptics",Menu,&Settings::menuHaptics},
        {"MenuHapticStrength","Haptic strength",Menu,&Settings::hapticStrength,.05f,1,.05f},
        {"MenuGrabControls","Grab / scale controls",Menu,&Settings::menuGrab},
        {"MenuFollowHead","Keep panel in front of head",Menu,&Settings::menuFollow},
        {"MenuWidthMeters","Panel width",Menu,&Settings::menuWidth,.5f,2.5f,.1f," m"},
        {"MenuDepthMeters","Panel depth",Menu,&Settings::menuDepth,.4f,4,.1f," m"}
    };
    return all;
}
inline float Get(const Settings& s,const Option& o) {return std::visit([&](auto m){return float(s.*m);},o.member);}
inline void Set(Settings& s,const Option& o,float value) {
    if(!std::isfinite(value))return;
    value=std::clamp(value,o.low,o.high);
    if(!o.choices.empty()) {
        float nearest=o.choices[0].first;
        for(const auto& choice:o.choices)if(std::abs(choice.first-value)<std::abs(nearest-value))nearest=choice.first;
        value=nearest;
    }
    std::visit([&](auto m){using T=std::decay_t<decltype(s.*m)>;s.*m=static_cast<T>(value);},o.member);
}
inline void Change(Settings& s,const Option& o,int direction) {
    float value=Get(s,o);
    if(!o.choices.empty()) {
        int at=0;float error=1e20f;
        for(int i=0;i<int(o.choices.size());++i)if(std::abs(value-o.choices[i].first)<error) {at=i;error=std::abs(value-o.choices[i].first);}
        at=std::clamp(at+(direction<0?-1:1),0,int(o.choices.size())-1);value=o.choices[at].first;
    } else if(std::holds_alternative<bool Settings::*>(o.member)) value=value ? 0.f : 1.f;
    else value+=o.step*(direction<0?-1:1);
    Set(s,o,value);
}
inline std::string Number(float value) {std::ostringstream out;out<<std::fixed<<std::setprecision(2)<<value;auto s=out.str();while(s.back()=='0')s.pop_back();if(s.back()=='.')s.pop_back();return s;}
inline std::string Value(const Settings& s,const Option& o) {
    const float value=Get(s,o);std::string result;
    for(const auto& choice:o.choices)if(std::abs(choice.first-value)<.001f)result=choice.second;
    if(result.empty()) result=std::holds_alternative<bool Settings::*>(o.member)?(value?"On":"Off"):Number(value)+o.unit;
    return result+(o.restart?"  (restart)":"");
}
inline std::string Save(const Settings& s,std::string text) {
    for(const auto& o:Options()) text=VrSettings::SetValue(text,o.key,
        std::holds_alternative<bool Settings::*>(o.member)&&std::string_view(o.key)!="RenderWindow"?(Get(s,o)?"true":"false"):Number(Get(s,o)));
    return text;
}
template<class Map> inline void Load(Settings& s,const Map& input) {
    for(const auto& o:Options()) {
        const auto i=input.find(o.key);if(i==input.end())continue;
        bool valid=false;const auto value=VrSettings::ConfigValue<float>(i->second,Get(s,o),&valid);
        if(valid)Set(s,o,value);
    }
}
inline std::vector<int> On(Page page) {std::vector<int> rows;for(int i=0;i<int(Options().size());++i)if(Options()[i].page==page)rows.push_back(i);return rows;}
inline void ApplyPreset(Settings& s,int preset) {
    const int widths[]={2016,1728,1536,2016},samples[]={4,2,2,2},modes[]={0,2,1,2},outputs[]={2016,2016,2016,2560};
    const int i=std::clamp(preset,0,3);s.eyeWidth=widths[i];s.msaa=samples[i];s.upscaler=modes[i];s.upscaleWidth=outputs[i];
}
}
