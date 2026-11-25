#include "vr.h"
#include <Windows.h>
#include "sdk.h"
#include "game.h"
#include "hooks.h"
#include "trace.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <filesystem>
#include <thread>
#include <type_traits>
#include <algorithm>
#include <cctype>
#include <cstring>
#include "../dxvk/src/d3d9/d3d9_vr.h"
#include "debuglog.h"
#include "cameracollision.h"
#include "portalpose.h"
#include "optionalgungrip.h"
#include "portaltrace.h"
#include "gunray.h"
#include "pickuptrace.h"
#include "vrsettings.h"
#include "stereoprojection.h"
#include "graphicsmenupaint.h"
#include "vrmenupaint.h"
#include "nativefit.h"
#include "twohandaim.h"
#include "vrexternal.h"

namespace
{
    constexpr bool kEnableVrMenuSubmission = true;
    constexpr bool kEnableVrMenuInput = true;
    constexpr bool kEnableConfigWatcher = false;
    constexpr bool kEnableMaterialSystemEyeTargets = true;

    bool GetRuntimeBaseDirectory(char *buffer, size_t bufferSize)
    {
        if (!buffer || bufferSize == 0)
            return false;

        HMODULE module = GetModuleHandleA("d3d9.dll");
        if (!module)
            return false;

        const DWORD length = GetModuleFileNameA(module, buffer, static_cast<DWORD>(bufferSize));
        if (length == 0 || length >= bufferSize)
            return false;

        char *lastSlash = strrchr(buffer, '\\');
        if (!lastSlash)
            return false;

        *lastSlash = '\0';
        return true;
    }

    void GetOverlayWindowSize(const VR &vr, int &width, int &height)
    {
        width = static_cast<int>(vr.m_VKBackBuffer.m_VulkanData.m_nWidth);
        height = static_cast<int>(vr.m_VKBackBuffer.m_VulkanData.m_nHeight);

        if (width <= 0)
            width = static_cast<int>(vr.m_RenderWidth);
        if (height <= 0)
            height = static_cast<int>(vr.m_RenderHeight);
    }
}

VR::VR(Game *game) 
{
    m_Game = game;
    PortalVrLog("VR::VR start build=PCVR-10-HudRecovery");

    vr::HmdError error = vr::VRInitError_None;
    m_System = vr::VR_Init(&error, vr::VRApplication_Scene);

    if (error != vr::VRInitError_None) 
    {
        PortalVrLog("VR_Init failed error=%d (%s); retrying when SteamVR is ready", error, vr::VR_GetVRInitErrorAsEnglishDescription(error));
        return;
    }
    PortalVrLog("VR_Init succeeded");

    if (!vr::VRCompositor())
    {
        PortalVrLog("VRCompositor init failed");
        vr::VR_Shutdown();
        return;
    }
    PortalVrLog("VRCompositor ready");

    m_Input = vr::VRInput();
    m_System = vr::OpenVRInternal_ModuleContext().VRSystem();
    if (!m_Input || !m_System)
    {
        PortalVrLog("OpenVR system or input interface unavailable");
        vr::VR_Shutdown();
        return;
    }

    m_System->GetRecommendedRenderTargetSize(&m_RenderWidth, &m_RenderHeight);
    m_RecommendedWidth=m_RenderWidth;m_RecommendedHeight=m_RenderHeight;
    m_AntiAliasing = 0;
    PortalVrLog("Recommended render target %u x %u", m_RenderWidth, m_RenderHeight);

    float l_left = 0.0f, l_right = 0.0f, l_top = 0.0f, l_bottom = 0.0f;
    m_System->GetProjectionRaw(vr::EVREye::Eye_Left, &l_left, &l_right, &l_top, &l_bottom);

    float r_left = 0.0f, r_right = 0.0f, r_top = 0.0f, r_bottom = 0.0f;
    m_System->GetProjectionRaw(vr::EVREye::Eye_Right, &r_left, &r_right, &r_top, &r_bottom);

    const StereoProjection::Frustum raw[2]={{l_left,l_right,l_top,l_bottom},
        {r_left,r_right,r_top,r_bottom}};
    StereoProjection::Plan projection;
    if (!StereoProjection::Build(raw,projection)) {
        PortalVrLog("VR initialization stopped: invalid optical projection");
        vr::VR_Shutdown();
        return;
    }
    m_Aspect=projection.aspect;
    m_Fov=projection.horizontalFov;
    for (int eye=0;eye<2;++eye) {
        const auto& bounds=projection.eyes[eye];
        m_TextureBounds[eye]={bounds.uMin,bounds.vMin,bounds.uMax,bounds.vMax};
        m_CropWidth=std::min(m_CropWidth,bounds.uMax-bounds.uMin);
        m_CropHeight=std::min(m_CropHeight,bounds.vMax-bounds.vMin);
        PortalVrLog("Stereo projection eye=%d raw=%f,%f,%f,%f bounds=%f,%f,%f,%f fov=%f aspect=%f",
            eye,raw[eye].left,raw[eye].right,raw[eye].top,raw[eye].bottom,
            bounds.uMin,bounds.vMin,bounds.uMax,bounds.vMax,m_Fov,m_Aspect);
    }

    InstallApplicationManifest("manifest.vrmanifest");
    if (SetActionManifest("action_manifest.json") != 0)
    {
        PortalVrLog("VR initialization stopped: action manifest unavailable");
        vr::VR_Shutdown();
        return;
    }
    ParseConfigFile();
    char shellDirectory[MAX_PATH]{};
    if(GetRuntimeBaseDirectory(shellDirectory,sizeof(shellDirectory))){
        const bool loaded=m_GunShell.Load(std::filesystem::path(shellDirectory)/"VR"/"gun-shell51.bin");
        PortalVrLog("Opaque gun mesh ready=%d parts=%zu triangles=%zu checksum=%d",loaded,m_GunShell.parts.size(),m_GunShell.triangles.size(),m_GunShell.checksum);
    }
    m_ActiveLegacyEffects=m_Graphics.legacyEffects;
    const auto target=VrGraphics::ExplicitTarget(m_Graphics.eyeWidth,m_CropWidth,m_CropHeight);
    m_RenderWidth=target.width;m_RenderHeight=target.height;m_ActiveMsaa=m_AntiAliasing;
    const auto output=VrGraphics::ExplicitTarget(m_Graphics.upscaleWidth,m_CropWidth,m_CropHeight);
    m_UpscaleWidth=std::max(output.width,m_RenderWidth);m_UpscaleHeight=std::max(output.height,m_RenderHeight);
    PortalVrLog("VR graphics active width=%u height=%u configuredWidth=%d msaa=%u mirror=%u nativeModels=%d legacyEffects=%d",
        m_RenderWidth,m_RenderHeight,m_Graphics.eyeWidth,m_AntiAliasing,m_RenderWindow,m_NativeModels,m_ActiveLegacyEffects);

    if (kEnableConfigWatcher)
    {
        std::thread configParser(&VR::WaitForConfigUpdate, this);
        configParser.detach();
        PortalVrLog("Config watcher started");
    }
    else
    {
        PortalVrLog("Config watcher disabled");
    }

    if (!g_D3DVR9)
    {
        PortalVrLog("VR::VR aborted: D3D9 VR interop not ready");
        vr::VR_Shutdown();
        return;
    }
    PortalVrLog("g_D3DVR9 ready");

    g_D3DVR9->GetBackBufferData(&m_VKBackBuffer);
    PortalVrLog(
        "Backbuffer ready width=%u height=%u",
        m_VKBackBuffer.m_VulkanData.m_nWidth,
        m_VKBackBuffer.m_VulkanData.m_nHeight);
    if (kEnableVrMenuSubmission || kEnableVrMenuInput)
    {
        m_Overlay = vr::VROverlay();
        if (m_Overlay)
        {
            m_Overlay->CreateOverlay("MenuOverlayKey", "MenuOverlay", &m_MainMenuHandle);
            m_Overlay->SetOverlayInputMethod(m_MainMenuHandle, vr::VROverlayInputMethod_None);
            m_Overlay->SetOverlayFlag(m_MainMenuHandle,vr::VROverlayFlags_IgnoreTextureAlpha,true);
            m_Overlay->SetOverlayAlpha(m_MainMenuHandle,1);
            m_Overlay->SetOverlayColor(m_MainMenuHandle,1,1,1);
            m_Overlay->SetOverlayFlag(m_MainMenuHandle, vr::VROverlayFlags_SendVRDiscreteScrollEvents, true);

            const vr::HmdVector2_t mouseScaleMenu = {
                static_cast<float>(m_RenderWidth),
                static_cast<float>(m_RenderHeight)
            };
            m_Overlay->SetOverlayCurvature(m_MainMenuHandle, 0.15f);
            m_Overlay->SetOverlayMouseScale(m_MainMenuHandle, &mouseScaleMenu);
        }
    }
    else
    {
        PortalVrLog("VR menu overlay initialization skipped");
    }

    UpdatePosesAndActions();
    GetPoses();
    ResetPosition();
    PortalVrLog("UpdatePosesAndActions complete");

    if (!kEnableMaterialSystemEyeTargets)
        PortalVrLog("Using backbuffer VR submit path; materialsystem eye targets disabled");

    m_IsInitialized = true;
    m_IsVREnabled = true;
    PortalVrLog("VR::VR complete");
}

int VR::SetActionManifest(const char *fileName) 
{
    char currentDir[MAX_STR_LEN];
    if (!GetRuntimeBaseDirectory(currentDir, ARRAYSIZE(currentDir)))
    {
        PortalVrLog("SetActionManifest failed: runtime base directory unavailable");
        Game::errorMsg("Failed to resolve runtime directory for action manifest");
        return 1;
    }

    char path[MAX_STR_LEN];
    sprintf_s(path, MAX_STR_LEN, "%s\\VR\\SteamVRActionManifest\\%s", currentDir, fileName);
    PortalVrLog("SetActionManifest path=%s", path);

    const auto manifestError=m_Input->SetActionManifestPath(path);
    PortalVrLog("SetActionManifestPath result=%d",int(manifestError));
    if (manifestError != vr::VRInputError_None)
    {
        char message[160];sprintf_s(message,"SteamVR controller setup failed (OpenVR input error %d). Details saved to portalvr.log.",int(manifestError));
        Game::errorMsg(message);
        return 1;
    }

    m_Input->GetActionHandle("/actions/main/in/ActivateVR", &m_ActionActivateVR);
    m_Input->GetActionHandle("/actions/main/in/Jump", &m_ActionJump);
    m_Input->GetActionHandle("/actions/main/in/PrimaryAttack", &m_ActionPrimaryAttack);
    m_Input->GetActionHandle("/actions/main/in/Reload", &m_ActionReload);
    m_Input->GetActionHandle("/actions/main/in/Use", &m_ActionUse);
    m_Input->GetActionHandle("/actions/main/in/Walk", &m_ActionWalk);
    m_Input->GetActionHandle("/actions/main/in/Turn", &m_ActionTurn);
    m_Input->GetActionHandle("/actions/main/in/SecondaryAttack", &m_ActionSecondaryAttack);
    m_Input->GetActionHandle("/actions/main/in/NextItem", &m_ActionNextItem);
    m_Input->GetActionHandle("/actions/main/in/PrevItem", &m_ActionPrevItem);
    m_Input->GetActionHandle("/actions/main/in/ResetPosition", &m_ActionResetPosition);
    m_Input->GetActionHandle("/actions/main/in/Crouch", &m_ActionCrouch);
    m_Input->GetActionHandle("/actions/main/in/Flashlight", &m_ActionFlashlight);
    m_Input->GetActionHandle("/actions/main/in/MenuSelect", &m_MenuSelect);
    m_Input->GetActionHandle("/actions/main/in/MenuBack", &m_MenuBack);
    m_Input->GetActionHandle("/actions/main/in/MenuUp", &m_MenuUp);
    m_Input->GetActionHandle("/actions/main/in/MenuDown", &m_MenuDown);
    m_Input->GetActionHandle("/actions/main/in/MenuLeft", &m_MenuLeft);
    m_Input->GetActionHandle("/actions/main/in/MenuRight", &m_MenuRight);
    m_Input->GetActionHandle("/actions/main/in/Spray", &m_Spray);
    m_Input->GetActionHandle("/actions/main/in/Scoreboard", &m_Scoreboard);
    m_Input->GetActionHandle("/actions/main/in/ShowHUD", &m_ShowHUD);
    m_Input->GetActionHandle("/actions/main/in/Pause", &m_Pause);
    m_Input->GetActionHandle("/actions/base/in/MenuEscape",&m_MenuEscape);
    m_Input->GetActionHandle("/actions/base/in/MenuToggle",&m_MenuToggle);
    for(int hand=0;hand<2;++hand) {
        const std::string suffix=hand ? "Right" : "Left";
        m_Input->GetActionHandle(("/actions/base/in/MenuTrigger"+suffix).c_str(),&m_MenuTriggers[hand]);
        m_Input->GetActionHandle(("/actions/base/in/MenuGrip"+suffix).c_str(),&m_MenuGrips[hand]);
        m_Input->GetActionHandle(("/actions/base/in/MenuStick"+suffix).c_str(),&m_MenuSticks[hand]);
        m_Input->GetActionHandle(("/actions/base/in/MenuStickClick"+suffix).c_str(),&m_MenuStickClicks[hand]);
        m_Input->GetActionHandle(hand ? "/actions/base/out/vibration_right" : "/actions/base/out/vibration_left",&m_MenuVibration[hand]);
        m_Input->GetInputSourceHandle(hand?"/user/hand/right":"/user/hand/left",&m_PhysicalInputSource[hand]);
    }
    for(int i=0;i<4;++i) {
        static const char* names[]={"MenuDpadUp","MenuDpadDown","MenuDpadLeft","MenuDpadRight"};
        m_Input->GetActionHandle((std::string("/actions/base/in/")+names[i]).c_str(),&m_MenuDpad[i]);
    }
	if (m_Input->GetActionHandle("/actions/base/in/skeleton_lefthand", &m_ActionSkeletonLeft) != vr::VRInputError_None)
		m_ActionSkeletonLeft = 0;
	if (m_Input->GetActionHandle("/actions/base/in/skeleton_righthand", &m_ActionSkeletonRight) != vr::VRInputError_None)
		m_ActionSkeletonRight = 0;
	if (m_Input->GetActionHandle("/actions/base/in/support_lefthand", &m_ActionSupportLeft) != vr::VRInputError_None)
		m_ActionSupportLeft = 0;
	if (m_Input->GetActionHandle("/actions/base/in/support_righthand", &m_ActionSupportRight) != vr::VRInputError_None)
		m_ActionSupportRight = 0;

    m_Input->GetActionSetHandle("/actions/main", &m_ActionSet);
    m_Input->GetActionSetHandle("/actions/base", &m_BaseActionSet);
    m_Input->GetActionSetHandle("/actions/left_handed", &m_LeftActionSet);
    m_LeftActions.clear();
    for(const char* name : {"ActivateVR","Jump","PrimaryAttack","Reload","Use","Walk","Turn",
        "SecondaryAttack","NextItem","PrevItem","ResetPosition","Crouch","Flashlight",
        "MenuSelect","MenuBack","MenuUp","MenuDown","MenuLeft","MenuRight","Spray",
        "Scoreboard","ShowHUD","Pause"}) {
        vr::VRActionHandle_t normal=0,left=0;
        const std::string suffix=std::string("/in/")+name;
        if(m_Input->GetActionHandle(("/actions/main"+suffix).c_str(),&normal)==vr::VRInputError_None
            && m_Input->GetActionHandle(("/actions/left_handed"+suffix).c_str(),&left)==vr::VRInputError_None)
            m_LeftActions[normal]=left;
    }
    m_ActiveActionSet = {};
    m_ActiveActionSet.ulActionSet = m_ActionSet;

    return 0;
}

