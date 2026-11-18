#pragma once
#include <ctime>
#include "renderpolicy.h"
#include "twohandaim.h"
#include "gungrab.h"
#include "menuimage.h"
#include "holsterfit.h"
namespace {
matrix3x4_t MenuMatrix(const vr::HmdMatrix34_t& source) {matrix3x4_t out;std::memcpy(&out,&source,sizeof(out));return out;}
vr::HmdMatrix34_t OpenVrMatrix(const matrix3x4_t& source) {vr::HmdMatrix34_t out;std::memcpy(&out,&source,sizeof(out));return out;}
bool MenuDigital(vr::IVRInput* input,vr::VRActionHandle_t action) {
    vr::InputDigitalActionData_t state{};return input && action && input->GetDigitalActionData(action,&state,sizeof(state),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None && state.bActive && state.bState;
}
vr::InputAnalogActionData_t MenuAnalog(vr::IVRInput* input,vr::VRActionHandle_t action) {
    vr::InputAnalogActionData_t state{};if(!input||!action||input->GetAnalogActionData(action,&state,sizeof(state),vr::k_ulInvalidInputValueHandle)!=vr::VRInputError_None||!state.bActive)state={};return state;
}
void MenuKey(WORD key) {INPUT input{};input.type=INPUT_KEYBOARD;input.ki.wVk=key;SendInput(1,&input,sizeof(input));input.ki.dwFlags=KEYEVENTF_KEYUP;SendInput(1,&input,sizeof(input));}
void MenuMouse(bool down) {INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=down?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP;SendInput(1,&input,sizeof(input));}
}
bool VR::SaveGraphicsSettings() {
    char directory[MAX_PATH]{};if(!GetRuntimeBaseDirectory(directory,sizeof(directory)))return false;
    const auto config=std::filesystem::path(directory)/"VR"/"config.txt",temporary=std::filesystem::path(directory)/"VR"/"menu-config.tmp";
    try {
        std::ifstream input(config,std::ios::binary);if(!input)throw std::runtime_error("config not readable");
        std::string text((std::istreambuf_iterator<char>(input)),{});input.close();text=VrMenu::Save(m_Graphics,text);
        text=VrSettings::SetValue(text,"TurnSpeed",VrMenu::Number(m_Graphics.turnDegrees/1000));
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);output<<text;output.close();
        if(!output||!MoveFileExW(temporary.c_str(),config.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("atomic settings save failed");
        return true;
    } catch(const std::exception& error){PortalVrLog("VR settings save failed: %s",error.what());return false;}
}
void VR::ApplyGraphicsSettings() {
    if(m_Graphics.leftHanded!=m_LeftHanded)HandleSettingsCommand(m_Graphics.leftHanded?"portal1vr_hand_left":"portal1vr_hand_right");
    m_RenderWindow=m_Graphics.mirror?1:0;m_SnapTurning=m_Graphics.snap;m_SnapTurnAngle=m_Graphics.snapAngle;
    m_TurnSpeed=m_Graphics.turnDegrees/1000;m_FirstPersonBody=m_Graphics.body;m_FirstPersonBodyHideUpper=m_Graphics.hideUpper;
    m_FirstPersonBodyBackOffset=m_Graphics.bodyBack;m_LeftHandGunGrip=m_Graphics.support;m_6DOF=m_Graphics.roomscale;
    m_AutoCalibration=m_Graphics.autoCalibration;m_AimMode=m_Graphics.aim;
    m_LeftHandGunGripRadius=m_Graphics.supportRadiusCM*m_VRScale/100;
    auto live=m_Graphics;live.legacyEffects=m_ActiveLegacyEffects;
    const auto commands=VrGraphics::Commands(live);
    if(commands!=m_LastGraphicsCommandList) {
        m_LastGraphicsCommandList=commands;
        PortalVrLog("Applying changed live graphics commands (no video-mode commands)");
        m_Game->ClientCmd_Unrestricted(commands.c_str());
    }
    m_GraphicsApplyPending=false;
    if(m_Overlay)for(auto overlay:{m_MainMenuHandle,m_GraphicsHandle,m_GraphicsStagingHandle})if(overlay){
        const auto topResult=m_Overlay->SetOverlayFlag(overlay,vr::VROverlayFlags_SortWithNonSceneOverlays,false);
        m_Overlay->SetOverlayFlag(overlay,vr::VROverlayFlags_VisibleInDashboard,false);
        const auto sortResult=m_Overlay->SetOverlaySortOrder(overlay,m_Graphics.menuTop?(overlay==m_MainMenuHandle?1100:1200):(overlay==m_MainMenuHandle?10:20));
        PortalVrLog("VR menu layer overlay=%llu topResult=%d sortResult=%d",overlay,topResult,sortResult);
        // We provide pointers ourselves. Runtime-interactive mode can remove
        // the application's action focus, which previously stranded Escape.
        m_Overlay->SetOverlayFlag(overlay,vr::VROverlayFlags_MakeOverlaysInteractiveIfVisible,false);
    }
    PortalVrLog("VR settings applied arms=%d movement=%d turning=%d snap=%d menuTop=%d",m_Graphics.arms,m_Graphics.movement,m_Graphics.turning,m_Graphics.snap,m_Graphics.menuTop);
}
void VR::HideOwnedOverlays() {
    if(!m_Overlay)return;
    for(auto handle:{m_MainMenuHandle,m_GraphicsHandle,m_GraphicsStagingHandle,m_MenuChrome,
        m_MenuBeam[0],m_MenuBeam[1],m_MenuDot[0],m_MenuDot[1],m_VignetteHandle,m_FpsHandle})
        if(handle)m_Overlay->HideOverlay(handle);
}
void VR::RefreshSystemFocus(bool sampleActions) {
    auto* compositor=vr::VRCompositor();
    const bool dashboard=m_Overlay&&m_Overlay->IsDashboardVisible();
    const bool available=m_System&&m_System->IsInputAvailable();
    const bool pause=!m_System||m_System->ShouldApplicationPause();
    const bool scene=compositor&&compositor->CanRenderScene();
    const bool focused=VrFocus::OwnsInput(dashboard,available,pause,scene);
    const bool previous=m_SystemFocus.focused;
    m_SystemFocus.Focus(focused);
    if(previous!=focused) {
        PortalVrLog("System focus focused=%d dashboard=%d inputAvailable=%d pause=%d sceneFocus=%d",focused,dashboard,available,pause,scene);
        if(!focused){ReleaseMenuInput();m_GrabPoseValid=false;m_OptionalGripState={};m_OptionalSupportActive=m_LeftGripPressed=false;}
    }
    if(sampleActions&&focused&&m_SystemFocus.neutralRequired) {
        bool held=false;
        for(auto action:{m_MenuEscape,m_MenuToggle,m_MenuTriggers[0],m_MenuTriggers[1],m_MenuGrips[0],m_MenuGrips[1],
            m_MenuStickClicks[0],m_MenuStickClicks[1],m_MenuDpad[0],m_MenuDpad[1],m_MenuDpad[2],m_MenuDpad[3],
            ResolveAction(m_ActionPrimaryAttack),ResolveAction(m_ActionSecondaryAttack),ResolveAction(m_ActionUse),
            ResolveAction(m_ActionJump),ResolveAction(m_ActionCrouch),ResolveAction(m_ActionReload),
            ResolveAction(m_ActionPrevItem),ResolveAction(m_ActionNextItem),ResolveAction(m_ActionResetPosition),
            ResolveAction(m_ActionFlashlight),ResolveAction(m_Pause)})held|=MenuDigital(m_Input,action);
        for(auto action:{ResolveAction(m_ActionWalk),ResolveAction(m_ActionTurn),m_MenuSticks[0],m_MenuSticks[1]}) {
            const auto stick=MenuAnalog(m_Input,action);held|=std::abs(stick.x)>.15f||std::abs(stick.y)>.15f;
        }
        bool controllerActive=false;
        for(auto action:m_MenuTriggers) {
            vr::InputDigitalActionData_t trigger{};
            if(m_Input&&action&&m_Input->GetDigitalActionData(action,&trigger,sizeof(trigger),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None)
                controllerActive|=trigger.bActive;
        }
        m_SystemFocus.Sample(m_ActionStateFresh&&controllerActive,held);
        if(m_SystemFocus.Allowed()) {
            m_MenuEscapeHeld=m_MenuToggleHeld=false;m_MenuStickChord={};
            m_MenuGripHeld[0]=m_MenuGripHeld[1]=false;
            m_MenuSuppressUntil=GetTickCount64()+250;m_GraphicsDirty=true;
            m_PrevFrameTime=std::chrono::steady_clock::now();
            PortalVrLog("System input resumed after fresh neutral controller sample");
        }
    }
    if(!m_SystemFocus.focused)HideOwnedOverlays();
}
void VR::ReleaseMenuInput() {
    m_PropHold.Suspend();
    m_ThrowReleaseUntil=0;m_HandContact[0]=m_HandContact[1]={};
    m_Weapon.Suspend();m_LegacyHapticUntil[0]=m_LegacyHapticUntil[1]=0;m_WeaponConsumesGrip=true;m_UseGripBlocked[0]=m_UseGripBlocked[1]=true;
    m_OptionalSupportActive=false;m_OptionalGripState.active=false;m_OptionalGripState.wasPressed=true;
    MenuMouse(false);m_Game->ClientCmd_Unrestricted("-attack;-attack2;-use;-jump;-duck;-reload");m_UseCommandHeld=false;
    m_MenuGrabHand=-1;m_MenuGrabCorner=-1;m_MenuHoveredRow=-1;m_MenuHoverHand=-1;
    m_NativeMenuHoverPanel=0;
    m_MenuMouseHand=-1;m_PressedTurn=false;m_PrevFrameTime=std::chrono::steady_clock::now();
    m_MenuTriggerHeld[0]=m_MenuTriggerHeld[1]=false;
    m_HandSwitchSuppressUntil=GetTickCount64()+350;
}
void VR::CloseVrMenus(bool resume) {
    m_BodyCapture.Cancel();m_GraphicsOpen=false;if(m_Overlay){if(m_GraphicsHandle)m_Overlay->HideOverlay(m_GraphicsHandle);
        if(m_GraphicsStagingHandle)m_Overlay->HideOverlay(m_GraphicsStagingHandle);
        if(m_MenuChrome)m_Overlay->HideOverlay(m_MenuChrome);
        for(int i=0;i<2;++i){if(m_MenuBeam[i])m_Overlay->HideOverlay(m_MenuBeam[i]);if(m_MenuDot[i])m_Overlay->HideOverlay(m_MenuDot[i]);}}
    m_MenuFocusPanel=0;
    ReleaseMenuInput();if(resume){m_Game->ClientCmd_Unrestricted("gameui_hide");m_MenuWasVisible=false;m_MenuCursorSeen=false;}
    m_MenuSuppressUntil=GetTickCount64()+350;PortalVrLog("VR menu close resume=%d",resume);
}
void VR::SetMenuPlacement(bool focusMod) {
    if(!m_Overlay || !m_Poses[0].bPoseIsValid)return;
    m_MenuLayoutFocused=focusMod;
    const auto head=MenuMatrix(m_Poses[0].mDeviceToAbsoluteTracking);
    m_MenuTransform=m_GraphicsOpen?MenuPose::Beside(head,m_Graphics.menuDepth,m_Graphics.menuWidth,0,focusMod):MenuPose::Front(head,m_Graphics.menuDepth);
    m_ModMenuTransform=MenuPose::Beside(head,m_Graphics.menuDepth,m_Graphics.menuWidth,1,focusMod);
    m_ModMenuWidth=m_Graphics.menuWidth;m_MenuPoseValid=true;ApplyMenuPlacement();
}
void VR::ApplyMenuPlacement() {
    if(!m_Overlay||!m_MenuPoseValid)return;
    const auto origin=vr::VRCompositor()->GetTrackingSpace();
    for(auto overlay:{m_MainMenuHandle,m_GraphicsHandle,m_GraphicsStagingHandle}) {
        if(!overlay)continue;const int panel=overlay==m_MainMenuHandle?0:1;
        const auto transform=OpenVrMatrix(MenuPanelTransform(panel));
        m_Overlay->SetOverlayTransformAbsolute(overlay,origin,&transform);
        m_Overlay->SetOverlayWidthInMeters(overlay,MenuPanelWidth(panel));
        m_Overlay->SetOverlayCurvature(overlay,0);
        m_Overlay->SetOverlayFlag(overlay,vr::VROverlayFlags_SortWithNonSceneOverlays,false);
        m_Overlay->SetOverlayFlag(overlay,vr::VROverlayFlags_VisibleInDashboard,false);
        m_Overlay->SetOverlaySortOrder(overlay,m_Graphics.menuTop?(panel?1200:1100):(panel?20:10));
    }
}
bool VR::HandleMenuFallback() {
    bool escape=MenuDigital(m_Input,m_MenuEscape),toggle=MenuDigital(m_Input,m_MenuToggle);
    const bool chord=m_MenuStickChord.Update(MenuDigital(m_Input,m_MenuStickClicks[0]),MenuDigital(m_Input,m_MenuStickClicks[1]));
    // Use the explicit physical VIEW binding. Legacy application-menu masks
    // can alias Frame D-pad buttons, so they are not a safe Escape fallback.
    const bool pressedEscape=escape&&!m_MenuEscapeHeld,pressedToggle=toggle&&!m_MenuToggleHeld;
    m_MenuEscapeHeld=escape;m_MenuToggleHeld=toggle;
    if(pressedEscape) {
        if(m_GraphicsOpen||m_Game->IsCursorVisible())CloseVrMenus(true);
        else {ReleaseMenuInput();m_Game->ClientCmd_Unrestricted("gameui_activate");SetMenuPlacement();}
        MenuHaptic(0,true);return true;
    }
    if(chord||pressedToggle) {
        PortalVrLog("VR settings shortcut source=%s",chord?"both-stick-clicks":"right-menu");
        OpenGraphicsMenu(true);MenuHaptic(0,true);MenuHaptic(1,true);return true;
    }
    return false;
}
void VR::OpenGraphicsMenu(bool focusMod) {
    if(!m_Overlay)return;
    for(int slot=0;slot<2;++slot) {
        auto& handle=slot?m_GraphicsStagingHandle:m_GraphicsHandle;if(handle)continue;
        const auto error=m_Overlay->CreateOverlay(slot?"portal.frame.settings.back":"portal.frame.settings","Portal Frame VR Settings",&handle);
        if(error!=vr::VROverlayError_None){PortalVrLog("VR settings create error=%d",error);return;}
        m_Overlay->SetOverlayInputMethod(handle,vr::VROverlayInputMethod_None);
        m_Overlay->SetOverlayFlag(handle,vr::VROverlayFlags_IgnoreTextureAlpha,true);
        m_Overlay->SetOverlayAlpha(handle,1);m_Overlay->SetOverlayColor(handle,1,1,1);
        m_Overlay->SetOverlayTexelAspect(handle,1);
        const vr::VRTextureBounds_t bounds{0,0,1,1};m_Overlay->SetOverlayTextureBounds(handle,&bounds);
    }
    m_BodyCapture.Cancel();ReleaseMenuInput();if(!m_Game->IsCursorVisible())m_Game->ClientCmd_Unrestricted("gameui_activate");
    m_MenuPage=VrMenu::Home;m_GraphicsSelected=0;m_GraphicsOpen=true;m_GraphicsDirty=true;m_MenuUploadLogged=false;
    m_MenuFocusPanel=focusMod?1:0;m_MenuCursorSeen=m_Game->IsCursorVisible();
    m_MenuSuppressUntil=GetTickCount64()+250;SetMenuPlacement(focusMod);m_MenuWasVisible=true;
    PortalVrLog("VR settings panel opened alongside native menu focus=%d handle=%llu",focusMod,m_GraphicsHandle);
}
void VR::ChangeGraphicsSetting(int row,int direction) {
    const auto rows=VrMenu::Rows(m_Graphics,m_MenuPage);if(row<0||row>=int(rows.size()))return;const auto selected=rows[row];
    switch(selected.action) {
    case VrMenu::OpenPage:m_BodyCapture.Cancel();m_MenuPage=VrMenu::Page(selected.target);m_GraphicsSelected=0;break;
    case VrMenu::Back:m_BodyCapture.Cancel();m_MenuPage=VrMenu::Parent(m_MenuPage);m_GraphicsSelected=0;break;
    case VrMenu::SetStandingHeight: {
        const auto previous=m_Graphics;const float height=StandingEyeHeightCM();
        if(height<90||height>220){m_GraphicsStatus="Stand upright with valid floor tracking first.";break;}
        m_Graphics.bodyEyeCM=height;
        if(SaveGraphicsSettings())m_GraphicsStatus="Standing height saved. Physical crouch starts from here.";
        else {m_Graphics=previous;m_GraphicsStatus="Save failed. Previous height kept.";}break;
    }
    case VrMenu::StartTPose:
        ReleaseMenuInput();m_BodyCapture.Start();m_CalibrationPrevious=0;m_CalibrationProgress=-1;m_MenuPage=VrMenu::TPose;m_GraphicsSelected=0;
        m_GraphicsStatus="Release triggers; stand straight with arms sideways. Hold BOTH triggers 1 second.";break;
    case VrMenu::ApplyTPose:
        if(m_BodyCapture.review)ApplyBodyMeasurements(m_BodyCapture.measured);else m_GraphicsStatus="Capture a steady standing T-pose first.";break;
    case VrMenu::ApplyManualFit:
        ApplyBodyMeasurements({m_Graphics.bodyEyeCM,m_Graphics.bodySpanCM,m_Graphics.bodyWidthCM,m_Graphics.bodyDropCM});break;
    case VrMenu::CancelTPose:m_BodyCapture.Cancel();m_GraphicsStatus="Capture discarded. Saved fit unchanged.";break;
    case VrMenu::ResetBodyFit:case VrMenu::ResetHandFit:case VrMenu::OriginalBodyShape:case VrMenu::SetHolster: {
        const auto previous=m_Graphics;
        if(selected.action==VrMenu::ResetBodyFit){m_BodyCapture.Cancel();m_Graphics.bodyFit=false;}
        else if(selected.action==VrMenu::ResetHandFit) {
            m_Graphics.leftX=m_Graphics.leftY=m_Graphics.leftZ=m_Graphics.leftPitch=m_Graphics.leftYaw=m_Graphics.leftRoll=0;
            m_Graphics.rightX=m_Graphics.rightY=m_Graphics.rightZ=m_Graphics.rightPitch=m_Graphics.rightYaw=m_Graphics.rightRoll=0;
            m_Graphics.controllerHands=true;
        } else if(selected.action==VrMenu::OriginalBodyShape)m_Graphics.wristWidth=m_Graphics.forearmWidth=m_Graphics.upperArmWidth=m_Graphics.handSize=m_Graphics.torsoWidth=100;
        else {
            const int physical=selected.target;const bool logicalLeft=(physical==0)!=m_LeftHanded;
            if(!m_ActionStateFresh||!(logicalLeft?m_LeftControllerPose.isValid:m_RightControllerPose.isValid)){m_GraphicsStatus="Matching controller must be tracked.";break;}
            Vector f=m_HmdForward;f.z=0;if(!TwoHandAim::Unit(f))f={1,0,0};const auto r=CrossProduct(f,{0,0,1});
            const auto hand=logicalLeft?GetLeftControllerAbsPos():GetRightControllerAbsPos();
            const auto oldOffset=physical?Vector(previous.holsterRightX,previous.holsterRightY,previous.holsterRightZ):Vector(previous.holsterLeftX,previous.holsterLeftY,previous.holsterLeftZ);
            const auto base=HolsterCenter(physical)-(f*oldOffset.x+r*oldOffset.y+Vector(0,0,oldOffset.z))*(m_VRScale/100);Vector offset;
            if(!HolsterFit::Capture(hand,base,f,r,m_VRScale/100,offset)){m_GraphicsStatus="Controller is too far from the shoulder setup area.";break;}
            if(physical){m_Graphics.holsterRightX=offset.x;m_Graphics.holsterRightY=offset.y;m_Graphics.holsterRightZ=offset.z;}
            else {m_Graphics.holsterLeftX=offset.x;m_Graphics.holsterLeftY=offset.y;m_Graphics.holsterLeftZ=offset.z;}
            MenuHaptic(physical,true);
        }
        if(SaveGraphicsSettings()){m_GraphicsApplyPending=true;m_GraphicsStatus="Fit saved. Changes apply immediately.";}
        else {m_Graphics=previous;m_GraphicsStatus="Save failed. Previous fit preserved.";}break;
    }
    case VrMenu::ResetGunGrip:case VrMenu::SetGrabPoint:case VrMenu::ResetGrabPoint: {
        const auto previous=m_Graphics;Vector next(0,0,0);
        const bool primaryLeft=(WeaponHand()==0)!=m_LeftHanded;
        if(selected.action==VrMenu::SetGrabPoint) {
            if(m_Weapon.stowed||!WeaponTracked()||!m_LeftControllerPose.isValid||!m_RightControllerPose.isValid||!m_PortalAimLastSeen) {
                m_GraphicsStatus="Draw the gun and track both controllers first.";break;
            }
            if(!GunGrab::Capture(GetGunControllerFrame(),TwoHandAim::Position(GetAvatarHandFrame(primaryLeft)),
                TwoHandAim::Position(GetAvatarHandFrame(!primaryLeft)),m_VRScale,{previous.grabXCM,previous.grabYCM,previous.grabZCM},next)) {
                m_GraphicsStatus="Touch the desired grip with the free hand, closer to the gun.";break;
            }
        }
        m_Graphics.grabXCM=next.x;m_Graphics.grabYCM=next.y;m_Graphics.grabZCM=next.z;
        if(!SaveGraphicsSettings()){m_Graphics=previous;m_GraphicsStatus="Save failed. Previous grip kept.";break;}
        m_OptionalSupportActive=false;m_OptionalGripState.active=false;m_OptionalGripState.wasPressed=true;
        m_GraphicsStatus=selected.action==VrMenu::SetGrabPoint?"Free hand sampled. New grip saved for either hand.":"Authored gun grab point restored.";
        MenuHaptic(primaryLeft?0:1,true);PortalVrLog("Gun grab point saved offsetCM=%f,%f,%f",next.x,next.y,next.z);break;
    }
    case VrMenu::TestHaptic:
        ControllerHaptic(selected.target,m_Graphics.grabHapticStrength,m_Graphics.hapticDurationMS/1000,m_Graphics.hapticFrequency,"user-test");
        m_GraphicsStatus="Haptic requested. Binding and result saved silently.";break;
    case VrMenu::Resume:CloseVrMenus(true);return;
    case VrMenu::Restart:RequestPortalRestart();return;
    case VrMenu::Recenter:ResetPosition();SetMenuPlacement();m_GraphicsStatus="Headset and menu recentered.";break;
    case VrMenu::ResetPlacement:SetMenuPlacement();m_GraphicsStatus="Panel placed in front of you.";break;
    case VrMenu::Benchmark:m_PerfStart=1;m_PerfPrevious=0;m_PerfElapsedMs=0;m_PerfIntervals.clear();m_PerfGpu.clear();m_GraphicsStatus="Sample armed: resume gameplay for 30 seconds.";break;
    case VrMenu::Edit:case VrMenu::ResetPage:case VrMenu::Preset: {
        const auto previous=m_Graphics;
        if(selected.action==VrMenu::Preset) {
            VrMenu::ApplyPreset(m_Graphics,selected.target);
        } else if(selected.action==VrMenu::Edit)VrMenu::Change(m_Graphics,VrMenu::Options()[selected.target],direction);
        else {const VrGraphics::Settings defaults;for(int i:VrMenu::On(m_MenuPage))VrMenu::Set(m_Graphics,VrMenu::Options()[i],VrMenu::Get(defaults,VrMenu::Options()[i]));}
        if(SaveGraphicsSettings()) {
            m_GraphicsApplyPending=true;
            const bool restart=selected.action==VrMenu::Preset||(selected.action==VrMenu::Edit&&VrMenu::Options()[selected.target].restart);
            m_GraphicsStatus=restart?"Saved. Restart Portal to apply this setting.":"Saved and applied.";
            if(previous.menuWidth!=m_Graphics.menuWidth||previous.menuDepth!=m_Graphics.menuDepth||previous.menuFollow!=m_Graphics.menuFollow)SetMenuPlacement(m_MenuLayoutFocused);
            if(m_PerfStart){m_PerfStart=1;m_PerfPrevious=0;m_PerfElapsedMs=0;m_PerfIntervals.clear();m_PerfGpu.clear();}
            PortalVrLog("VR settings change page=%d key=%s",int(m_MenuPage),selected.action==VrMenu::Edit?VrMenu::Options()[selected.target].key:selected.action==VrMenu::Preset?"visual-preset":"reset-page");
        } else {m_Graphics=previous;m_GraphicsStatus="Save failed. Previous settings kept.";}
        break;
    }
    default:break;
    }
    m_MenuHoveredRow=-1;m_GraphicsDirty=true;
    m_MenuNavNext=GetTickCount64()+220;
}
vr::EVROverlayError VR::UploadOverlayFile(vr::VROverlayHandle_t handle,const std::vector<uint32_t>& pixels,int width,int height,const char* name) {
    if(!g_D3DVR9||width<1||height<1||pixels.size()!=size_t(width)*height)return vr::VROverlayError_InvalidParameter;
    const auto result=g_D3DVR9->UploadOverlayPixels(handle,pixels.data(),width,height);
    static unsigned audits=0;
    if(audits++<12||FAILED(result))PortalVrLog("GPU overlay raster name=%s size=%dx%d result=%08x",name,width,height,unsigned(result));
    return SUCCEEDED(result)?vr::VROverlayError_None:vr::VROverlayError_RequestFailed;
}
void VR::OverlayTextureSubmitted(vr::VROverlayHandle_t handle,vr::EVROverlayError result) {
    if(result!=vr::VROverlayError_None) {
        PortalVrLog("GPU overlay submission handle=%llu error=%d",handle,result);
        if(handle==m_GraphicsStagingHandle&&m_MenuRaster.pending){m_MenuRaster.Failed(GetTickCount64());m_GraphicsDirty=true;}
        return;
    }
    // Vulkan SetOverlayTexture completes the upload request synchronously.
    // Commit only the staged image, preserving the front on failure. Unlike
    // SetOverlayRaw, this path must not wait for an ImageLoaded file event.
    if(handle==m_GraphicsStagingHandle&&m_MenuRaster.pending) {
        if(!m_GraphicsOpen) {
            std::swap(m_GraphicsHandle,m_GraphicsStagingHandle);m_MenuRaster.Loaded();m_MenuImageFailures=0;
            return; // a pending upload must never reopen a menu the user closed
        }
        const auto shown=m_Overlay->ShowOverlay(m_GraphicsStagingHandle);
        if(shown!=vr::VROverlayError_None){m_MenuRaster.Failed(GetTickCount64());m_GraphicsDirty=true;return;}
        m_Overlay->HideOverlay(m_GraphicsHandle);std::swap(m_GraphicsHandle,m_GraphicsStagingHandle);m_MenuRaster.Loaded();m_MenuImageFailures=0;
        if(!m_MenuUploadLogged){PortalVrLog("GPU settings panel front ready page=%d",m_MenuRaster.shownPage);m_MenuUploadLogged=true;}
    }
}
vr::EVROverlayError VR::UploadMenuImageFromFile() {
    const auto name="menu-panel-"+std::to_string(1-m_MenuRaster.front)+".tga";
    const auto result=UploadOverlayFile(m_GraphicsStagingHandle,m_GraphicsPixels,VrMenu::Width,VrMenu::Height,name.c_str());
    if(result==vr::VROverlayError_None)m_MenuFileTransport=true;
    if(!m_MenuUploadLogged||(result!=vr::VROverlayError_None&&m_MenuImageFailures%20==0))PortalVrLog("VR settings file transport image=%dx%d result=%d",VrMenu::Width,VrMenu::Height,result);
    return result;
}
void VR::UpdateGraphicsMenu() {
    const bool capturing=m_BodyCapture.active;
    if(capturing)UpdateBodyCalibration();
    if(!capturing||!MenuDigital(m_Input,m_MenuTriggers[0])||!MenuDigital(m_Input,m_MenuTriggers[1]))UpdateMenuInteraction();
    UpdateHolsterReachTest();if(!m_GraphicsOpen||!m_Overlay)return;
    const auto now=GetTickCount64();vr::VREvent_t event{};
    while(m_Overlay->PollNextOverlayEvent(m_GraphicsStagingHandle,&event,sizeof(event))) {
        if(!m_MenuRaster.pending)continue;
        if(event.eventType==vr::VREvent_ImageLoaded) {
            const auto error=m_Overlay->ShowOverlay(m_GraphicsStagingHandle);
            if(error!=vr::VROverlayError_None){PortalVrLog("VR settings swap show error=%d",error);m_MenuRaster.Failed(now);m_GraphicsDirty=true;++m_MenuImageFailures;continue;}
            m_Overlay->HideOverlay(m_GraphicsHandle);
            std::swap(m_GraphicsHandle,m_GraphicsStagingHandle);m_MenuRaster.Loaded();m_MenuImageFailures=0;
            if(!m_MenuUploadLogged){PortalVrLog("VR settings stable front ready page=%d halfRaster=%d",m_MenuRaster.shownPage,m_MenuHalfRaster);m_MenuUploadLogged=true;}
            break;
        }
        if(event.eventType==vr::VREvent_ImageFailed) {
            PortalVrLog("VR settings back image failed halfRaster=%d; front preserved",m_MenuHalfRaster);
            m_MenuRaster.Failed(now);m_GraphicsDirty=true;m_MenuFileTransport=true;++m_MenuImageFailures;
        }
    }
    if(m_MenuRaster.pending&&now-m_MenuRaster.started>2000) {
        PortalVrLog("VR settings back upload timed out; front preserved");m_MenuRaster.Failed(now);
        m_GraphicsDirty=true;m_MenuFileTransport=true;++m_MenuImageFailures;
    }
    if(m_GraphicsDirty&&m_MenuRaster.Begin(now,int(m_MenuPage))) {
        const auto& image=m_VKLeftEye.m_VulkanData;
        const unsigned width=image.m_nWidth?image.m_nWidth:m_RenderWidth,height=image.m_nHeight?image.m_nHeight:m_RenderHeight;
        const auto& crop=m_TextureBounds[0];
        const std::string active="Scene "+std::to_string(width)+" x "+std::to_string(height)+"  |  View "+std::to_string(RenderPolicy::Visible(width,crop.uMin,crop.uMax))+" x "+std::to_string(RenderPolicy::Visible(height,crop.vMin,crop.vMax))+"  |  MSAA "+std::to_string(m_ActiveMsaa)+"x";
        std::string output;
        if(m_MenuPage==VrMenu::Visuals||m_MenuPage==VrMenu::Upscaling) {
            const auto& target=m_UpscaleReady?m_VKUpscaledLeft.m_VulkanData:image;
            output="Output "+std::to_string(target.m_nWidth)+" x "+std::to_string(target.m_nHeight)+(m_UpscaleReady?"  |  Mode "+std::to_string(m_Graphics.upscaler):"  |  Native")+(m_ActiveLegacyEffects?"  |  Legacy effects":"  |  Direct eye render");
        } else if(m_MenuPage==VrMenu::Effects)output="Bloom / motion blur require legacy effects. Legacy can soften VR.";
        else if(m_MenuPage==VrMenu::Gun)output="Transfer: hold both grips, then release the gun hand. Movement stays put.";
        else if(m_MenuPage==VrMenu::Support)output="Squeeze anywhere under the gun. Pull away to release after a handoff.";
        else if(m_MenuPage==VrMenu::Holster)output="Bring gun hand over a shoulder and squeeze. Squeeze there again to draw.";
        else if(m_MenuPage==VrMenu::Feedback)output="Use left/right test buttons. Strength 0 silences all weapon pulses.";
        else if(m_MenuPage==VrMenu::Arms)output="Hand visibility and placement. Native body and IK has optional body solving.";
        else if(m_MenuPage==VrMenu::TPose)output="Stand upright, arms sideways. Release triggers; hold BOTH for 1 second.";
        else if(m_MenuPage==VrMenu::Measurements)output="Head + hands measure reach/drop. Shoulder width is an adjustable estimate.";
        else if(m_MenuPage==VrMenu::BodyShape)output="Cuff inflation thickens the wrist/forearm without widening the fingers.";
        else if(m_MenuPage==VrMenu::HandFit)output="Palm mode anchors the grip inside the hand, not the wrist joint.";
        else if(m_MenuPage==VrMenu::BackGun)output="Back model follows the stored shoulder slot. Position and angle apply live.";
        else if(m_MenuPage==VrMenu::HolsterZones)output="Radius is shared size times each side's scale. Reach test pulses in pause.";
        else if(m_MenuPage==VrMenu::LeftHolster||m_MenuPage==VrMenu::RightHolster)output="Hold matching hand at desired zone; select Set with the OTHER pointer.";
        else if(m_MenuPage==VrMenu::GrabPoint)output="Touch desired grip with free hand; use gun hand pointer to select Set.";
        else if(m_MenuPage==VrMenu::BodyTuning)output="Rigid torso requires fitted body. Legs adapt; torso length stays fixed during jumps.";
        else if(m_MenuPage==VrMenu::Interactions)output="Optional beta features. Native carry and world collision remain available.";
        else if(m_MenuPage==VrMenu::WorldHands)output="Visual palms stop at surfaces. Squeeze overrides pose. Gun hands stay tracked.";
        else if(m_MenuPage==VrMenu::PropGrab)output="Stow gun, reach near a prop, then use the usual grab. Native collisions stay active.";
        else if(m_MenuPage==VrMenu::Throwing)output="Move then release grab. Physics supplies gravity; pause never throws a prop.";
        else if(m_MenuPage==VrMenu::Body)output="Native Chell skin, complete arms/hands. Head + controllers; legs inferred.";
        m_GraphicsPixels=VrMenu::Paint(m_Graphics,m_MenuPage,m_GraphicsSelected,active,m_GraphicsStatus,output);
        if(m_GraphicsPixels.empty()){PortalVrLog("VR settings paint failed: empty raster");m_MenuRaster.Failed();return;}
        // The Frame client can retain every SetOverlayRaw shared-memory block.
        // Never fall back to raw for a changing image: exhaustion is process-wide.
        auto error=UploadMenuImageFromFile();
        if(error!=vr::VROverlayError_None)++m_MenuImageFailures;
        if(error!=vr::VROverlayError_None) {
            if(m_MenuImageFailures<=3||m_MenuImageFailures%20==0)PortalVrLog("VR settings upload rejected error=%d; retrying silently; front preserved",error);
            m_MenuRaster.Failed(now);m_GraphicsDirty=true;
        } else m_GraphicsDirty=false;
    }
    if(m_MenuRaster.ready)m_Overlay->ShowOverlay(m_GraphicsHandle);
}
bool VR::RequestPortalRestart() {
    if(m_RestartRequested)return false;
    char directory[MAX_PATH]{};if(!GetRuntimeBaseDirectory(directory,sizeof(directory)))return false;
    const auto root=std::filesystem::path(directory)/"VR";
    long long expires=0;std::ifstream alive(root/"restart-helper-alive.txt");alive>>expires;
    const auto now=std::time(nullptr);
    if(!alive||expires<now||expires>now+10) {
        m_GraphicsStatus="Restart unavailable. Save and relaunch with the Portal launcher.";m_GraphicsDirty=true;return false;
    }
    if(!SaveGraphicsSettings()){m_GraphicsStatus="Save failed. Portal has been left running.";m_GraphicsDirty=true;return false;}
    const auto temporary=root/"restart-request.tmp",request=root/"restart-request.json";
    std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
    output<<"{\"version\":1,\"operation\":\"restart_portal\",\"created_at\":"<<now<<",\"nonce\":\""<<GetCurrentProcessId()<<"-"<<GetTickCount64()<<"\"}\n";
    output.close();
    if(!output||!MoveFileExW(temporary.c_str(),request.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        m_GraphicsStatus="Could not request restart. Portal has been left running.";m_GraphicsDirty=true;return false;
    }
    m_RestartRequested=true;PortalVrLog("User requested Portal restart with saved settings");
    m_Game->ClientCmd_Unrestricted("quit");return true;
}
void VR::MenuHaptic(int hand,bool click) {
    if(!m_Graphics.menuHaptics||hand<0||hand>1)return;const auto now=GetTickCount64();
    if(now<m_WeaponHapticUntil[hand])return; // never truncate an explicit test or grab pulse
    if(!click&&now-m_MenuLastHaptic<70)return;m_MenuLastHaptic=now;
    const float amplitude=m_Graphics.hapticStrength*(click?1:.45f);
    if(!m_MenuVibration[hand]||m_Input->TriggerHapticVibrationAction(m_MenuVibration[hand],0,click?.025f:.01f,120,amplitude,vr::k_ulInvalidInputValueHandle)!=vr::VRInputError_None) {
        const auto index=m_System->GetTrackedDeviceIndexForControllerRole(hand?vr::TrackedControllerRole_RightHand:vr::TrackedControllerRole_LeftHand);
        if(index<vr::k_unMaxTrackedDeviceCount)m_System->TriggerHapticPulse(index,0,static_cast<unsigned short>(std::clamp(amplitude*2500,0.f,3999.f)));
    }
}
Vector VR::NativeHandPosition(bool left) {
    return NativeFit::Offset(left?GetLeftControllerAbsPos():GetRightControllerAbsPos(),
        left?m_LeftHandForward:m_RightHandForward,left?m_LeftControllerRight:m_RightControllerRight,
        left?m_LeftHandUp:m_RightHandUp,left?m_Graphics.leftX:m_Graphics.rightX,
        left?m_Graphics.leftY:m_Graphics.rightY,left?m_Graphics.leftZ:m_Graphics.rightZ,m_VRScale);
}
matrix3x4_t VR::GetHandFrame(bool left) {
    if(!m_NativeModels)return HandPose::ControllerHandFrame(left?m_LeftHandForward:m_RightHandForward,
        left?m_LeftControllerRight:m_RightControllerRight,left?m_LeftHandUp:m_RightHandUp,left?GetLeftHandAbsPos():GetRightHandAbsPos(),left);
    return NativeFit::Arm(left?m_LeftHandForward:m_RightHandForward,left?m_LeftControllerRight:m_RightControllerRight,
        left?m_LeftHandUp:m_RightHandUp,NativeHandPosition(left),left,
        left?m_Graphics.leftPitch:m_Graphics.rightPitch,left?m_Graphics.leftYaw:m_Graphics.rightYaw,left?m_Graphics.leftRoll:m_Graphics.rightRoll);
}
void VR::ControllerHaptic(int physical,float amplitude,float duration,float frequency,const char* reason) {
    if(!m_SystemFocus.Allowed()||physical<0||physical>1||!m_Input)return;
    amplitude=std::clamp(amplitude,0.f,1.f);duration=std::clamp(duration,.01f,.3f);frequency=std::clamp(frequency,40.f,320.f);
    if(!std::isfinite(amplitude)||!std::isfinite(duration)||!std::isfinite(frequency)||amplitude<=0)return;
    const auto started=GetTickCount64();
    if(started<m_WeaponHapticUntil[physical]&&amplitude<m_WeaponHapticAmplitude[physical]&&std::strcmp(reason,"user-test"))return;
    m_WeaponHapticUntil[physical]=started+uint64_t(duration*1000);m_WeaponHapticAmplitude[physical]=amplitude;
    vr::InputBindingInfo_t binding[4]{};uint32_t count=0;
    const auto bindingResult=m_Input->GetActionBindingInfo(m_MenuVibration[physical],binding,sizeof(binding[0]),4,&count);
    binding[0].rchDevicePathName[127]=0;binding[0].rchInputPathName[127]=0;
    const auto result=m_MenuVibration[physical]?m_Input->TriggerHapticVibrationAction(m_MenuVibration[physical],0,duration,frequency,amplitude,m_PhysicalInputSource[physical]):vr::VRInputError_InvalidHandle;
    const bool fallback=result!=vr::VRInputError_None||(bindingResult==vr::VRInputError_None&&count==0);
    if(fallback) {
        const auto now=GetTickCount64();m_LegacyHapticUntil[physical]=now+uint64_t(duration*1000);
        m_LegacyHapticNext[physical]=now;m_LegacyHapticAmplitude[physical]=amplitude;PumpLegacyHaptics();
    }
    static unsigned pulses[2]{};
    if(pulses[physical]++<16||!std::strcmp(reason,"user-test")||result!=vr::VRInputError_None)
        PortalVrLog("Controller haptic reason=%s physical=%d action=%llu source=%llu amplitude=%f duration=%f frequency=%f result=%d bindings=%u bindingResult=%d firstDevice=%s firstPath=%s legacy=%d",reason,physical,m_MenuVibration[physical],m_PhysicalInputSource[physical],amplitude,duration,frequency,int(result),count,int(bindingResult),binding[0].rchDevicePathName,binding[0].rchInputPathName,fallback);
}
void VR::PumpLegacyHaptics() {
    if(!m_SystemFocus.Allowed()){m_LegacyHapticUntil[0]=m_LegacyHapticUntil[1]=0;return;}
    const auto now=GetTickCount64();
    for(int physical=0;physical<2;++physical)if(now<m_LegacyHapticUntil[physical]&&now>=m_LegacyHapticNext[physical]) {
        const auto index=m_System->GetTrackedDeviceIndexForControllerRole(physical?vr::TrackedControllerRole_RightHand:vr::TrackedControllerRole_LeftHand);
        if(index<vr::k_unMaxTrackedDeviceCount)m_System->TriggerHapticPulse(index,0,static_cast<unsigned short>(std::clamp(m_LegacyHapticAmplitude[physical]*3999,0.f,3999.f)));
        m_LegacyHapticNext[physical]=now+10;
    }
}
void VR::WeaponHaptic(int logicalHand,float strength,bool enabled) {
    if(!enabled||logicalHand<0||logicalHand>1)return;
    ControllerHaptic(m_LeftHanded?1-logicalHand:logicalHand,strength*m_Graphics.grabHapticStrength,
        m_Graphics.hapticDurationMS/1000,m_Graphics.hapticFrequency,"weapon");
}
Vector VR::WeaponPosition(bool support) {
    const int hand=support?1-m_Weapon.hand:m_Weapon.hand;
    return hand==0?GetLeftHandAbsPos():GetRightHandAbsPos();
}
Vector VR::WeaponForward(){return m_Weapon.hand==0?m_LeftControllerForward:m_RightControllerForward;}
Vector VR::WeaponUp(){return m_Weapon.hand==0?m_LeftControllerUp:m_RightControllerUp;}
matrix3x4_t VR::GetPhysicalHandFrame(bool physicalLeft) {
    const bool logicalLeft=physicalLeft!=m_LeftHanded;const auto& s=m_Graphics;
    return NativeFit::Arm(logicalLeft?m_LeftHandForward:m_RightHandForward,logicalLeft?m_LeftControllerRight:m_RightControllerRight,
        logicalLeft?m_LeftHandUp:m_RightHandUp,NativeHandPosition(logicalLeft),physicalLeft,
        logicalLeft?s.leftPitch:s.rightPitch,logicalLeft?s.leftYaw:s.rightYaw,logicalLeft?s.leftRoll:s.rightRoll);
}
matrix3x4_t VR::GetAvatarHandFrame(bool physicalLeft,bool contact) {
    auto result=GetPhysicalHandFrame(physicalLeft);
    const bool primaryLeft=(WeaponHand()==0)!=m_LeftHanded;
    const auto& g=m_Graphics;
    if(m_OptionalSupportActive&&physicalLeft!=primaryLeft) {
        if(g.snapSupportHand) {
            const auto socket=HandPose::Concat(GetGunControllerFrame(),m_SupportFromController);
            for(int r=0;r<3;++r)result[r][3]=socket[r][3];
        }
        const auto p=NativeFit::Offset(TwoHandAim::Position(result),TwoHandAim::Axis(result,0),
            physicalLeft?-TwoHandAim::Axis(result,1):TwoHandAim::Axis(result,1),{0,0,1},g.supportHandXCM,g.supportHandYCM,g.supportHandZCM,m_VRScale);
        result=NativeFit::Rotate(result,g.supportHandPitch,g.supportHandYaw,g.supportHandRoll);
        for(int r=0;r<3;++r)result[r][3]=p[r];
    } else if(!g.controllerHands&&!m_BodyCapture.active&&physicalLeft==primaryLeft&&g.gripHand&&!m_Weapon.stowed&&m_PortalAimLastSeen) {
        const auto frame=GetGunControllerFrame();result=HandPose::Concat(frame,m_GunWristFromController);
        const auto uncalibrated=GunGrab::Apply(frame,{-g.grabXCM,-g.grabYCM,-g.grabZCM},m_VRScale);
        for(int r=0;r<3;++r)result[r][3]+=uncalibrated[r][3]-frame[r][3];
        if(physicalLeft)result=HandPose::Concat(result,HandPose::RotateAroundAxis({1,0,0},3.14159265358979323846f));
        const bool logicalLeft=WeaponHand()==0;
        const auto p=NativeFit::Offset(TwoHandAim::Position(result),TwoHandAim::Axis(frame,2),-TwoHandAim::Axis(frame,0),TwoHandAim::Axis(frame,1),
            logicalLeft?g.leftX:g.rightX,logicalLeft?g.leftY:g.rightY,logicalLeft?g.leftZ:g.rightZ,m_VRScale);
        result=NativeFit::Rotate(result,logicalLeft?g.leftPitch:g.rightPitch,logicalLeft?g.leftYaw:g.rightYaw,logicalLeft?g.leftRoll:g.rightRoll);
        for(int r=0;r<3;++r)result[r][3]=p[r];
    }
    const auto& touch=m_HandContact[physicalLeft?0:1];
    if(contact&&g.handContact&&touch.valid&&touch.touching&&GetTickCount64()-touch.time<150&&InteractionAllowed())return touch.frame;
    return result;
}
matrix3x4_t VR::GetCanonicalGunFrame() {
    const bool left=m_Weapon.hand==0;
    const Vector forward=WeaponForward(),right=left?m_LeftControllerRight:m_RightControllerRight,up=WeaponUp();
    const Vector base=(left?GetLeftControllerAbsPos():GetRightControllerAbsPos())-forward*2.5f-right;
    const Vector position=NativeFit::Offset(base,forward,right,up,m_Graphics.gunX,m_Graphics.gunY,m_Graphics.gunZ,m_VRScale);
    return GunGrab::Apply(NativeFit::Gun(HandPose::Frame(-right,up,forward,position),m_Graphics.gunPitch,m_Graphics.gunYaw,m_Graphics.gunRoll),
        {m_Graphics.grabXCM,m_Graphics.grabYCM,m_Graphics.grabZCM},m_VRScale);
}
matrix3x4_t VR::GetRawGunControllerFrame(){
    return GetCanonicalGunFrame();
}
matrix3x4_t VR::GetGunControllerFrame() {
    const auto raw=GetRawGunControllerFrame();
    const bool enabled=m_Graphics.twoHandAim&&m_LeftHandGunGrip&&m_OptionalSupportActive&&m_SupportLastSeen
        &&m_PortalAimLastSeen&&m_SystemFocus.Allowed()&&!m_GraphicsOpen&&!m_Game->IsCursorVisible()&&!m_Weapon.stowed
        &&m_LeftControllerPose.isValid&&m_RightControllerPose.isValid;
    const Vector pivot=m_Weapon.hand==0?GetLeftControllerAbsPos():GetRightControllerAbsPos();
    return TwoHandAim::Solve(raw,pivot,WeaponPosition(true),m_SupportFromController,
        enabled,m_Graphics.twoHandPercent/100.f,m_VRScale*.06f);
}
#include "vrcalibration_impl.h"
#include "vrmenuinteraction_impl.h"