void VR::InstallApplicationManifest(const char *fileName)
{
    char currentDir[MAX_STR_LEN];
    if (!GetRuntimeBaseDirectory(currentDir, ARRAYSIZE(currentDir)))
    {
        PortalVrLog("InstallApplicationManifest failed: runtime base directory unavailable");
        return;
    }

    char path[MAX_STR_LEN];
    sprintf_s(path, MAX_STR_LEN, "%s\\VR\\%s", currentDir, fileName);
    PortalVrLog("InstallApplicationManifest path=%s", path);

    vr::VRApplications()->AddApplicationManifest(path);
}

void VR::SetScreenSizeOverride(bool bState) {
    bool isOverriding = m_Game->IsScreenSizeOverrideActive();

    if (bState && !isOverriding || !bState && isOverriding) {
        int iOldWidth, iOldHeight;
        if (!m_Game->GetScreenSize(iOldWidth, iOldHeight))
            return;

        m_Game->ForceScreenSizeOverride(bState, m_RenderWidth, m_RenderHeight);
       /*int x = 0, y = 0, w = m_RenderWidth, h = m_RenderHeight;

        if (m_Game->m_ClientMode->GetViewport())
            m_Game->m_ClientMode->AdjustEngineViewport(x, y, w, h);*/

        if (bState) {
            /*IMatRenderContext* renderContext = m_Game->m_MaterialSystem->GetRenderContext();
            renderContext->Viewport(0, 0, m_RenderWidth, m_RenderHeight);
            renderContext->Release();*/
        }
        m_Game->OnScreenSizeChanged(iOldWidth, iOldHeight);
    }
}

void VR::Update()
{
    if (!m_IsInitialized || !m_Game->m_Initialized)
        return;

    const bool hasGameplayInterfaces = m_Game->GetLocalPortalPlayer() != nullptr
        && m_Game->GetEngineTrace() != nullptr;
    const bool cursorVisible = m_Game->IsCursorVisible();
    const bool inGame = m_Game->IsInGame();
    static bool loggedUpdateEntry = false;
    static bool loggedAfterSubmit = false;
    static bool loggedAfterPoses = false;
    static bool loggedAfterTracking = false;
    static bool loggedSkippedMenuInput = false;
    static bool loggedAfterInput = false;
    static bool loggedSkippedGameplayInput = false;
    static bool wasInGame = false;

    if (!loggedUpdateEntry)
    {
        PortalVrLog("VR::Update first entry cursorVisible=%d inGame=%d gameplayInterfaces=%d", cursorVisible, inGame, hasGameplayInterfaces);
        loggedUpdateEntry = true;
    }

    if (m_IsVREnabled && g_D3DVR9)
    {
        //SetScreenSizeOverride(inGame);

        // Named Source targets survive map changes. Reopening allocation here
        // interrupts the material/lightmap restore during a save load. DXVK
        // refreshes the underlying eye surfaces when Source next binds them.
        if (!inGame)
        {
            m_CameraCollisionOffset = { 0, 0, 0 };
            m_CameraBlocked = false;
            m_Game->m_CachedArmsModel = false;
            if (wasInGame)
                PortalVrLog("Leaving gameplay: retaining named VR render targets");
            m_RenderedNewFrame = false;
            m_GrabPoseValid = false;
        } 

        if (kEnableMaterialSystemEyeTargets && !m_CreatedVRTextures)
        {
            PortalVrLog("VR::Update creating VR textures outside RenderView");
            CreateVRTextures();
        }
    }

    if(m_GraphicsApplyPending || (inGame && !wasInGame)) ApplyGraphicsSettings();
    wasInGame = inGame;
    SamplePerformance();

    RefreshSystemFocus(false);
    SubmitVRTextures();
    if (!loggedAfterSubmit)
    {
        PortalVrLog("VR::Update completed SubmitVRTextures");
        loggedAfterSubmit = true;
    }

    UpdatePosesAndActions();
    if (!loggedAfterPoses)
    {
        PortalVrLog("VR::Update completed UpdatePosesAndActions");
        loggedAfterPoses = true;
    }

    UpdateTracking();
    if (!loggedAfterTracking)
    {
        PortalVrLog("VR::Update completed UpdateTracking");
        loggedAfterTracking = true;
    }

    RefreshSystemFocus(true);
    if(!m_SystemFocus.Allowed())return;
    PumpLegacyHaptics();

    // A native menu transition always brings its companion settings panel.
    // Native Resume/load/escape must also dismiss the companion immediately.
    if(m_Game->IsCursorVisible()) {
        if(!m_MenuWasVisible&&GetTickCount64()>=m_MenuSuppressUntil)OpenGraphicsMenu(false);
        m_MenuCursorSeen=true;
    } else if(m_MenuCursorSeen) {
        CloseVrMenus(false);m_MenuWasVisible=false;m_MenuCursorSeen=false;
    }
    if(HandleMenuFallback()) return;
    UpdateComfortVignette();
    UpdateFpsOverlay();
    if(m_GraphicsOpen) { UpdateGraphicsMenu(); return; }
    if (m_Game->IsCursorVisible()) {
        if (!kEnableVrMenuInput)
        {
            if (!loggedSkippedMenuInput)
            {
                PortalVrLog("VR::Update skipped VR menu input");
                loggedSkippedMenuInput = true;
            }
            return;
        }

        ProcessMenuInput();
    } else if (hasGameplayInterfaces) {
        if(m_MenuWasVisible) {ReleaseMenuInput();m_MenuWasVisible=false;}
        if(m_MenuChrome)m_Overlay->HideOverlay(m_MenuChrome);
        for(int hand=0;hand<2;++hand) {if(m_MenuBeam[hand])m_Overlay->HideOverlay(m_MenuBeam[hand]);if(m_MenuDot[hand])m_Overlay->HideOverlay(m_MenuDot[hand]);}
        ProcessInput();
        if (!loggedAfterInput)
        {
            PortalVrLog("VR::Update completed ProcessInput");
            loggedAfterInput = true;
        }
    } else if (!loggedSkippedGameplayInput) {
        PortalVrLog("VR::Update skipped gameplay input because gameplay interfaces are unavailable");
        loggedSkippedGameplayInput = true;
    }
}

void VR::CreateVRTextures()
{
    if (m_CreatedVRTextures)
        return;

    IMaterialSystem *materialSystem = m_Game->GetMaterialSystem();
    if (!materialSystem)
    {
        PortalVrLog("CreateVRTextures skipped: material system unavailable");
        return;
    }

    PortalVrLog("CreateVRTextures start width=%u height=%u", m_RenderWidth, m_RenderHeight);

    materialSystem->BeginRenderTargetAllocation();

    m_CreatingTextureID = Texture_LeftEye;
    m_LeftEyeTexture = materialSystem->CreateNamedRenderTargetTextureEx("leftEye0", m_RenderWidth, m_RenderHeight, RT_SIZE_LITERAL, materialSystem->GetBackBufferFormat(), MATERIAL_RT_DEPTH_SEPARATE, TEXTUREFLAGS_NOMIP);
    
    m_CreatingTextureID = Texture_RightEye;
    m_RightEyeTexture = materialSystem->CreateNamedRenderTargetTextureEx("rightEye0", m_RenderWidth, m_RenderHeight, RT_SIZE_LITERAL, materialSystem->GetBackBufferFormat(), MATERIAL_RT_DEPTH_SEPARATE, TEXTUREFLAGS_NOMIP);
    m_CreatingTextureID = Texture_None;

    materialSystem->EndRenderTargetAllocation();

    m_CreatedVRTextures = m_LeftEyeTexture && m_RightEyeTexture
        && m_D9LeftEyeSurface && m_D9RightEyeSurface
        && m_VKLeftEye.m_VulkanData.m_nImage && m_VKRightEye.m_VulkanData.m_nImage;
    PortalVrLog(
        "CreateVRTextures complete this=%p left=%p right=%p created=%d",
        this,
        m_LeftEyeTexture,
        m_RightEyeTexture,
        m_CreatedVRTextures);
}

void VR::SubmitVRTextures()
{
    if (!g_D3DVR9)return;
    const bool desktopReady=SUCCEEDED(g_D3DVR9->GetBackBufferData(&m_VKBackBuffer));
    auto *compositor = vr::VRCompositor();
    if (!compositor)
        return;

    const bool eyeFrame = m_RenderedNewFrame && m_CreatedVRTextures;
    const bool canRender=compositor->CanRenderScene();
    const bool backgroundReady=canRender&&!eyeFrame&&SUCCEEDED(g_D3DVR9->GetMenuBackgroundData(&m_VKBlankTexture));
    const auto source=VrFocus::SelectSubmission(canRender,eyeFrame,backgroundReady);
    if(source==VrFocus::Submission::None){HideOwnedOverlays();m_RenderedNewFrame=false;return;}
    VrExternal::Guard<IDirect3DVR9,true> external(g_D3DVR9);
    if(!external){PortalVrLog("External texture queue acquisition failed; frame skipped");m_RenderedNewFrame=false;return;}
    if (m_Overlay)
    {
        g_D3DVR9->SubmitOverlayTextures();
        if (desktopReady && m_SystemFocus.focused && (!eyeFrame || m_Game->IsCursorVisible() || m_GraphicsOpen) && kEnableVrMenuSubmission)
        {
            if (!m_Overlay->IsOverlayVisible(m_MainMenuHandle) && !m_GraphicsOpen)
                RepositionOverlays();
            vr::VRTextureBounds_t bounds{0, 0, 1, 1};
            const vr::HmdVector2_t mouseScale = {
                static_cast<float>(m_VKBackBuffer.m_VulkanData.m_nWidth),
                static_cast<float>(m_VKBackBuffer.m_VulkanData.m_nHeight) };
            m_Overlay->SetOverlayMouseScale(m_MainMenuHandle, &mouseScale);
            m_Overlay->SetOverlayTexelAspect(m_MainMenuHandle, 1.0f);
            m_Overlay->SetOverlayTextureBounds(m_MainMenuHandle, &bounds);
            auto error = m_Overlay->SetOverlayTexture(m_MainMenuHandle, &m_VKBackBuffer.m_VRTexture);
            static int lastOverlayError = -1;
            if (lastOverlayError != error)
            {
                PortalVrLog("Menu texture submission result=%d", error);
                lastOverlayError = error;
            }
            m_Overlay->ShowOverlay(m_MainMenuHandle);
        }
        else
            m_Overlay->HideOverlay(m_MainMenuHandle);
    }

    const bool upscaled=m_UpscaleReady && m_Graphics.upscaler!=0;
    auto left = compositor->Submit(vr::Eye_Left,
        eyeFrame ? (upscaled ? &m_VKUpscaledLeft.m_VRTexture : &m_VKLeftEye.m_VRTexture) : &m_VKBlankTexture.m_VRTexture,
        eyeFrame ? &m_TextureBounds[0] : nullptr);
    auto right = compositor->Submit(vr::Eye_Right,
        eyeFrame ? (upscaled ? &m_VKUpscaledRight.m_VRTexture : &m_VKRightEye.m_VRTexture) : &m_VKBlankTexture.m_VRTexture,
        eyeFrame ? &m_TextureBounds[1] : nullptr);
    static int lastLeft = -1, lastRight = -1;
    static unsigned frames = 0;
    static bool lastEyeFrame = false;
    if ((++frames % 600) == 0 || lastEyeFrame != eyeFrame || lastLeft != left || lastRight != right)
    {
        PortalVrLog("VR frame=%u eyes=%d submitLeft=%d submitRight=%d inGame=%d hmdTracked=%d cursor=%d", frames, eyeFrame, left, right,
            m_Game->IsInGame(), m_Poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid, m_Game->IsCursorVisible());
        const auto& submitted=eyeFrame?(upscaled?m_VKUpscaledLeft.m_VulkanData:m_VKLeftEye.m_VulkanData):m_VKBlankTexture.m_VulkanData;
        PortalVrLog("Scene submission source=%s size=%ux%u dashboardYield=%d",eyeFrame?(upscaled?"upscaled-eyes":"native-eyes"):"independent-black",submitted.m_nWidth,submitted.m_nHeight,!m_SystemFocus.focused);
        lastLeft = left;
        lastRight = right;
        lastEyeFrame = eyeFrame;
        vr::InputAnalogActionData_t walk{};
        auto inputError = m_Input->GetAnalogActionData(m_ActionWalk, &walk, sizeof(walk), vr::k_ulInvalidInputValueHandle);
        PortalVrLog("Tracking yaw=%f walkError=%d active=%d axes=%f,%f", m_HmdAngAbs.y, inputError, walk.bActive, walk.x, walk.y);
    }
    m_RenderedNewFrame = false;
}

void VR::GetPoseData(vr::TrackedDevicePose_t &poseRaw, TrackedDevicePoseData &poseOut)
{
    poseOut.isValid = poseRaw.bPoseIsValid && poseRaw.bDeviceIsConnected;
    for(int row=0;row<3 && poseOut.isValid;++row) {
        for(int col=0;col<4;++col)
            poseOut.isValid=poseOut.isValid && std::isfinite(poseRaw.mDeviceToAbsoluteTracking.m[row][col]);
        poseOut.isValid=poseOut.isValid && std::isfinite(poseRaw.vVelocity.v[row])
            && std::isfinite(poseRaw.vAngularVelocity.v[row]);
    }
    if (poseOut.isValid)
    {
        vr::HmdMatrix34_t mat = poseRaw.mDeviceToAbsoluteTracking;
        Vector pos;
        Vector vel;
        QAngle ang;
        QAngle angvel;
        pos.x = -mat.m[2][3];
        pos.y = -mat.m[0][3];
        pos.z = mat.m[1][3];
        ang.x = asinf(std::clamp(mat.m[1][2],-1.f,1.f)) * (180.0 / 3.141592654);
        ang.y = atan2f(mat.m[0][2], mat.m[2][2]) * (180.0 / 3.141592654);
        ang.z = atan2f(-mat.m[1][0], mat.m[1][1]) * (180.0 / 3.141592654);
        vel.x = -poseRaw.vVelocity.v[2];
        vel.y = -poseRaw.vVelocity.v[0];
        vel.z = poseRaw.vVelocity.v[1];
        angvel.x = -poseRaw.vAngularVelocity.v[2] * (180.0 / 3.141592654);
        angvel.y = -poseRaw.vAngularVelocity.v[0] * (180.0 / 3.141592654);
        angvel.z = poseRaw.vAngularVelocity.v[1] * (180.0 / 3.141592654);

        poseOut.TrackedDevicePos = pos;
        poseOut.TrackedDeviceVel = vel;
        poseOut.TrackedDeviceAng = ang;
        poseOut.TrackedDeviceAngVel = angvel;
    }
}

void VR::RepositionOverlays() { SetMenuPlacement(); }

void VR::GetPoses() 
{
    vr::TrackedDevicePose_t hmdPose = m_Poses[vr::k_unTrackedDeviceIndex_Hmd];

    vr::TrackedDeviceIndex_t leftControllerIndex = m_System->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
    vr::TrackedDeviceIndex_t rightControllerIndex = m_System->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);

    // Capture static grip-origin metadata once controllers wake, without
    // changing the accepted raw tracking/aim path. Useful if a physical fit
    // still differs from the anatomical palm anchor after user testing.
    static bool gripLogged[2]{};const vr::TrackedDeviceIndex_t physicalIds[]={leftControllerIndex,rightControllerIndex};
    for(int physical=0;physical<2;++physical)if(!gripLogged[physical]&&physicalIds[physical]<vr::k_unMaxTrackedDeviceCount&&m_PhysicalInputSource[physical]!=vr::k_ulInvalidInputValueHandle) {
        char model[vr::k_unMaxPropertyStringSize]{};vr::ETrackedPropertyError property=vr::TrackedProp_Success;
        m_System->GetStringTrackedDeviceProperty(physicalIds[physical],vr::Prop_RenderModelName_String,model,sizeof(model),&property);
        if(property!=vr::TrackedProp_Success)continue;
        vr::RenderModel_ControllerMode_State_t mode{};vr::RenderModel_ComponentState_t component{};
        auto* models=vr::VRRenderModels();
        const bool valid=models&&models->GetComponentStateForDevicePath(model,vr::k_pch_Controller_Component_HandGrip,m_PhysicalInputSource[physical],&mode,&component);
        const auto& m=component.mTrackingToComponentLocal.m;
        PortalVrLog("Controller static handgrip physical=%d model=%s valid=%d local=%f,%f,%f,%f;%f,%f,%f,%f;%f,%f,%f,%f",physical,model,valid,m[0][0],m[0][1],m[0][2],m[0][3],m[1][0],m[1][1],m[1][2],m[1][3],m[2][0],m[2][1],m[2][2],m[2][3]);
        gripLogged[physical]=true;
    }

    if (m_LeftHanded)
        std::swap(leftControllerIndex, rightControllerIndex);

    vr::TrackedDevicePose_t leftControllerPose{};
    vr::TrackedDevicePose_t rightControllerPose{};
    if (leftControllerIndex < vr::k_unMaxTrackedDeviceCount)
        leftControllerPose = m_Poses[leftControllerIndex];
    if (rightControllerIndex < vr::k_unMaxTrackedDeviceCount)
        rightControllerPose = m_Poses[rightControllerIndex];

    GetPoseData(hmdPose, m_HmdPose);
    GetPoseData(leftControllerPose, m_LeftControllerPose);
    GetPoseData(rightControllerPose, m_RightControllerPose);
}

void VR::UpdatePosesAndActions() 
{
    static unsigned int loggedPoses = 0;
    if (loggedPoses++ < 3)
        PortalVrLog("Pose update this=%p poses=%p compositor=%p input=%p", this, m_Poses, vr::VRCompositor(), m_Input);
    if(g_D3DVR9) {
        VrExternal::Guard<IDirect3DVR9,false> queue(g_D3DVR9);
        if(!queue){m_ActionStateFresh=false;return;}
        vr::VRCompositor()->WaitGetPoses(m_Poses,vr::k_unMaxTrackedDeviceCount,nullptr,0);
    } else vr::VRCompositor()->WaitGetPoses(m_Poses,vr::k_unMaxTrackedDeviceCount,nullptr,0);
    m_ActiveActionSet.ulActionSet=m_LeftHanded && m_LeftActionSet ? m_LeftActionSet : m_ActionSet;
    vr::VRActiveActionSet_t actionSets[2] = { m_ActiveActionSet, {} };
    actionSets[1].ulActionSet = m_BaseActionSet;
    m_ActionStateFresh=m_Input->UpdateActionState(actionSets, sizeof(vr::VRActiveActionSet_t), m_BaseActionSet ? 2 : 1)==vr::VRInputError_None;

	// Presence comes from the action state, never from curl magnitude: zero
	// is a valid fully open hand, including Pico's estimated skeleton stream.
	auto updateFingerSummary = [this](vr::VRActionHandle_t action, float *curl, bool &valid,int physicalHand) {
		vr::InputSkeletalActionData_t state{};
		vr::VRSkeletalSummaryData_t summary{};
        vr::EVRSkeletalTrackingLevel level=vr::VRSkeletalTracking_Estimated;
        const auto trackingError=action?m_Input->GetSkeletalTrackingLevel(action,&level):vr::VRInputError_InvalidHandle;
        const auto preferred=level==vr::VRSkeletalTracking_Estimated?vr::VRSummaryType_FromAnimation:vr::VRSummaryType_FromDevice;
        const auto alternate=preferred==vr::VRSummaryType_FromAnimation?vr::VRSummaryType_FromDevice:vr::VRSummaryType_FromAnimation;
        valid=action&&m_Input->GetSkeletalActionData(action,&state,sizeof(state))==vr::VRInputError_None&&state.bActive
            &&(m_Input->GetSkeletalSummaryData(action,preferred,&summary)==vr::VRInputError_None
                ||m_Input->GetSkeletalSummaryData(action,alternate,&summary)==vr::VRInputError_None);
		for (int i = 0; i < 5 && valid; ++i) valid = std::isfinite(summary.flFingerCurl[i]);
        static unsigned summaryFrames[2]{};
        if((summaryFrames[physicalHand]++%1200)==0) {
            PortalVrLog("Finger input action=%llu active=%d valid=%d trackingLevel=%d trackingError=%d curl=%f,%f,%f,%f,%f",action,state.bActive,valid,int(level),int(trackingError),summary.flFingerCurl[0],summary.flFingerCurl[1],summary.flFingerCurl[2],summary.flFingerCurl[3],summary.flFingerCurl[4]);
        }
		static const float relaxed[5] = {0.15f, 0.20f, 0.25f, 0.25f, 0.25f};
        vr::InputDigitalActionData_t trigger{},grip{};
        if(!valid) {
            m_Input->GetDigitalActionData(m_MenuTriggers[physicalHand],&trigger,sizeof(trigger),vr::k_ulInvalidInputValueHandle);
            m_Input->GetDigitalActionData(m_MenuGrips[physicalHand],&grip,sizeof(grip),vr::k_ulInvalidInputValueHandle);
        }
        for(int i=0;i<5;++i)curl[i]=valid?std::clamp(summary.flFingerCurl[i],0.f,1.f):
            i==1?(trigger.bActive&&trigger.bState?.9f:relaxed[i]):
            i>=2?(grip.bActive&&grip.bState?.9f:relaxed[i]):relaxed[i];
	};
	updateFingerSummary(m_LeftHanded ? m_ActionSkeletonRight : m_ActionSkeletonLeft, m_LeftFingerCurl, m_LeftSkeletonValid,m_LeftHanded?1:0);
	updateFingerSummary(m_LeftHanded ? m_ActionSkeletonLeft : m_ActionSkeletonRight, m_RightFingerCurl, m_RightSkeletonValid,m_LeftHanded?0:1);
    for(int hand=0;hand<2;++hand) {
        const int physical=m_LeftHanded?1-hand:hand;
        vr::InputDigitalActionData_t grip{};
        const auto action=physical?m_ActionSupportRight:m_ActionSupportLeft;
        const bool valid=action&&m_Input->GetDigitalActionData(action,&grip,sizeof(grip),vr::k_ulInvalidInputValueHandle)==vr::VRInputError_None&&grip.bActive;
        vr::VRControllerState_t state{};
        const auto index=m_System->GetTrackedDeviceIndexForControllerRole(physical?vr::TrackedControllerRole_RightHand:vr::TrackedControllerRole_LeftHand);
        const bool pressed=index<vr::k_unMaxTrackedDeviceCount&&m_System->GetControllerState(index,&state,sizeof(state))&&(state.ulButtonPressed&vr::ButtonMaskFromId(vr::k_EButton_Grip));
        const bool skeleton=hand==0?m_LeftSkeletonValid:m_RightSkeletonValid;
        const auto* curls=hand==0?m_LeftFingerCurl:m_RightFingerCurl;
        const bool raw=valid?grip.bState:OptionalGunGrip::Squeeze(pressed,skeleton,curls,m_GripPressed[hand]);
        const bool holdingWeapon=!m_Weapon.stowed&&m_PropHold.hand!=hand&&(hand==m_Weapon.hand||m_OptionalSupportActive);
        m_GripPressed[hand]=m_GunGripHold[hand].Resolve(raw,holdingWeapon&&m_Graphics.naturalGunHold,m_ActionStateFresh,skeleton,curls,m_GripPressed[hand],GetTickCount64(),m_Graphics.gunHoldCurl/100,m_Graphics.gunHoldGraceMS);
    }
    m_LeftGripPressed=m_GripPressed[0];

}

void VR::GetViewParameters() 
{
    vr::HmdMatrix34_t eyeToHeadLeft = m_System->GetEyeToHeadTransform(vr::Eye_Left);
    vr::HmdMatrix34_t eyeToHeadRight = m_System->GetEyeToHeadTransform(vr::Eye_Right);
    m_EyeToHeadTransformPosLeft.x = eyeToHeadLeft.m[0][3];
    m_EyeToHeadTransformPosLeft.y = eyeToHeadLeft.m[1][3];
    m_EyeToHeadTransformPosLeft.z = eyeToHeadLeft.m[2][3];

    m_EyeToHeadTransformPosRight.x = eyeToHeadRight.m[0][3];
    m_EyeToHeadTransformPosRight.y = eyeToHeadRight.m[1][3];
    m_EyeToHeadTransformPosRight.z = eyeToHeadRight.m[2][3];
}

bool VR::PressedDigitalAction(vr::VRActionHandle_t &actionHandle, bool checkIfActionChanged)
{
    if(!m_SystemFocus.Allowed() || GetTickCount64()<m_HandSwitchSuppressUntil) return false;
    const bool fire=actionHandle==m_ActionPrimaryAttack||actionHandle==m_ActionSecondaryAttack;
    if(fire&&(m_Weapon.stowed||m_Weapon.fireBlocked||PropBlocksGun()||!WeaponTracked()))return false;
    if(actionHandle==m_ActionUse) {
        if(HandControlledProps())return m_PropHold.use&&!m_PropHold.release;
        if(m_WeaponConsumesGrip||m_Weapon.blocked[m_Weapon.hand]||m_UseGripBlocked[m_Weapon.hand])return false;
        if(m_Weapon.hand==0)return m_GripPressed[0]&&!m_Weapon.blocked[0];
    }
    auto action=actionHandle;
    const int physical=TriggerColors::Physical(actionHandle==m_ActionPrimaryAttack,m_Graphics.swapPortalColors);
    if(fire)action=m_MenuTriggers[physical];
    vr::InputDigitalActionData_t digitalActionData{};
    vr::EVRInputError result = m_Input->GetDigitalActionData(fire?action:ResolveAction(action), &digitalActionData, sizeof(digitalActionData), vr::k_ulInvalidInputValueHandle);
    if(fire&&(result!=vr::VRInputError_None||!digitalActionData.bActive)) {
        const auto fallback=TriggerColors::LegacyPrimary(physical,m_LeftHanded&&m_LeftActionSet)?m_ActionPrimaryAttack:m_ActionSecondaryAttack;
        result=m_Input->GetDigitalActionData(ResolveAction(fallback),&digitalActionData,sizeof(digitalActionData),vr::k_ulInvalidInputValueHandle);
    }
    
    if (result == vr::VRInputError_None && digitalActionData.bActive)
    {
        if (checkIfActionChanged)
            return digitalActionData.bState && digitalActionData.bChanged;
        else
            return digitalActionData.bState;
    }

    return false;
}

bool VR::GetAnalogActionData(vr::VRActionHandle_t &actionHandle, vr::InputAnalogActionData_t &analogDataOut)
{
    analogDataOut={};
    if(!m_SystemFocus.Allowed() || GetTickCount64()<m_HandSwitchSuppressUntil) return false;
    vr::EVRInputError result = m_Input->GetAnalogActionData(ResolveAction(actionHandle), &analogDataOut, sizeof(analogDataOut), vr::k_ulInvalidInputValueHandle);

    if (result == vr::VRInputError_None && analogDataOut.bActive)
        return true;

    return false;
}

vr::VRActionHandle_t VR::ResolveAction(vr::VRActionHandle_t action) const {
    const auto found=m_LeftActions.find(action);
    return m_LeftHanded && m_LeftActionSet && found!=m_LeftActions.end() ? found->second : action;
}

bool VR::HandleSettingsCommand(const char* command) {
    const auto selection=VrSettings::Parse(command ? command : "");
    if(selection==VrSettings::Command::None) return false;
    if(!m_IsInitialized) return true;
    if(selection==VrSettings::Command::Graphics) { OpenGraphicsMenu(); return true; }
    if(selection==VrSettings::Command::Recenter) {
        ResetPosition();
        PortalVrLog("VR menu: manual recenter requested");
        return true;
    }
    const bool left=selection==VrSettings::Command::Left;
    if(left && (!m_LeftActionSet || m_LeftActions.size()!=23)) {
        Game::errorMsg("Left-handed bindings are unavailable. Reinstall the complete Portal1VR update.");
        return true;
    }
    char directory[MAX_PATH]{};
    if(!GetRuntimeBaseDirectory(directory,sizeof(directory))) return true;
    const auto config=std::filesystem::path(directory)/"VR"/"config.txt";
    const auto temporary=std::filesystem::path(directory)/"VR"/"config.txt.tmp";
    try {
        std::ifstream input(config,std::ios::binary);
        if(!input) throw std::runtime_error("cannot read config.txt");
        const std::string original((std::istreambuf_iterator<char>(input)),{});
        input.close();
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        output<<VrSettings::SetBool(original,"LeftHanded",left);
        output.close();
        if(!output || !MoveFileExW(temporary.c_str(),config.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("cannot save config.txt");
    } catch(const std::exception& error) {
        PortalVrLog("VR menu: hand preference save failed: %s",error.what());
        Game::errorMsg("Could not save the VR hand preference. Check that bin/VR/config.txt is writable.");
        return true;
    }
    if(left!=m_LeftHanded) {
        // Release old action state before changing pose roles. A menu click must
        // not become a shot or pickup when the new action set becomes active.
        m_Game->ClientCmd_Unrestricted("-attack;-attack2;-use;-jump;-duck;-reload");
        m_UseCommandHeld=false;
        m_LeftHanded=left;
        m_HandSwitchSuppressUntil=GetTickCount64()+500;
        m_GrabPoseValid=m_PickupAimValid=false;
        m_PortalAimLastSeen=m_SupportLastSeen=0;
        m_OptionalGripState={};m_OptionalSupportActive=m_LeftGripPressed=false;
        m_Weapon={};m_Weapon.Suspend();
        m_PressedTurn=false;
        m_CalibrationDrift.Reset();m_CalibrationStability.Reset();
        m_CalibrationSuppressUntil=GetTickCount64()+2000;
    }
    PortalVrLog("VR menu: %s-handed controls selected and saved",left ? "left" : "right");
    return true;
}

void VR::ProcessMenuInput() { UpdateMenuInteraction(); }

bool VR::UpdateOptionalGunSupport(matrix3x4_t *target)
{
    const int off=1-m_Weapon.hand;
    const auto controller=GetGunControllerFrame();
    const auto gun=HandPose::Concat(controller,m_GunFromController);
    const float unit=m_VRScale/100,scale=m_Graphics.gripZoneScale/100.f;
    GripZone::Shape shape;shape.length=m_Graphics.gripZoneLengthCM*unit*scale;
    shape.radius=m_Graphics.supportRadiusCM*unit*scale;
    shape.center={m_Graphics.gripZoneXCM*unit,-2.8f+m_Graphics.gripZoneYCM*unit,12+m_Graphics.gripZoneZCM*unit};
    auto hit=GripZone::Closest(gun,WeaponPosition(true),shape);
    const bool physicalLeft=(off==0)!=m_LeftHanded;
    const bool palm=m_Graphics.controllerHands&&m_Graphics.handAnchor==1;
    const auto rawHand=GetPhysicalHandFrame(physicalLeft);
    FingerSurface::Shape shell;shell.gun=true;shell.frame=gun;m_GunAnimation.Shape(shell);
    const auto palmPoint=TwoHandAim::Position(rawHand)+TwoHandAim::Axis(rawHand,0)*(palm?0:m_VRScale*.045f);
    const float captureRadius=std::min(shape.radius,m_Graphics.supportCaptureCM*unit);
    if(m_Graphics.firmSupportGrip){hit.distance=std::max(0.f,shell.Distance(palmPoint));hit.valid=std::isfinite(hit.distance);}
    const auto anchor=HandPose::Concat(controller,m_SupportFromController);
    const float distance=m_OptionalSupportActive?sqrtf((WeaponPosition(true)-TwoHandAim::Position(anchor)).LengthSqr()):hit.distance;
    const bool was=m_OptionalSupportActive;
    const bool enabled=m_LeftHandGunGrip&&m_IsVREnabled&&m_SystemFocus.Allowed()&&!m_GraphicsOpen&&!m_Game->IsCursorVisible()&&!m_Weapon.stowed&&m_PropHold.hand<0;
    const bool tracked=m_ActionStateFresh&&m_LeftControllerPose.isValid&&m_RightControllerPose.isValid
        &&(m_OptionalSupportActive||(WeaponPosition(true)-WeaponPosition()).LengthSqr()>=m_VRScale*m_VRScale*.0036f);
    const bool pressed=!m_Weapon.blocked[off]&&m_GripPressed[off];
    m_OptionalSupportActive=m_Graphics.firmSupportGrip?
        SupportGrip49::Update(m_OptionalGripState,enabled,tracked,m_SupportLastSeen&&hit.valid,pressed,
            hit.distance,captureRadius,distance,m_Graphics.supportReleaseCM*unit,m_Graphics.gripHoldNear):
        m_OptionalGripState.Update(enabled,tracked,m_SupportLastSeen&&hit.valid,pressed,distance,shape.radius,m_Graphics.gripHoldNear);
    if(m_Graphics.firmSupportGrip&&m_OptionalSupportActive&&!was){
        const auto hand=SupportGrip::Surface(rawHand,shell,m_VRScale,palm,m_Graphics.gunPalmGapMM);
        m_AnimatedSupport.Reset();
        if(m_SupportLatch.Capture(controller,hand)){
            const auto at=TwoHandAim::Position(hand)+TwoHandAim::Axis(hand,0)*(palm?0:m_VRScale*.045f);
            m_AnimatedSupport.pending=m_Graphics.animatedSupportGrip&&GunAnimation::FrontClosest(shell,at);
            if(m_AnimatedSupport.pending&&m_GunAnimation.valid)m_AnimatedSupport.Capture(HandPose::Concat(gun,m_GunAnimation.front),hand);
            // Raw controller socket controls aim; corrected surface pose controls
            // the visible hand. Matching them would jump the gun on acquisition.
            m_SupportFromController=HandPose::Concat(HandPose::InverseRigid(GetRawGunControllerFrame()),GripZone::Point(WeaponPosition(true)));
            PortalVrLog("Firm support captured hand=%d captureCM=%f releaseCM=%f front=%d pending=%d animation=%d",off,m_Graphics.supportCaptureCM,m_Graphics.supportReleaseCM,m_AnimatedSupport.bound,m_AnimatedSupport.pending,m_GunAnimation.valid);
        }else m_OptionalSupportActive=m_OptionalGripState.active=false;
    }
    if(!m_Graphics.firmSupportGrip&&m_OptionalSupportActive&&(!was||m_Graphics.slideSupportGrip)) {
        m_ActiveGripGunLocal=hit.point;
        m_SupportFromController=HandPose::Concat(m_GunFromController,GripZone::Point(hit.point));
    }
    if(!m_OptionalSupportActive){m_Weapon.transferSupport=false;m_SupportLatch.Reset();m_AnimatedSupport.Reset();}
    if(target&&m_OptionalSupportActive)*target=GetAvatarHandFrame((off==0)!=m_LeftHanded);
    if(m_GripHover[off].Update(enabled&&tracked&&m_SupportLastSeen&&hit.valid,hit.distance,m_Graphics.firmSupportGrip?captureRadius:shape.radius,GetTickCount64()))
        WeaponHaptic(off,.65f,m_Graphics.grabHover);
    if(was!=m_OptionalSupportActive) {
        PortalVrLog("Support grip active=%d hand=%d distance=%f radius=%f length=%f twoHand=%d enabled=%d tracked=%d fresh=%d pressed=%d blocked=%d",m_OptionalSupportActive,off,distance,shape.radius,shape.length,m_Graphics.twoHandAim,enabled,tracked,m_SupportLastSeen&&hit.valid,pressed,m_Weapon.blocked[off]);
        WeaponHaptic(off,m_OptionalSupportActive?1:.4f,m_Graphics.grabHaptics);
    }
    return m_OptionalSupportActive;
}

void VR::UpdateWeaponHandling() {
    bool tracked[2]={m_ActionStateFresh&&m_LeftControllerPose.isValid,m_ActionStateFresh&&m_RightControllerPose.isValid},shoulderNear[2]{};
    Vector forward=m_HmdForward;forward.z=0;if(!TwoHandAim::Unit(forward))forward={1,0,0};
    const Vector right=CrossProduct(forward,{0,0,1}),head=m_SetupOrigin+(m_6DOF?m_HmdPosRelative:Vector(0,0,0))+Vector(0,0,HeightOffset())+m_CameraCollisionOffset;
    const float unit=m_VRScale/100;
    for(int h=0;h<2;++h)for(int physical=0;physical<2;++physical) {
        const bool slot=m_Graphics.shoulderSlot==0||(m_Graphics.shoulderSlot==1&&physical==(m_Weapon.hand^(m_LeftHanded?1:0)))||m_Graphics.shoulderSlot==physical+2;
        const Vector shoulder=HolsterCenter(physical);
        const Vector hand=h==0?GetLeftControllerAbsPos():GetRightControllerAbsPos();
        shoulderNear[h]|=slot&&tracked[h]&&m_HmdPose.isValid&&(hand-shoulder).LengthSqr()<=powf(HolsterRadius(physical),2);
    }
    for(int h=0;h<2;++h) {
        if(m_Graphics.holster&&shoulderNear[h]&&!m_ShoulderHover[h])WeaponHaptic(h,.65f,m_Graphics.holsterHover);
        m_ShoulderHover[h]=shoulderNear[h];
    }
    bool gunGrip[2]={m_GripPressed[0],m_GripPressed[1]};
    if(m_PropHold.hand>=0)gunGrip[m_PropHold.hand]=false;
    const auto event=m_Weapon.Update(gunGrip,tracked,shoulderNear,m_OptionalSupportActive,m_Graphics.holster,m_Graphics.transfer);
    if(!m_Graphics.holster)m_Weapon.stowed=false;
    if(event==WeaponHandling::Stow) {
        const auto p=m_Weapon.hand==0?GetLeftControllerAbsPos():GetRightControllerAbsPos();
        float nearest=1e30f;
        for(int physical=0;physical<2;++physical) {
            const bool slot=m_Graphics.shoulderSlot==0||(m_Graphics.shoulderSlot==1&&physical==(m_Weapon.hand^(m_LeftHanded?1:0)))||m_Graphics.shoulderSlot==physical+2;
            const float distance=(p-HolsterCenter(physical)).LengthSqr();
            if(slot&&distance<nearest){nearest=distance;m_HolsteredShoulder=physical;}
        }
    }
    if(event==WeaponHandling::Transfer||event==WeaponHandling::Draw||event==WeaponHandling::Stow) {
        // Transfer adopts exactly the same receiver pose as shoulder draw.
        // A released former gun hand must not remain a virtual support anchor.
        m_Weapon.transferSupport=false;m_OptionalSupportActive=false;m_SupportLatch.Reset();m_AnimatedSupport.Reset();
        m_OptionalGripState.active=false;m_OptionalGripState.wasPressed=true;
        if(event==WeaponHandling::Transfer) {
            WeaponHaptic(0,.8f,m_Graphics.transferHaptics);WeaponHaptic(1,.8f,m_Graphics.transferHaptics);
        } else WeaponHaptic(m_Weapon.hand,1,m_Graphics.holsterHaptics);
    }
    if(event!=WeaponHandling::None) {
        m_ContactResetUntil=GetTickCount64()+250;m_GunWallRetraction=0;
        m_ThrowReleaseUntil=0;
        m_Game->ClientCmd_Unrestricted(m_PropHold.attached?"-attack;-attack2":"-attack;-attack2;-use");if(!m_PropHold.attached)m_UseCommandHeld=false;m_GrabPoseValid=m_PickupAimValid=false;
        PortalVrLog("Weapon interaction event=%d hand=%d stowed=%d",int(event),m_Weapon.hand,m_Weapon.stowed);
    }
    bool trigger[2]{};
    for(int h=0;h<2;++h){vr::InputDigitalActionData_t state{};trigger[h]=m_Input->GetDigitalActionData(m_MenuTriggers[h],&state,sizeof(state),vr::k_ulInvalidInputValueHandle)!=vr::VRInputError_None||!state.bActive||state.bState;}
    m_Weapon.Triggers(trigger[0],trigger[1]);
    UpdateOptionalGunSupport();
    m_WeaponConsumesGrip=m_OptionalSupportActive||event!=WeaponHandling::None||(m_Graphics.holster&&shoulderNear[m_Weapon.hand]);
    for(int h=0;h<2;++h) {
        if(!m_GripPressed[h])m_UseGripBlocked[h]=false;
        else if(m_WeaponConsumesGrip)m_UseGripBlocked[h]=true;
    }
}

void VR::UpdatePropInput() {
    const bool tracked[2]={m_ActionStateFresh&&m_LeftControllerPose.isValid,m_ActionStateFresh&&m_RightControllerPose.isValid};
    bool eligible[2]{};
    for(int h=0;h<2;++h)eligible[h]=!m_UseGripBlocked[h]&&!m_Weapon.blocked[h]
        &&!(m_Graphics.holster&&m_ShoulderHover[h])&&!m_OptionalSupportActive;
    const int previousHand=m_PropHold.hand;
    const int nextMode=m_Graphics.autoPalmStowed&&m_Weapon.stowed?1:m_Graphics.propGrabMode;
    const bool handoffNearby=m_Graphics.propTransfer&&tracked[0]&&tracked[1]&&m_PropHold.attached
        &&(GetLeftHandAbsPos()-GetRightHandAbsPos()).LengthSqr()<=powf(m_Graphics.propTransferCM*m_VRScale/100,2);
    m_PropHold.Step(HandControlledProps()&&InteractionAllowed()&&(m_PropHold.attached||GetTickCount64()>=m_HandSwitchSuppressUntil),m_GripPressed,tracked,eligible,handoffNearby);
    if(previousHand<0&&m_PropHold.hand>=0)m_PropCarryMode=nextMode;
    if(previousHand>=0&&m_PropHold.hand>=0&&previousHand!=m_PropHold.hand){m_PropVisualStamp=0;WeaponHaptic(m_PropHold.hand,.5f,m_Graphics.grabHaptics);}
    if(m_PropHold.release)m_ThrowReleaseUntil=m_PropHold.throwRelease?GetTickCount64()+300:0;
}

void VR::ProcessInput()
{
    if (!m_IsVREnabled)
        return;

    //vr::VROverlay()->SetOverlayFlag(m_HUDHandle, vr::VROverlayFlags_MakeOverlaysInteractiveIfVisible, false);

    typedef std::chrono::duration<float, std::milli> duration;
    auto currentTime = std::chrono::steady_clock::now();
    duration elapsed = currentTime - m_PrevFrameTime;
    float deltaTime = NativeFit::FrameDelta(elapsed.count());
    m_PrevFrameTime = currentTime;

    vr::InputAnalogActionData_t analogActionData;

    if (m_Graphics.turning && GetAnalogActionData(m_ActionTurn, analogActionData))
    {
        float turnAngle = 0.0f;
        if (m_SnapTurning)
        {
            if (!m_PressedTurn && analogActionData.x > std::max(.3f,m_Graphics.turnDeadzone))
            {
                turnAngle = -m_SnapTurnAngle;
                m_PressedTurn = true;
            }
            else if (!m_PressedTurn && analogActionData.x < -std::max(.3f,m_Graphics.turnDeadzone))
            {
                turnAngle = m_SnapTurnAngle;
                m_PressedTurn = true;
            }
            else if (analogActionData.x < 0.3 && analogActionData.x > -0.3)
                m_PressedTurn = false;
        }
        // Smooth turning
        else
        {
            float deadzone = m_Graphics.turnDeadzone;
            // smoother turning
            float xNormalized = (abs(analogActionData.x) - deadzone) / (1 - deadzone);
            if (analogActionData.x > deadzone)
            {
                turnAngle = -m_TurnSpeed * deltaTime * xNormalized;
            }
            if (analogActionData.x < -deadzone)
            {
                turnAngle = m_TurnSpeed * deltaTime * xNormalized;
            }
        }

        if (turnAngle != 0.0f)
        {
            m_RotationOffset.y += turnAngle;

            // Keep the real HMD fixed in the room while artificial turning
            // rotates the playspace. Without this counter-pivot, the tracked
            // head offset is rotated around the recenter point and the player
            // appears to walk in an arc around that point.
            Vector hmdRelative = m_HmdPose.TrackedDevicePos - m_Center;
            VectorPivotXY(hmdRelative, { 0, 0, 0 }, -turnAngle);
            m_Center = m_HmdPose.TrackedDevicePos - hmdRelative;

            // The center change is an intentional rotation, not roomscale
            // locomotion. Keep the roomscale delta baseline in the same frame.
            m_HmdPosRelativeRaw = hmdRelative;
            m_HmdPosRelativeRawPrev = hmdRelative;
        }

        // Wrap from 0 to 360
        m_RotationOffset.y -= 360 * std::floor(m_RotationOffset.y / 360);
		if (turnAngle != 0.0f)
			UpdateHMDAngles();
    }

    UpdateWeaponHandling();
    UpdatePropInput();
    if (PressedDigitalAction(m_ActionPrimaryAttack))
    {
        m_Game->ClientCmd_Unrestricted("+attack");
    }
    else
    {
        m_Game->ClientCmd_Unrestricted("-attack");
    }

    if (PressedDigitalAction(m_ActionSecondaryAttack))
    {
        m_Game->ClientCmd_Unrestricted("+attack2");
    }
    else
    {
        m_Game->ClientCmd_Unrestricted("-attack2");
    }

    if (PressedDigitalAction(m_ActionJump))
    {
        m_Game->ClientCmd_Unrestricted("+jump");
    }
    else
    {
        m_Game->ClientCmd_Unrestricted("-jump");
    }

    // Consume shared crouch only during a deliberate support gesture.
    if (PressedDigitalAction(m_ActionCrouch) && !m_OptionalSupportActive)
    {
        m_Game->ClientCmd_Unrestricted("+duck");
    }
    else
    {
        m_Game->ClientCmd_Unrestricted("-duck");
    }

    // Keep the normal Source input state in sync, but only emit the console
    // command on an edge. CreateMove also mirrors this state into IN_USE so
    // the pickup controller sees a stable button during the same tick.
    const bool useHeld = PressedDigitalAction(m_ActionUse);
    if(useHeld&&!m_UseCommandHeld)WeaponHaptic(PropHand(),.65f,m_Graphics.grabHaptics);
    // Publish the controller pose as soon as the action state is sampled. The
    // server-side grab callbacks can run before the next CreateMove callback;
    // keeping this snapshot here prevents that first pickup tick from falling
    // back to the HMD pose.
    SnapshotGrabPose();
    if (useHeld != m_UseCommandHeld)
    {
        m_Game->ClientCmd_Unrestricted(useHeld ? "+use" : "-use");
        m_UseCommandHeld = useHeld;
    }

    if (PressedDigitalAction(m_ActionReload))
    {
        m_Game->ClientCmd_Unrestricted("+reload");
    }
    else
    {
        m_Game->ClientCmd_Unrestricted("-reload");
    }

    if (PressedDigitalAction(m_ActionPrevItem, true))
    {
        m_Game->ClientCmd_Unrestricted("invprev");
    }
    else if (PressedDigitalAction(m_ActionNextItem, true))
    {
        m_Game->ClientCmd_Unrestricted("invnext");
    }

    if (PressedDigitalAction(m_ActionResetPosition, true))
    {
        ResetPosition();
    }

    if (PressedDigitalAction(m_ActionFlashlight, true))
    {
        m_Game->ClientCmd_Unrestricted("impulse 100");
    }

    if (PressedDigitalAction(m_Spray, true))
    {
        m_Game->ClientCmd_Unrestricted("impulse 201");
    }
    
    /*bool isControllerVertical = m_RightControllerAngAbs.x > 60 || m_RightControllerAngAbs.x < -45;
    if ((PressedDigitalAction(m_ShowHUD) || PressedDigitalAction(m_Scoreboard) || isControllerVertical || m_HudAlwaysVisible)
        && m_RenderedHud)
    {
        if (!vr::VROverlay()->IsOverlayVisible(m_HUDHandle) || m_HudAlwaysVisible)
            RepositionOverlays();

        if (PressedDigitalAction(m_Scoreboard))
            m_Game->ClientCmd_Unrestricted("+showscores");
        else
            m_Game->ClientCmd_Unrestricted("-showscores");

        vr::VROverlay()->ShowOverlay(m_HUDHandle);
    }
    else
    {
        vr::VROverlay()->HideOverlay(m_HUDHandle);
    }*/

    m_RenderedHud = false;

    if (PressedDigitalAction(m_Pause, true))
    {
        m_Game->ClientCmd_Unrestricted("gameui_activate");
        RepositionOverlays();
    }
}

VMatrix VR::VMatrixFromHmdMatrix(const vr::HmdMatrix34_t &hmdMat)
{
    // VMatrix has a different implicit coordinate system than HmdMatrix34_t, but this function does not convert between them
    VMatrix vMat(
        hmdMat.m[0][0], hmdMat.m[1][0], hmdMat.m[2][0], 0.0f,
        hmdMat.m[0][1], hmdMat.m[1][1], hmdMat.m[2][1], 0.0f,
        hmdMat.m[0][2], hmdMat.m[1][2], hmdMat.m[2][2], 0.0f,
        hmdMat.m[0][3], hmdMat.m[1][3], hmdMat.m[2][3], 1.0f
    );

    return vMat;
}

vr::HmdMatrix34_t VR::VMatrixToHmdMatrix(const VMatrix &vMat)
{
    vr::HmdMatrix34_t hmdMat = {0};

    hmdMat.m[0][0] = vMat.m[0][0];
    hmdMat.m[1][0] = vMat.m[0][1];
    hmdMat.m[2][0] = vMat.m[0][2];

    hmdMat.m[0][1] = vMat.m[1][0];
    hmdMat.m[1][1] = vMat.m[1][1];
    hmdMat.m[2][1] = vMat.m[1][2];

    hmdMat.m[0][2] = vMat.m[2][0];
    hmdMat.m[1][2] = vMat.m[2][1];
    hmdMat.m[2][2] = vMat.m[2][2];

    hmdMat.m[0][3] = vMat.m[3][0];
    hmdMat.m[1][3] = vMat.m[3][1];
    hmdMat.m[2][3] = vMat.m[3][2];

    return hmdMat;
}

vr::HmdMatrix34_t VR::GetControllerTipMatrix(vr::ETrackedControllerRole controllerRole)
{
    vr::VRInputValueHandle_t inputValue = vr::k_ulInvalidInputValueHandle;

    if (controllerRole == vr::TrackedControllerRole_RightHand)
    {
        m_Input->GetInputSourceHandle("/user/hand/right", &inputValue);
    }
    else if (controllerRole == vr::TrackedControllerRole_LeftHand)
    {
        m_Input->GetInputSourceHandle("/user/hand/left", &inputValue);
    }

    if (inputValue != vr::k_ulInvalidInputValueHandle)
    {
        char buffer[vr::k_unMaxPropertyStringSize];

        m_System->GetStringTrackedDeviceProperty(vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(controllerRole), vr::Prop_RenderModelName_String, 
                                                 buffer, vr::k_unMaxPropertyStringSize);

        vr::RenderModel_ControllerMode_State_t controllerState = {0};
        vr::RenderModel_ComponentState_t componentState = {0};

        if (vr::VRRenderModels()->GetComponentStateForDevicePath(buffer, vr::k_pch_Controller_Component_Tip, inputValue, &controllerState, &componentState))
        {
            return componentState.mTrackingToComponentLocal;
        }
    }

    // Not a hand controller role or tip lookup failed, return identity
    const vr::HmdMatrix34_t identity = 
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    return identity;
}

bool VR::CheckOverlayIntersectionForController(vr::VROverlayHandle_t overlayHandle, vr::ETrackedControllerRole controllerRole)
{
    vr::TrackedDeviceIndex_t deviceIndex = m_System->GetTrackedDeviceIndexForControllerRole(controllerRole);

    if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid)
        return false;

    vr::TrackedDevicePose_t &controllerPose = m_Poses[deviceIndex];

    if (!controllerPose.bPoseIsValid)
        return false;

    VMatrix controllerVMatrix = VMatrixFromHmdMatrix(controllerPose.mDeviceToAbsoluteTracking);
    VMatrix tipVMatrix        = VMatrixFromHmdMatrix(GetControllerTipMatrix(controllerRole));
    tipVMatrix.MatrixMul(controllerVMatrix, controllerVMatrix);

    vr::VROverlayIntersectionParams_t  params  = {0};
    vr::VROverlayIntersectionResults_t results = {0};

    params.eOrigin    = vr::VRCompositor()->GetTrackingSpace();
    params.vSource    = { controllerVMatrix.m[3][0],  controllerVMatrix.m[3][1],  controllerVMatrix.m[3][2]};
    params.vDirection = {-controllerVMatrix.m[2][0], -controllerVMatrix.m[2][1], -controllerVMatrix.m[2][2]};

    return m_Overlay->ComputeOverlayIntersection(overlayHandle, &params, &results);
}

QAngle VR::GetRightControllerAbsAngle()
{
    return m_RightControllerAngAbs;
}

QAngle& VR::GetRightControllerAbsAngleConst()
{
    return m_RightControllerAngAbs;
}

Vector VR::GetRightControllerAbsPos(Vector eyePosition)
{
    Vector offset = eyePosition;

    if (offset.x == 0 && offset.y == 0 && offset.z == 0) {
        /*int playerIndex = m_Game->m_EngineClient->GetLocalPlayer();
        C_BasePlayer* localPlayer = (C_BasePlayer*)m_Game->GetClientEntity(playerIndex);
        if (!localPlayer)
            return {0, 0, 0};

        offset = localPlayer->EyePosition();*/

        offset = m_SetupOrigin;
    }

    Vector position = offset + m_RightControllerPosRel;

    if (m_6DOF)
        position += m_HmdPosRelative;

    position.z+=HeightOffset();
    return position + m_CameraCollisionOffset;
}

Vector VR::GetRecommendedViewmodelAbsPos(Vector eyePosition)
{
    Vector viewmodelPos = GetRightControllerAbsPos(eyePosition);
    viewmodelPos -= m_ViewmodelForward * m_ViewmodelPosOffset.x;
    viewmodelPos -= m_ViewmodelRight * m_ViewmodelPosOffset.y;
    viewmodelPos -= m_ViewmodelUp * m_ViewmodelPosOffset.z;

    return viewmodelPos;
}

QAngle VR::GetRecommendedViewmodelAbsAngle()
{
    QAngle result{};

    QAngle::VectorAngles(m_ViewmodelForward, m_ViewmodelUp, result);

    return result;
}

void VR::UpdateHMDAngles() {
    QAngle hmdAngLocal = m_HmdPose.TrackedDeviceAng;

    //hmdAngLocal += m_RotationOffset;
    hmdAngLocal.x += m_RotationOffset.x;
    hmdAngLocal.y += m_RotationOffset.y;
    hmdAngLocal.z += m_RotationOffset.z;

    //hmdAngLocal.Normalize();

    QAngle::AngleVectors(hmdAngLocal, &m_HmdForward, &m_HmdRight, &m_HmdUp);

    //hmdAngLocal.x = (hmdAngLocal.x > 180 ? 180)
    hmdAngLocal.Normalize();

    m_HmdAngAbs = hmdAngLocal;
}

void VR::ResetPosition()
{
    // The compositor may not have supplied a valid HMD pose during startup.
    // Defer centering instead of treating the tracking origin as the player.
    m_CenterPending = !m_HmdPose.isValid;
    if (m_CenterPending) return;
    m_Center = m_HmdPose.TrackedDevicePos;
    m_HmdPosRelativeRaw = m_HmdPosRelative = {0,0,0};
    m_HmdPosRelativeRawPrev = {0,0,0};
    m_CameraCollisionOffset = {0,0,0};
    m_CameraBlocked = false;
    m_CalibrationDrift.Reset();
    m_CalibrationStability.Reset();
    m_CalibrationSuppressUntil = GetTickCount64()+10000;
}

void VR::UpdateAutoCalibration()
{
    const auto now=GetTickCount64();
    const float dt=m_CalibrationTime ? (now-m_CalibrationTime)*.001f : 0;
    m_CalibrationTime=now;
    const Vector playerDelta=m_SetupOrigin-m_CalibrationPlayerPosition;
    m_CalibrationPlayerPosition=m_SetupOrigin;
    const bool tracked=m_HmdPose.isValid
        && m_Poses[vr::k_unTrackedDeviceIndex_Hmd].eTrackingResult==vr::TrackingResult_Running_OK;
    if(!tracked || !m_CalibrationTracked || dt>.1f || playerDelta.LengthSqr()>64.f*64.f) {
        m_CalibrationDrift.Reset();
        m_CalibrationStability.Reset();
        m_CalibrationSuppressUntil=std::max(m_CalibrationSuppressUntil,now+2000);
        // Never turn movement during lost tracking/a paused frame into a large
        // roomscale locomotion impulse on the first recovered frame.
        m_HmdPosRelativeRawPrev=m_HmdPose.TrackedDevicePos-m_Center;
    }
    m_CalibrationTracked=tracked;
    if(!tracked) return;
    // SteamVR supplies an explicit transform for origin/floor/heading changes.
    // Compensate that transform exactly rather than treating head turns or
    // crouches as calibration errors.
    const auto rawStanding=m_System->GetRawZeroPoseToStandingAbsoluteTrackingPose();
    auto frame=AutoCalibration::SourceSpace(rawStanding.m);
    const auto space=vr::VRCompositor()->GetTrackingSpace();
    if(space==vr::TrackingUniverseSeated) {
        const auto seated=m_System->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
        frame=HandPose::Concat(HandPose::InverseRigid(AutoCalibration::SourceSpace(seated.m)),frame);
    } else if(space==vr::TrackingUniverseRawAndUncalibrated) frame=PortalPose::Frame({0,0,0},{0,0,0});
    if(m_CalibrationOrigin.Update(frame,m_AutoCalibration && !m_CenterPending,m_Center,m_RotationOffset.y)) {
        m_HmdPosRelativeRawPrev=m_HmdPose.TrackedDevicePos-m_Center;
        if(m_CalibrationOrigin.Significant()) {
            m_CameraCollisionOffset={0,0,0};m_CalibrationDrift.Reset();m_CalibrationStability.Reset();m_CalibrationSuppressUntil=now+2000;
        }
        static uint64_t nextOriginAudit=0;
        if(now>=nextOriginAudit){nextOriginAudit=now+3000;PortalVrLog("Auto calibration origin delta meters=%f yaw=%f contactReset=%d",m_CalibrationOrigin.translationMeters,m_CalibrationOrigin.yawDegrees,m_CalibrationOrigin.Significant());}
    }
    vr::InputAnalogActionData_t walk{};
    const bool moving=GetAnalogActionData(m_ActionWalk,walk) && walk.bActive && walk.x*walk.x+walk.y*walk.y>.36f;
    const bool stable=m_CalibrationStability.Step(dt,m_HmdPose.TrackedDevicePos,m_HmdPose.TrackedDeviceAng,tracked);
    vr::InputAnalogActionData_t turn{};
    const bool turning=GetAnalogActionData(m_ActionTurn,turn) && turn.bActive && fabsf(turn.x)>.2f;
    auto* player=reinterpret_cast<C_BasePlayer*>(m_Game->GetLocalPortalPlayer());
    bool eligible=m_AutoCalibration && m_6DOF && stable && !turning
        && m_Game->IsInGame() && !m_Game->IsCursorVisible() && now>=m_CalibrationSuppressUntil
        && player
        && now-m_LastCarryUpdate>500 && now-m_LastPhysicalPush>500
        && !m_HandContact[0].touching&&!m_HandContact[1].touching&&!m_GunWallTouch
        && moving && dt>0 && playerDelta.LengthSqr()<dt*dt
        && m_HmdPose.TrackedDeviceVel.LengthSqr()<.04f*.04f
        && Vector(m_HmdPose.TrackedDeviceAngVel.x,m_HmdPose.TrackedDeviceAngVel.y,m_HmdPose.TrackedDeviceAngVel.z).LengthSqr()<15.f*15.f
        && fabsf(m_HmdPose.TrackedDeviceAng.x)<25 && fabsf(m_HmdPose.TrackedDeviceAng.z)<25
        && !PressedDigitalAction(m_ActionPrimaryAttack) && !PressedDigitalAction(m_ActionSecondaryAttack)
        && !PressedDigitalAction(m_ActionUse) && !PressedDigitalAction(m_ActionCrouch);
    if(eligible) {
        Vector forward,right;
        QAngle::AngleVectors({0,m_HmdAngAbs.y,0},&forward,&right,nullptr);
        Vector movement=forward*walk.y+right*walk.x;
        VectorNormalize(movement);movement*=24.f;
        const Vector mins(-16,-16,-1),maxs(16,16,1);
        Ray_t bodyRay{},headRay{};
        Vector headOffset=m_CalibrationStability.mean-m_Center;
        headOffset.z=0;
        VectorPivotXY(headOffset,{0,0,0},m_RotationOffset.y);
        headOffset*=m_VRScale;
        bodyRay.Init(m_SetupOrigin,m_SetupOrigin+movement,mins,maxs);
        headRay.Init(m_SetupOrigin+headOffset,m_SetupOrigin+headOffset+movement,mins,maxs);
        CGameTrace bodyTrace{},headTrace{};
        CTraceFilterSkipEntity filter(reinterpret_cast<IHandleEntity*>(player),0);
        static const auto binding=PortalTrace::Binding::Resolve(m_Game->m_BaseClient);
        constexpr unsigned mask=CONTENTS_SOLID|CONTENTS_WINDOW|CONTENTS_GRATE|CONTENTS_MOVEABLE;
        auto trace=[&](Ray_t& ray,CGameTrace& hit) {
            return binding.Trace(player,ray,mask,&filter,&hit) || m_Game->TraceRay(ray,mask,&filter,&hit);
        };
        eligible=trace(bodyRay,bodyTrace) && trace(headRay,headTrace)
            && AutoCalibration::NeedsBodyAlignment(bodyTrace.fraction,bodyTrace.startsolid||bodyTrace.allsolid,
                headTrace.fraction,headTrace.startsolid||headTrace.allsolid);
    }
    const bool wasActive=m_CalibrationDrift.active;
    const Vector adjustment=m_CalibrationDrift.Step(dt,m_CalibrationStability.mean-m_Center,eligible);
    if(adjustment.LengthSqr()>0) {
        m_Center+=adjustment;
        m_HmdPosRelativeRawPrev=m_HmdPose.TrackedDevicePos-m_Center;
        if(!wasActive) PortalVrLog("Auto calibration: stable headset/player, blocked body and clear head route; easing horizontal alignment");
    } else if(wasActive) m_CalibrationSuppressUntil=now+10000;
}

void VR::SnapshotGrabPose()
{
    const bool controlled=HandControlledProps(),left=PropHand()==0;
    m_GrabPoseValid = controlled?PropHandTracked():WeaponTracked();
    m_GrabControllerPos = controlled?TwoHandAim::Position(GetPhysicalHandFrame(left!=m_LeftHanded)):WeaponPosition();
    // Carry forward from the visible wrist in the same aim orientation as the
    // gun. The uncorrected grip direction is 30 degrees higher on this binding.
    m_GrabControllerAng = PickupTrace::CarryAngles(controlled?(left?m_LeftControllerForward:m_RightControllerForward):WeaponForward(),controlled?(left?m_LeftControllerUp:m_RightControllerUp):WeaponUp());
    m_GrabHandRelative = PortalPose::RelativeHand(
        m_GrabControllerPos - m_SetupOrigin, m_GrabControllerAng, m_HmdAngAbs);
    if(controlled)m_PropVisualFromGrab=HandPose::Concat(HandPose::InverseRigid(PortalPose::Frame(m_GrabControllerPos,m_GrabControllerAng)),GetPhysicalHandFrame(left!=m_LeftHanded));
    if(controlled){m_PickupAimValid=m_GrabPoseValid;m_PickupAimRelative=m_GrabHandRelative;return;}
    // Selection follows the visible gun's centerline. Carry physics continues
    // to use the wrist snapshot above, so acquiring a prop does not move it.
    Vector aimOrigin,aimDirection;
    m_PickupAimValid = GetPortalAimRay(aimOrigin,aimDirection);
    if (m_PickupAimValid) {
        QAngle aimAngles;
        QAngle::VectorAngles(aimDirection,WeaponUp(),aimAngles);
        m_PickupAimRelative = PortalPose::RelativeHand(
            aimOrigin-m_SetupOrigin,aimAngles,m_HmdAngAbs);
    }
}

bool VR::InteractionAllowed() {
    return m_IsVREnabled&&m_SystemFocus.Allowed()&&!m_GraphicsOpen&&m_Game
        &&m_Game->IsInGame()&&!m_Game->IsCursorVisible()&&!m_BodyCapture.active;
}
void VR::AvatarFingerCurls(bool physicalLeft,float* out) {
    const bool logicalLeft=physicalLeft!=m_LeftHanded;
    const auto* source=logicalLeft?m_LeftFingerCurl:m_RightFingerCurl;
    const auto& state=m_HandContact[physicalLeft?0:1];
    const bool flat=m_Graphics.handContact&&m_Graphics.contactPose&&state.valid&&state.touching
        &&GetTickCount64()-state.time<150&&InteractionAllowed()
        &&!HandContact::Override(source,m_Graphics.fingerRange/100,m_Graphics.contactOverride/100);
    for(int f=0;f<5;++f)out[f]=flat||!m_Graphics.fingerTracking?0:source[f];
    if(m_Graphics.propShapePose&&m_PropHold.attached&&logicalLeft==(PropHand()==0)) {
        // Curl approximation keeps tracked fingers free to curl farther.
        for(int f=1;f<5;++f)out[f]=std::max(out[f],m_Graphics.propPoseCurl/100*(f==1?.8f:1.f));
    }
    out[0]=m_Graphics.thumbPose?0:std::clamp(out[0]*m_Graphics.thumbGain/100-m_Graphics.thumbStraighten/100,0.f,1.f);
}
void VR::UpdateHandContacts() {
    if(!m_Graphics.handContact||!InteractionAllowed()||PortalContactClearance()){m_HandContact[0]=m_HandContact[1]={};return;}
    auto* player=m_Game->GetClientEntity(m_Game->GetLocalPlayerIndex());
    if(!player)return;
    const auto now=GetTickCount64();const float unit=m_VRScale/100;
    const bool primaryLeft=(WeaponHand()==0)!=m_LeftHanded;
    for(int h=0;h<2;++h) {
        auto& state=m_HandContact[h];const bool physicalLeft=h==0,logicalLeft=physicalLeft!=m_LeftHanded;
        const auto& tracked=logicalLeft?m_LeftControllerPose:m_RightControllerPose;
        const bool gunHand=!m_Weapon.stowed&&m_PortalAimLastSeen&&(physicalLeft==primaryLeft||m_OptionalSupportActive);
        const bool carrying=(logicalLeft==(PropHand()==0))&&m_PropHold.attached;
        if(!tracked.isValid||gunHand||carrying){state={};continue;}
        if(now<state.next)continue;
        const auto raw=GetAvatarHandFrame(physicalLeft,false);const auto target=TwoHandAim::Position(raw);
        if(!TwoHandAim::Finite(target)){state={};continue;}
        if(state.valid&&((target-state.safe).LengthSqr()>powf(m_VRScale*1.5f,2)||now-state.time>150))state={};
        const bool previousTouch=state.touching;
        auto start=state.valid?state.safe:m_SetupOrigin;
        const auto end=target-(state.touching?state.normal*(unit*.4f):Vector(0,0,0));
        CGameTrace hit{};hit.fraction=1;
        CTraceFilterSkipEntity filter(reinterpret_cast<IHandleEntity*>(player),0);
        Ray_t ray;const float radius=m_Graphics.contactRadiusCM*unit;
        ray.Init(start,end,{-radius,-radius,-radius},{radius,radius,radius});
        // Portal-aware client trace, already used by head/camera collision.
        static const auto binding=PortalTrace::Binding::Resolve(m_Game->m_BaseClient);
        const bool traced=binding.Trace(player,ray,0x0600400b,&filter,&hit)||m_Game->TraceRay(ray,0x0600400b,&filter,&hit);
        state.next=now+static_cast<unsigned long long>(1000/std::clamp(m_Graphics.contactHz,20.f,90.f));
        if(!traced||hit.startsolid||hit.allsolid){state.valid=false;state.touching=false;continue;}
        state.valid=true;state.time=now;state.touching=hit.fraction<.999f&&TwoHandAim::Unit(hit.plane.normal);
        state.safe=state.touching?hit.endpos+hit.plane.normal*(unit*.15f):target;
        if(state.touching)state.normal=hit.plane.normal;
        const auto* curls=logicalLeft?m_LeftFingerCurl:m_RightFingerCurl;
        const bool overridePose=HandContact::Override(curls,m_Graphics.fingerRange/100,m_Graphics.contactOverride/100);
        state.frame=HandContact::Surface(raw,state.normal,state.safe,state.touching&&m_Graphics.contactPose&&!overridePose);
        if(state.touching&&!previousTouch){static unsigned contacts=0;if(contacts++<32)PortalVrLog("World hand contact physical=%d radiusCM=%f correctionCM=%f normal=%f,%f,%f poseOverride=%d",h,m_Graphics.contactRadiusCM,sqrtf((target-state.safe).LengthSqr())/unit,state.normal.x,state.normal.y,state.normal.z,overridePose);}
        if(state.touching&&!previousTouch&&m_Graphics.contactHaptics)ControllerHaptic(h,.2f,m_Graphics.hapticDurationMS/1000,m_Graphics.hapticFrequency,"world-touch");
    }
}

void VR::UpdateTracking()
{
    GetPoses();
    UpdateAutoCalibration();
    if (m_CenterPending && m_HmdPose.isValid) ResetPosition();

    // HMD tracking
    Vector hmdPosLocal = m_HmdPose.TrackedDevicePos;
    Vector hmdPosCentered = hmdPosLocal - m_Center;

    m_HmdPosRelativeRaw = hmdPosCentered;

    //std::cout << "HMD - X: " << hmdWorldPos.x << ", Y: " << hmdWorldPos.y << ", Z: " << hmdWorldPos.z << "\n";

    Vector hmdPosCorrected = hmdPosCentered;
    VectorPivotXY(hmdPosCorrected, { 0, 0, 0 }, m_RotationOffset.y);
    
    UpdateHMDAngles();

    m_HmdPosRelative = hmdPosCorrected * m_VRScale;

    C_BasePlayer* localPlayer = reinterpret_cast<C_BasePlayer *>(m_Game->GetLocalPortalPlayer());
    if (!localPlayer)
        return;

    // Roomscale setup
    /*Vector cameraMovingDirection = m_Center - m_SetupOriginPrev;
    Vector cameraToPlayer = m_HmdPosAbsPrev - m_SetupOriginPrev;
    cameraMovingDirection.z = 0;
    cameraToPlayer.z = 0;
    float cameraFollowing = DotProduct(cameraMovingDirection, cameraToPlayer);
    float cameraDistance = VectorLength(cameraToPlayer);

    if (localPlayer->m_hGroundEntity != -1 && localPlayer->m_vecVelocity.IsZero())
        m_RoomscaleActive = true;

    // TODO: Get roomscale to work while using thumbstick
    if ((cameraFollowing < 0 && cameraDistance > 1) || (m_PushingThumbstick))
        m_RoomscaleActive = false;*/

    // Check if camera is clipping inside wall
    /*CGameTrace trace;
    Ray_t ray;
    CTraceFilterSkipEntity tracefilter((IHandleEntity*)localPlayer, 0);

    Vector extendedHmdPos = m_HmdPosAbs - m_SetupOrigin;
    VectorNormalize(extendedHmdPos);
    extendedHmdPos = m_HmdPosAbs + (extendedHmdPos * 10);
    ray.Init(m_SetupOrigin, extendedHmdPos);

    if (!m_Game->TraceRay(ray, STANDARD_TRACE_MASK, &tracefilter, &trace))
        return vecEnd;
    if (trace.fraction < 1 && trace.fraction > 0)
    {
        Vector distanceInsideWall = trace.endpos - extendedHmdPos;
        m_CameraAnchor += distanceInsideWall;
        m_HmdPosAbs = m_CameraAnchor - Vector(0, 0, 64) + m_HmdPosLocalInWorld;
    }

    // Reset camera if it somehow gets too far
    m_SetupOriginToHMD = m_HmdPosAbs - m_SetupOrigin;
    if (VectorLength(m_SetupOriginToHMD) > 150)
        ResetPosition();

    m_HmdPosAbsPrev = m_HmdPosAbs;
    m_SetupOriginPrev = m_SetupOrigin;*/

    GetViewParameters();
    m_Ipd = m_EyeToHeadTransformPosRight.x * 2;
    m_EyeZ = m_EyeToHeadTransformPosRight.z;

    // Hand tracking
    Vector leftControllerPosLocal = m_LeftControllerPose.TrackedDevicePos;
    QAngle leftControllerAngLocal = m_LeftControllerPose.TrackedDeviceAng;

    Vector rightControllerPosLocal = m_RightControllerPose.TrackedDevicePos;
    QAngle rightControllerAngLocal = m_RightControllerPose.TrackedDeviceAng;

    //std::cout << "Right Controller - X: " << rightControllerPosLocal.x << "Y: " << rightControllerPosLocal.y << "Z: " << rightControllerPosLocal.z << "\n";

    Vector hmdToController = rightControllerPosLocal - hmdPosLocal;
    //Vector rightControllerPosCorrected = hmdPosCorrected + hmdToController;

    // When using stick turning, pivot the controllers around the HMD
    VectorPivotXY(hmdToController, { 0, 0, 0 }, m_RotationOffset.y);

    m_RightControllerPosRel = hmdToController * m_VRScale;
    Vector hmdToLeftController = leftControllerPosLocal - hmdPosLocal;
    VectorPivotXY(hmdToLeftController, { 0, 0, 0 }, m_RotationOffset.y);
    m_LeftControllerPosRel = hmdToLeftController * m_VRScale;
    leftControllerAngLocal.x += m_RotationOffset.x;
    leftControllerAngLocal.y += m_RotationOffset.y;
    leftControllerAngLocal.z += m_RotationOffset.z;

    //rightControllerAngLocal += m_RotationOffset;
    rightControllerAngLocal.x += m_RotationOffset.x;
    rightControllerAngLocal.y += m_RotationOffset.y;
    rightControllerAngLocal.z += m_RotationOffset.z;

    // Wrap angle from -180 to 180
    //rightControllerAngLocal.Normalize();

    QAngle::AngleVectors(leftControllerAngLocal, &m_LeftControllerForward, &m_LeftControllerRight, &m_LeftControllerUp);
    QAngle::AngleVectors(rightControllerAngLocal, &m_RightControllerForward, &m_RightControllerRight, &m_RightControllerUp);

    // Bare wrists use the tracked grip orientation, without the gun aim tilt.
    m_RightHandForward = m_RightControllerForward;
    m_RightHandUp = m_RightControllerUp;
    m_LeftHandForward = m_LeftControllerForward;
    m_LeftHandUp = m_LeftControllerUp;

    const float offset = -30;

    // Adjust controller angle downward
    m_LeftControllerForward = VectorRotate(m_LeftControllerForward, m_LeftControllerRight, offset);
    m_LeftControllerUp = VectorRotate(m_LeftControllerUp, m_LeftControllerRight, offset);

    m_RightControllerForward = VectorRotate(m_RightControllerForward, m_RightControllerRight, offset);
    m_RightControllerUp = VectorRotate(m_RightControllerUp, m_RightControllerRight, offset);

    // controller angles
    QAngle::VectorAngles(m_LeftControllerForward, m_LeftControllerUp, m_LeftControllerAngAbs);
    QAngle::VectorAngles(m_RightControllerForward, m_RightControllerUp, m_RightControllerAngAbs);
    m_RightControllerAngAbs.Normalize();

    PositionAngle viewmodelOffset = PositionAngle{ {4.5, -1, 1.5}, {0,0,0} };

    // Apply both hardcoded and custom (from config) viewmodel offsets here:
    m_ViewmodelPosOffset = viewmodelOffset.position + m_ViewmodelPosCustomOffset;
    m_ViewmodelAngOffset = viewmodelOffset.angle + m_ViewmodelAngCustomOffset;

    m_ViewmodelForward = m_RightControllerForward;
    m_ViewmodelUp = m_RightControllerUp;
    m_ViewmodelRight = m_RightControllerRight;

    // Viewmodel yaw offset
    m_ViewmodelForward = VectorRotate(m_ViewmodelForward, m_ViewmodelUp, m_ViewmodelAngOffset.y);
    m_ViewmodelRight = VectorRotate(m_ViewmodelRight, m_ViewmodelUp, m_ViewmodelAngOffset.y);

    // Viewmodel pitch offset
    m_ViewmodelForward = VectorRotate(m_ViewmodelForward, m_ViewmodelRight, m_ViewmodelAngOffset.x);
    m_ViewmodelUp = VectorRotate(m_ViewmodelUp, m_ViewmodelRight, m_ViewmodelAngOffset.x);

    // Viewmodel roll offset
    m_ViewmodelRight = VectorRotate(m_ViewmodelRight, m_ViewmodelForward, m_ViewmodelAngOffset.z);
    m_ViewmodelUp = VectorRotate(m_ViewmodelUp, m_ViewmodelForward, m_ViewmodelAngOffset.z);

    UpdateGunWallContact();
    SnapshotContactPoints();
    UpdateHandContacts();
    // Trace only after this frame's controller positions and orientations are ready.
    m_AimPos = Trace((uint32_t*)localPlayer);
    UpdateCrosshair();

    if (m_Game->m_Hooks->CreatePingPointer &&
        m_Game->m_Offsets->SetControlPoint.address && m_Game->m_Offsets->StopEmission.address) {
        C_Portal_Player* portalPlayer = (C_Portal_Player*)localPlayer;

        auto* activeWeaponAddr=m_Game->GetLocalPortalWeapon();
        //auto activeWeaponAddr = (*(int(__thiscall**)(void*))(*(uintptr_t*)m_Game->m_Offsets->GetActivePortalWeapon.address))(portalPlayer);

        if (m_AimMode == 2 && activeWeaponAddr && m_DrawCrosshair && !m_Weapon.stowed && m_Graphics.gun && WeaponTracked()) {
            CWeaponPortalBase* activeWeapon = (CWeaponPortalBase*)activeWeaponAddr;

            if (portalPlayer->m_PointLaser) {
                portalPlayer->m_PointLaser->SetControlPoint(1, m_AimPos);
                const int color=SigScanner::IsReadable(reinterpret_cast<uintptr_t>(activeWeapon),sizeof(CWeaponPortalBase))?std::clamp(activeWeapon->m_iLastFiredPortal,0,1):0;
                portalPlayer->m_PointLaser->SetControlPoint(2, m_Game->m_singlePlayerPortalColors[color] * 0.5f);
            }
            else if (m_Game->m_Hooks->CreatePingPointer) {
                std::cout << "Creating Point Laser Beam Sight Thingy" << "\n";
                m_Game->m_Hooks->CreatePingPointer(localPlayer, m_AimPos);
            }
        }
        else if (portalPlayer->m_PointLaser){
            portalPlayer->m_PointLaser->StopEmission(false, true, false);
            portalPlayer->m_PointLaser = NULL;
        }
    }
}

Vector VR::GetViewAngle()
{
    return Vector( m_HmdAngAbs.x, m_HmdAngAbs.y, m_HmdAngAbs.z );
}

Vector VR::GetViewOrigin(Vector setupOrigin)
{
    Vector center = setupOrigin;

    if (m_6DOF)
        center += m_HmdPosRelative;

    center.z+=HeightOffset();
    return center + (m_HmdForward * -(m_EyeZ * m_VRScale)) + m_CameraCollisionOffset;
}

void VR::UpdateCameraCollision(Vector setupOrigin)
{
    // Always solve from the engine's current player eye position. Never move
    // the tracking origin: leaning back, respawning, or portalling must recover.
    m_CameraCollisionOffset = { 0, 0, 0 };
    if(!m_Graphics.collision) {m_CameraBlocked=false;return;}
    auto* player = m_Game->GetLocalPortalPlayer();
    if (!player)
    {
        m_CameraBlocked = false;
        return;
    }

    const Vector desired = GetViewOrigin(setupOrigin);
    static const PortalTrace::Binding portalTrace = PortalTrace::Binding::Resolve(m_Game->m_BaseClient);
    const bool atPortal=portalTrace.Environment(player)!=nullptr;
    const float radius = CameraCollision::HullRadius(m_Ipd * m_IpdScale * m_VRScale, m_Fov, m_Aspect)
        *(atPortal?std::clamp(m_Graphics.portalHeadClearance/100,.1f,1.f):1.f);
    const Vector extent(radius, radius, radius);
    Ray_t ray{};
    ray.Init(setupOrigin, desired, extent * -1.0f, extent);
    CGameTrace trace;
    trace.fraction = 1.0f;
    trace.startsolid = trace.allsolid = false;
    CTraceFilterSkipEntity filter(reinterpret_cast<IHandleEntity*>(player), 0);
    constexpr unsigned mask = CONTENTS_SOLID | CONTENTS_WINDOW | CONTENTS_GRATE | CONTENTS_MOVEABLE;
    static bool loggedBinding = false;
    if (!loggedBinding) {
        PortalVrLog("Portal-aware head collision available=%d", portalTrace.function != nullptr);
        loggedBinding = true;
    }
    const bool usedPortal = portalTrace.Trace(player, ray, mask, &filter, &trace);
    if (!usedPortal && !m_Game->TraceRay(ray, mask, &filter, &trace))
    {
        m_CameraBlocked = false;
        return;
    }

    // An actual engine trace catches ABI errors that pure geometry tests cannot.
    static const bool debugCollision = strstr(GetCommandLineA(), "-portalvr-debug-collision") != nullptr;
    static bool wasPortal = false;
    if (debugCollision && usedPortal != wasPortal) {
        PortalVrLog("Head collision portal environment=%d fraction=%f startsolid=%d allsolid=%d",
            usedPortal, trace.fraction, trace.startsolid, trace.allsolid);
        wasPortal = usedPortal;
    }
    static bool loggedProbe = false;
    if (debugCollision && !loggedProbe)
    {
        Ray_t floorRay{};
        floorRay.Init(setupOrigin, setupOrigin - Vector(0, 0, 4096), extent * -1.0f, extent);
        CGameTrace floorTrace;
        if (m_Game->TraceRay(floorRay, mask, &filter, &floorTrace))
        {
            PortalVrLog("Collision engine probe fraction=%f startsolid=%d allsolid=%d start=%f,%f,%f end=%f,%f,%f rayFlags=%d,%d",
                floorTrace.fraction, floorTrace.startsolid, floorTrace.allsolid,
                setupOrigin.x, setupOrigin.y, setupOrigin.z,
                floorTrace.endpos.x, floorTrace.endpos.y, floorTrace.endpos.z,
                floorRay.m_IsRay, floorRay.m_IsSwept);
            loggedProbe = true;
        }
    }

    const Vector safe = CameraCollision::Constrain(setupOrigin, desired, trace.fraction, trace.startsolid, trace.allsolid);
    m_CameraCollisionOffset = safe - desired;
    const bool blocked = m_CameraCollisionOffset.LengthSqr() > 0.0001f;
    if (blocked != m_CameraBlocked)
        PortalVrLog("Head collision blocked=%d fraction=%f radius=%f correction=%f,%f,%f startsolid=%d allsolid=%d",
            blocked, trace.fraction, radius, m_CameraCollisionOffset.x, m_CameraCollisionOffset.y,
            m_CameraCollisionOffset.z, trace.startsolid, trace.allsolid);
    m_CameraBlocked = blocked;
}

Vector VR::GetViewOriginLeft(Vector setupOrigin)
{
    Vector viewOriginLeft = GetViewOrigin(setupOrigin);
    viewOriginLeft -= m_HmdRight * ((m_Ipd * m_IpdScale * m_VRScale) / 2);

    return viewOriginLeft;
}

Vector VR::GetViewOriginRight(Vector setupOrigin)
{
    Vector viewOriginRight = GetViewOrigin(setupOrigin);
    viewOriginRight += m_HmdRight * ((m_Ipd * m_IpdScale * m_VRScale) / 2);

    return viewOriginRight;
}

bool VR::GetPortalAimRay(Vector& origin, Vector& direction) {
    if (!m_IsVREnabled || !WeaponTracked() || m_Weapon.stowed) return false;
    origin = WeaponPosition();
    direction = WeaponForward();
    // This is model-local metadata, not a cached world pose. Keep it while the
    // same weapon is equipped, including when an overhead gun is culled.
    if (m_PortalAimLastSeen) {
        const auto controller = GetGunControllerFrame();
        return GunRay::FromBarrel(HandPose::Concat(controller,m_PortalAimFromController),
            WeaponPosition(),origin,direction);
    }
    return std::isfinite(direction.LengthSqr()) && direction.LengthSqr() > .9f;
}

Vector VR::Trace(uint32_t* localPlayer) {
    Vector vecStart, direction;
    if (!GetPortalAimRay(vecStart,direction)) return GetRightHandAbsPos();
    Vector vecEnd = vecStart + direction * MAX_TRACE_LENGTH;

    CGameTrace trace;
    Ray_t ray;
    CTraceFilterSkipEntity tracefilter((IHandleEntity*)localPlayer, 0);

    ray.Init(vecStart, vecEnd);

    if (!m_Game->TraceRay(ray, MASK_SHOT | MASK_SHOT_HULL, &tracefilter, &trace))
        return vecEnd;

    return trace.endpos;
}

void AngleMatrix(const QAngle& angles, matrix3x4_t& matrix)
{
    float sr, sp, sy, cr, cp, cy;

    SinCos(DEG2RAD(angles[YAW]), &sy, &cy);
    SinCos(DEG2RAD(angles[PITCH]), &sp, &cp);
    SinCos(DEG2RAD(angles[ROLL]), &sr, &cr);

    // matrix = (YAW * PITCH) * ROLL
    matrix[0][0] = cp * cy;
    matrix[1][0] = cp * sy;
    matrix[2][0] = -sp;

    // NOTE: Do not optimize this to reduce multiplies! optimizer bug will screw this up.
    matrix[0][1] = sr * sp * cy + cr * -sy;
    matrix[1][1] = sr * sp * sy + cr * cy;
    matrix[2][1] = sr * cp;
    matrix[0][2] = (cr * sp * cy + -sr * -sy);
    matrix[1][2] = (cr * sp * sy + -sr * cy);
    matrix[2][2] = cr * cp;

    matrix[0][3] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][3] = 0.0f;
}

void MatrixCopy(const matrix3x4_t& in, matrix3x4_t& out)
{
    memcpy(out.Base(), in.Base(), sizeof(float) * 3 * 4);
}


/*
================
R_ConcatTransforms
================
*/

void ConcatTransforms(const matrix3x4_t& in1, const matrix3x4_t& in2, matrix3x4_t& out)
{
    if (&in1 == &out)
    {
        matrix3x4_t in1b;
        MatrixCopy(in1, in1b);
        ConcatTransforms(in1b, in2, out);
        return;
    }
    if (&in2 == &out)
    {
        matrix3x4_t in2b;
        MatrixCopy(in2, in2b);
        ConcatTransforms(in1, in2b, out);
        return;
    }
    out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
        in1[0][2] * in2[2][0];
    out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
        in1[0][2] * in2[2][1];
    out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
        in1[0][2] * in2[2][2];
    out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
        in1[0][2] * in2[2][3] + in1[0][3];
    out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
        in1[1][2] * in2[2][0];
    out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
        in1[1][2] * in2[2][1];
    out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
        in1[1][2] * in2[2][2];
    out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
        in1[1][2] * in2[2][3] + in1[1][3];
    out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
        in1[2][2] * in2[2][0];
    out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
        in1[2][2] * in2[2][1];
    out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
        in1[2][2] * in2[2][2];
    out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
        in1[2][2] * in2[2][3] + in1[2][3];
}

void MatrixAngles(const matrix3x4_t& matrix, float* angles)
{
    float forward[3];
    float left[3];
    float up[3];

    //
    // Extract the basis vectors from the matrix. Since we only need the Z
    // component of the up vector, we don't get X and Y.
    //
    forward[0] = matrix[0][0];
    forward[1] = matrix[1][0];
    forward[2] = matrix[2][0];
    left[0] = matrix[0][1];
    left[1] = matrix[1][1];
    left[2] = matrix[2][1];
    up[2] = matrix[2][2];

    float xyDist = sqrtf(forward[0] * forward[0] + forward[1] * forward[1]);

    // enough here to get angles?
    if (xyDist > 0.001f)
    {
        // (yaw)	y = ATAN( forward.y, forward.x );		-- in our space, forward is the X axis
        angles[1] = RAD2DEG(atan2f(forward[1], forward[0]));

        // (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
        angles[0] = RAD2DEG(atan2f(-forward[2], xyDist));

        // (roll)	z = ATAN( left.z, up.z );
        angles[2] = RAD2DEG(atan2f(left[2], up[2]));
    }
    else	// forward is mostly Z, gimbal lock-
    {
        // (yaw)	y = ATAN( -left.x, left.y );			-- forward is mostly z, so use right for yaw
        angles[1] = RAD2DEG(atan2f(-left[0], left[1]));

        // (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
        angles[0] = RAD2DEG(atan2f(-forward[2], xyDist));

        // Assume no roll in this case as one degree of freedom has been lost (i.e. yaw == roll)
        angles[2] = 0;
    }
}

inline void MatrixAngles(const matrix3x4_t& matrix, QAngle& angles)
{
    MatrixAngles(matrix, &angles.x);
}



// transform a set of angles in the input space of parentMatrix to the output space
QAngle TransformAnglesToWorldSpace(const QAngle& angles, const matrix3x4_t& parentMatrix)
{
    matrix3x4_t angToParent, angToWorld;
    AngleMatrix(angles, angToParent);
    ConcatTransforms(parentMatrix, angToParent, angToWorld);
    QAngle out;
    MatrixAngles(angToWorld, out);
    return out;
}


Vector VR::TraceEye(uint32_t* localPlayer, Vector cameraPos, Vector eyePos, QAngle& eyeAngle) {
    CGameTrace trTestObstructionsNearPortals;
    Ray_t ray;
    CTraceFilterSkipEntity tracefilter((IHandleEntity*)localPlayer, 0);

    ray.Init(cameraPos, eyePos);
    if (!m_Game->TraceRay(ray, MASK_SHOT | MASK_SHOT_HULL, &tracefilter, &trTestObstructionsNearPortals))
        return eyePos;

    float flWallHitFraction = trTestObstructionsNearPortals.fraction + 0.01f;
    CPortal_Base2D* pPortal = (CPortal_Base2D*)m_Game->m_Hooks->UTIL_Portal_FirstAlongRay(ray, flWallHitFraction);

    if (trTestObstructionsNearPortals.DidHit() && pPortal) {
        float flRayHitFraction = m_Game->m_Hooks->UTIL_IntersectRayWithPortal(ray, pPortal);
        //Vector vNewEye;
        Vector vHitPoint = ray.m_Start + ray.m_Delta * flRayHitFraction;
        //vNewEye = m_Game->m_Hooks->UTIL_Portal_PointTransform(pPortal->MatrixThisToLinked(), vHitPoint, vNewEye);

        //VMatrix matrix = *(VMatrix*)((uintptr_t)pPortal + 0x4C4);
        VMatrix matrix = pPortal->MatrixThisToLinked();

   
        /*QAngle newAngle;
        m_Game->m_Hooks->UTIL_Portal_AngleTransform(matrix, eyeAngle, newAngle);*/
        eyeAngle = TransformAnglesToWorldSpace(eyeAngle, matrix.As3x4());

        return matrix * vHitPoint;

        //return pPortal->MatrixThisToLinked() * vHitPoint;
    }

    return eyePos;
}

// [CONFIG PARSING UTILITY FUNCTION]
// Generates an error message by stringifying and concatenating 'args...'.
template <typename... Ts>
static void concatErrorMsg(Game& game, const Ts&... args)
{
    std::ostringstream oss;
    (oss << ... << args);
    // Configuration errors recover to defaults; never block startup in a
    // modal dialog or throw out of the render-thread bootstrap.
    PortalVrLog("Config: %s",oss.str().c_str());
}

// [CONFIG PARSING UTILITY FUNCTION]
// Attempts to parse an entry with key 'key' from the provided 'userConfig'. If the key is
// missing or if the parsing fails, 'defaultValue' is returned and an error message is
// generated.
template <typename T>
static T parseConfigEntry(
    const std::unordered_map<std::string, std::string>& userConfig, Game& game,
    const char* key, const T& defaultValue)
{
    const auto itr = userConfig.find(key);

    if (itr == userConfig.end())
    {
        concatErrorMsg(game, "Config entry with key '", key,
            "' missing -- reverting to default value of '", defaultValue, "'");

        return defaultValue;
    }

    bool valid=false;
    const auto result=VrSettings::ConfigValue<T>(itr->second,defaultValue,&valid);
    if(!valid)concatErrorMsg(game,"Invalid config entry '",key,"'; using default '",defaultValue,"'");
    return result;
}

void VR::ParseConfigFile()
{
    char runtimeDirectory[MAX_PATH] = {};
    if (!GetRuntimeBaseDirectory(runtimeDirectory, sizeof(runtimeDirectory)))
        return;
    std::ifstream configStream(std::filesystem::path(runtimeDirectory) / "VR" / "config.txt");
    std::unordered_map<std::string, std::string> userConfig;

    std::string line;
    while (std::getline(configStream, line))
    {
        std::istringstream sLine(line);
        std::string key;
        if (std::getline(sLine, key, '='))
        {
            std::string value;
            if (std::getline(sLine, value, '#'))
                userConfig[key] = value;
            else if (std::getline(sLine, value))
                userConfig[key] = value;
        }
    }

    if (userConfig.empty())
        return;

    // Parse a single entry with key 'key' from the config into 'target'.
    // If the entry does not exist, or if the parsing fails, sets 'target' to
    // 'defaultValue'.
    const auto parseOrDefault = [&](const char* key, auto& target,
                                    const auto& defaultValue) 
    { 
        using Target=std::decay_t<decltype(target)>;
        target = parseConfigEntry(userConfig, *m_Game, key, static_cast<Target>(defaultValue));
        std::cout << "Setting '" << key << "' to '" << target << "'\n";
    };

    // Parses a vector or angle from the config into 'target'. The XYZ coordinates
    // are read from three separate config entries with key 'keyPrefix' + 'X'/'Y'/'Z'.
    // If any entry does not exist, or if the parsing fails, sets the corresponding
    // coordinate in 'target' to zero.
    const auto parseXYZOrDefaultZero = [&](std::string keyPrefix, auto& target)
    {
        parseOrDefault((keyPrefix + "X").c_str(), target.x, 0.f);
        parseOrDefault((keyPrefix + "Y").c_str(), target.y, 0.f);
        parseOrDefault((keyPrefix + "Z").c_str(), target.z, 0.f);
    };

    parseOrDefault("SnapTurning", m_SnapTurning, false);
    parseOrDefault("SnapTurnAngle", m_SnapTurnAngle, 45.0f);
    parseOrDefault("TurnSpeed", m_TurnSpeed, 0.15f);
    parseOrDefault("LeftHanded", m_LeftHanded, false);
    parseOrDefault("VRScale", m_VRScale, 43.2f);
    parseOrDefault("IPDScale", m_IpdScale, 1.0f);
    parseOrDefault("6DOF", m_6DOF, true);
    // Optional in older configs: upgrading must not introduce a modal warning.
    if (userConfig.count("AutoCalibration")) parseOrDefault("AutoCalibration", m_AutoCalibration, true);
    /*parseOrDefault("HudDistance", m_HudDistance, 1.3f);
    parseOrDefault("HudSize", m_HudSize, 4.0f);
    parseOrDefault("HudAlwaysVisible", m_HudAlwaysVisible, false);*/
    parseOrDefault("AimMode", m_AimMode, 2);
    parseOrDefault("FirstPersonBody", m_FirstPersonBody, true);
    parseOrDefault("FirstPersonBodyHideUpper", m_FirstPersonBodyHideUpper, true);
    parseOrDefault("LeftHandGunGrip", m_LeftHandGunGrip, true);
    parseOrDefault("LeftHandGunGripRadius", m_LeftHandGunGripRadius, 6.0f);
    m_LeftHandGunGripRadius = std::isfinite(m_LeftHandGunGripRadius)
        ? std::clamp(m_LeftHandGunGripRadius, 2.0f, 12.0f) : 6.0f;
    parseOrDefault("FirstPersonBodyBackOffset", m_FirstPersonBodyBackOffset, 8.0f);
    m_FirstPersonBodyBackOffset = std::isfinite(m_FirstPersonBodyBackOffset)
        ? std::clamp(m_FirstPersonBodyBackOffset, 0.0f, 24.0f) : 8.0f;
    parseOrDefault("AntiAliasing", m_AntiAliasing, 0);
    parseOrDefault("RenderWindow", m_RenderWindow, 0);
    if(userConfig.count("NativeModels")) parseOrDefault("NativeModels",m_NativeModels,true);
    const auto optional=[&](const char* key,auto& value) {if(userConfig.count(key))parseOrDefault(key,value,value);};
    optional("DetailedGpuTiming",m_Graphics.detailedGpuTiming);optional("MirrorReuseEye",m_Graphics.mirrorReuseEye);
    optional("NativeSceneHUD",m_Graphics.nativeSceneHud);optional("PhysicalTestButtons",m_Graphics.physicalButtons);
    optional("MirrorEye",m_Graphics.mirrorEye);m_Graphics.mirrorEye=std::clamp(m_Graphics.mirrorEye,0,1);
    optional("MirrorFit",m_Graphics.mirrorFit);m_Graphics.mirrorFit=std::clamp(m_Graphics.mirrorFit,0,1);
    optional("RenderScalePercent",m_Graphics.scale);
    optional("SpatialUpscaler",m_Graphics.upscaler);m_Graphics.upscaler=std::clamp(m_Graphics.upscaler,0,3);
    optional("MotionBlur",m_Graphics.blur);optional("Bloom",m_Graphics.bloom);
    optional("DynamicShadows",m_Graphics.shadows);optional("TextureQuality",m_Graphics.texture);
    optional("AnisotropicFiltering",m_Graphics.filter);optional("ShowFPS",m_Graphics.showFps);
    m_Graphics.scale=std::clamp(m_Graphics.scale,60,150);
    m_AntiAliasing=(m_AntiAliasing==2 || m_AntiAliasing==4 || m_AntiAliasing==8) ? m_AntiAliasing : 0;
    m_Graphics.msaa=int(m_AntiAliasing);m_Graphics.mirror=m_RenderWindow!=0;
    m_Graphics.texture=std::clamp(m_Graphics.texture,0,2);m_Graphics.filter=std::clamp(m_Graphics.filter,1,16);
    // Load legacy preferences into the shared menu model before optional new
    // keys, preserving saved handedness, body and turning choices.
    m_Graphics.snap=m_SnapTurning;m_Graphics.snapAngle=m_SnapTurnAngle;m_Graphics.turnDegrees=m_TurnSpeed*1000;
    m_Graphics.leftHanded=m_LeftHanded;m_Graphics.body=m_FirstPersonBody;m_Graphics.hideUpper=m_FirstPersonBodyHideUpper;
    m_Graphics.bodyBack=m_FirstPersonBodyBackOffset;m_Graphics.support=m_LeftHandGunGrip;
    m_Graphics.supportRadiusCM=m_LeftHandGunGripRadius*100/m_VRScale;
    m_Graphics.roomscale=m_6DOF;m_Graphics.autoCalibration=m_AutoCalibration;m_Graphics.aim=m_AimMode;
    VrMenu::Load(m_Graphics,userConfig);
    parseXYZOrDefaultZero("ViewmodelPosCustomOffset", m_ViewmodelPosCustomOffset);
    parseXYZOrDefaultZero("ViewmodelAngCustomOffset", m_ViewmodelAngCustomOffset);
}

void VR::WaitForConfigUpdate()
{
    char currentDir[MAX_STR_LEN];
    GetCurrentDirectory(MAX_STR_LEN, currentDir);
    char configDir[MAX_STR_LEN];
    sprintf_s(configDir, MAX_STR_LEN, "%s\\VR\\", currentDir);
    HANDLE fileChangeHandle = FindFirstChangeNotificationA(configDir, false, FILE_NOTIFY_CHANGE_LAST_WRITE);

    std::filesystem::file_time_type configLastModified;
    while (1)
    {
        try 
        {
            // Windows only notifies of change within a directory, so extra check here for just config.txt
            auto configModifiedTime = std::filesystem::last_write_time("VR\\config.txt");
            if (configModifiedTime != configLastModified)
            {
                configLastModified = configModifiedTime;
                ParseConfigFile();
                
                std::cout << "Successfully reloaded 'config.txt'\n";
            }
        }
        catch (const std::invalid_argument &e)
        {
            concatErrorMsg(
                *m_Game, "Failed to parse 'config.txt' (", e.what(), ")");
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            concatErrorMsg(
                *m_Game, "'config.txt' not found. (", e.what(), ")");
            
            return;
        }
        
        FindNextChangeNotification(fileChangeHandle);
        WaitForSingleObject(fileChangeHandle, INFINITE);
        Sleep(100); // Sometimes the thread tries to read config.txt before it's finished writing
    }
}
#include "vrgraphics_impl.h"
