#pragma once
#include "graphicssettings.h"
#include "vrmenuoptions.h"
#include "menupose.h"
#include "menuraster.h"
#include "framestats.h"
#include "scenebookmark53.h"
#include "forearmroll54.h"
#include "vrfocus.h"
#include "openvr.h"
#include "vector.h"
#include <chrono>
#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "optionalgungrip.h"
#include "weaponhandling.h"
#include "weaponpolish.h"
#include "gripzone.h"
#include "autocalibration.h"
#include "bodycalibration.h"
#include "handcontact.h"
#include "propholdinput.h"
#include "bodysecondary.h"
#include "bodyresponse.h"
#include "reticle.h"
#include "worldpush.h"
#include "guncontact.h"
#include "gunhandcontact.h"
#include "supportgrip49.h"
#include "gunanimation.h"
#include "gunshellmesh.h"
#include "creatorcamera.h"
#include "triggercolors.h"

#define MAX_STR_LEN 256

class Game;
struct IDirect3DTexture9;
struct IDirect3DSurface9;
class ITexture;


struct TrackedDevicePoseData
{
	bool isValid = false;
	std::string TrackedDeviceName;
	Vector TrackedDevicePos = { 0, 0, 0 };
	Vector TrackedDeviceVel = { 0, 0, 0 };
	QAngle TrackedDeviceAng = { 0, 0, 0 };
	QAngle TrackedDeviceAngVel = { 0, 0, 0 };
};

class SharedTextureHolder
{
public:
	vr::VRVulkanTextureData_t m_VulkanData{};
	vr::Texture_t m_VRTexture{};
};

class VR
{
public:
	Game *m_Game = nullptr;

	vr::IVRSystem *m_System = nullptr;
	vr::IVRInput *m_Input = nullptr;
	vr::IVROverlay *m_Overlay = nullptr;

	vr::VROverlayHandle_t m_MainMenuHandle = vr::k_ulOverlayHandleInvalid;
	//vr::VROverlayHandle_t m_HUDHandle;

	float m_HorizontalOffsetLeft;
	float m_VerticalOffsetLeft;
	float m_HorizontalOffsetRight;
	float m_VerticalOffsetRight;

	uint32_t m_RenderWidth = 0;
	uint32_t m_RenderHeight = 0;
	uint32_t m_AntiAliasing = 0;
	uint32_t m_RenderWindow = 0;
	bool m_NativeModels = false;
    VrGraphics::Settings m_Graphics;
    CreatorCamera::State m_CreatorCamera;bool m_CreatorRecording=false,m_CreatorReplay=false;
    std::string m_CreatorTake;
    void CreatorAction(int action);void CreatorMirror(Vector& origin,QAngle& angles,float& fov);
	uint32_t m_RecommendedWidth=0,m_RecommendedHeight=0;
	uint32_t m_UpscaleWidth=0,m_UpscaleHeight=0;
	bool m_UpscaleReady=false;
    bool m_EyeMirrorCandidate=false,m_EyeMirrorFailed=false;
    bool CanReuseEyeMirror() const;
    void ImageBaseline(bool restore);
	float m_CropWidth=1,m_CropHeight=1;
	bool m_GraphicsOpen=false,m_GraphicsDirty=false,m_GraphicsApplyPending=true;
	vr::VROverlayHandle_t m_GraphicsHandle=vr::k_ulOverlayHandleInvalid;
	int m_GraphicsSelected=0,m_ActiveMsaa=0;
	std::string m_GraphicsStatus="Settings saved on this headset.";
	std::string m_LastGraphicsCommandList;
	std::vector<uint32_t> m_GraphicsPixels;
	uint64_t m_PerfStart=1,m_PerfPrevious=0;
	double m_PerfElapsedMs=0;
	std::vector<float> m_PerfIntervals,m_PerfGpu;
	float m_LastEyeRenderMs=0,m_LastMirrorRenderMs=0,m_LastGpuWaitMs=0,m_LastBodyBuildMs=0;
    SceneBookmark::Request m_SceneBookmark;
    ForearmRoll54::History m_ForearmRoll;
    float m_LastUpperCM[2]{},m_LastForearmCM[2]{};
    unsigned m_LastSecondaryViews=0,m_LastViewDepth=0,m_LastAvatarRebases=0,m_LastAvatarCacheHits=0;
	FrameStats::Window m_FrameTimingWindow;
	uint64_t m_FrameTimingPrevious=0,m_FrameTimingStart=0;
	int m_FrameTimingPhase=-1;
	VrMenu::Page m_MenuPage=VrMenu::Home;
	matrix3x4_t m_MenuTransform,m_ModMenuTransform,m_PreviousGrabPose;
	float m_ModMenuWidth=1.3f;
	int m_MenuFocusPanel=0,m_MenuGrabPanel=0;
	bool m_MenuCursorSeen=false,m_MenuLayoutFocused=false;
	bool m_MenuHalfRaster=false,m_MenuUploadLogged=false;
    BodyCalibration::Capture m_BodyCapture;
    uint64_t m_CalibrationPrevious=0;
    int m_CalibrationProgress=-1;
    bool m_HolsterFitHover[2]{};
    uint64_t m_HolsterFitPulse[2]{};
    void UpdateBodyCalibration();
    void UpdateHolsterReachTest();
    bool ApplyBodyMeasurements(const BodyCalibration::Measurements& measured);
    Vector HolsterCenter(int physical);
    float StandingEyeHeightCM();
    matrix3x4_t GetHolsteredGunFrame();
    int m_HolsteredShoulder=1;
    float HolsterRadius(int physical);
    unsigned m_MenuImageFailures=0;
    bool m_MenuFileTransport=true;
    vr::EVROverlayError UploadMenuImageFromFile();
    vr::EVROverlayError UploadOverlayFile(vr::VROverlayHandle_t handle,const std::vector<uint32_t>& pixels,int width,int height,const char* name);
    void OverlayTextureSubmitted(vr::VROverlayHandle_t handle,vr::EVROverlayError result);
	vr::VROverlayHandle_t m_GraphicsStagingHandle=0;
	MenuRaster::Swap m_MenuRaster;
	bool m_ActiveLegacyEffects=false,m_RestartRequested=false;
	MenuPose::StickChord m_MenuStickChord;
	bool m_MenuPoseValid=false,m_MenuWasVisible=false,m_MenuEscapeHeld=false,m_MenuToggleHeld=false;
	bool m_MenuTriggerHeld[2]{},m_MenuGripHeld[2]{};
	int m_MenuGrabHand=-1,m_MenuGrabCorner=-1,m_MenuHoverHand=-1,m_MenuHoveredRow=-1;
	int m_MenuMouseHand=-1;
	float m_MenuGrabWidth=1,m_MenuGrabRadius=1,m_MenuVignetteAlpha=0;
	uint64_t m_MenuLastUpdate=0,m_MenuSuppressUntil=0,m_MenuNavNext=0,m_MenuLastHaptic=0;
	vr::VRActionHandle_t m_MenuEscape=0,m_MenuToggle=0,m_MenuTriggers[2]{},m_MenuGrips[2]{},m_MenuSticks[2]{},m_MenuStickClicks[2]{},m_MenuDpad[4]{},m_MenuVibration[2]{};
	vr::VROverlayHandle_t m_MenuBeam[2]{},m_MenuDot[2]{},m_VignetteHandle=0;
	vr::VROverlayHandle_t m_MenuChrome=0;
	vr::VROverlayHandle_t m_FpsHandle=0;
	uint64_t m_FpsLastUpdate=0;
	unsigned m_FpsFrames=0;
    bool m_FpsImageReady=false;
	std::vector<uint32_t> m_FpsPixels;
	int m_MenuChromeWidth=0,m_MenuChromeHeight=0;
	uint32_t m_NativeMenuHoverPanel=0;
	std::vector<uint32_t> m_MenuChromePixels;
	vr::TrackedDeviceIndex_t m_MenuTipDevice[2]{vr::k_unTrackedDeviceIndexInvalid,vr::k_unTrackedDeviceIndexInvalid};
	vr::HmdMatrix34_t m_MenuTip[2]{};
	uint64_t m_MenuTipRefresh[2]{};
	bool HandleMenuFallback();
	void UpdateMenuInteraction();
	VrFocus::Gate m_SystemFocus;
	bool m_ActionStateFresh=false;
	void RefreshSystemFocus(bool sampleActions);
	void HideOwnedOverlays();
	void ReleaseMenuInput();
	void CloseVrMenus(bool resume);
	void SetMenuPlacement(bool focusMod=false);
	void ApplyMenuPlacement();
	matrix3x4_t& MenuPanelTransform(int panel) {return panel?m_ModMenuTransform:m_MenuTransform;}
	float& MenuPanelWidth(int panel) {return panel?m_ModMenuWidth:m_Graphics.menuWidth;}
	void MenuHaptic(int hand,bool click);
	void UpdateMenuPointer(int hand,const Vector& source,const Vector& destination,bool hit,int panel=0);
	void UpdateComfortVignette();
	void UpdateMenuChrome(int width,int height);
	void UpdateFpsOverlay();
	matrix3x4_t GetHandFrame(bool left);
	matrix3x4_t GetGunControllerFrame(bool contact=true);
	matrix3x4_t GetRawGunControllerFrame();
	float HeightOffset() const {return (m_Graphics.height+(m_Graphics.seated?m_Graphics.seatBoost:0))*m_VRScale/100;}
	void OpenGraphicsMenu(bool focusMod=true);
	void UpdateGraphicsMenu();
	bool RequestPortalRestart();
	void ChangeGraphicsSetting(int row,int direction);
	bool SaveGraphicsSettings();
	void ApplyGraphicsSettings();
	void SamplePerformance();
	void SampleFrameDiagnostics();
	float m_Aspect;
	float m_Fov;

	vr::VRTextureBounds_t m_TextureBounds[2];
	vr::TrackedDevicePose_t m_Poses[vr::k_unMaxTrackedDeviceCount]{};

	Vector m_EyeToHeadTransformPosLeft = { 0,0,0 };
	Vector m_EyeToHeadTransformPosRight = { 0,0,0 };

	Vector m_HmdForward = { 0, 0, 0 };
	Vector m_HmdRight = { 0, 0, 0 };
	Vector m_HmdUp = { 0, 0, 0 };

	Vector m_HmdPosLocalInWorld = { 0,0,0 };

	Vector m_LeftControllerForward = { 0, 0, 0 };
	Vector m_LeftControllerRight = { 0, 0, 0 };
	Vector m_LeftControllerUp = { 0, 0, 0 };

	Vector m_RightControllerForward = { 0, 0, 0 };
	Vector m_RightControllerRight = { 0, 0, 0 };
	Vector m_RightControllerUp = { 0, 0, 0 };
	Vector m_RightHandForward = { 1, 0, 0 };
	Vector m_RightHandUp = { 0, 0, 1 };
	Vector m_LeftHandForward = { 1, 0, 0 };
	Vector m_LeftHandUp = { 0, 0, 1 };

	Vector m_ViewmodelForward = { 0, 0, 0 };
	Vector m_ViewmodelRight = { 0, 0, 0 };
	Vector m_ViewmodelUp = { 0, 0, 0 };

	QAngle m_HmdAngAbs = { 0, 0, 0 };

	Vector m_HmdPosRelativeRaw = { 0,0,0 };
	Vector m_HmdPosRelativeRawPrev = { 0,0,0 };

	Vector m_HmdPosRelative = { 0,0,0 };
	Vector m_HmdPosRelativePrev = { 0,0,0 };

	Vector m_AimPos = { 0, 0, 0 };
	bool m_Traced = false;

	Vector m_Center = { 0,0,0 };
	bool m_CenterPending = true;
	bool m_AutoCalibration = true;
	AutoCalibration::Origin m_CalibrationOrigin;
	AutoCalibration::Drift m_CalibrationDrift;
	AutoCalibration::Stability m_CalibrationStability;
	bool m_CalibrationTracked = false;
	std::uint64_t m_CalibrationTime = 0, m_CalibrationSuppressUntil = 0, m_LastCarryUpdate = 0;
	Vector m_CalibrationPlayerPosition = {0,0,0};
	void UpdateAutoCalibration();
	Vector m_SetupOrigin = { 0,0,0 };
	Vector m_CameraCollisionOffset = { 0,0,0 };
	bool m_CameraBlocked = false;

	float m_HeightOffset = 0.0;
	bool m_RoomscaleActive = false;

	Vector m_LeftControllerPosRel = { 0, 0, 0 };
	QAngle m_LeftControllerAngAbs = { 0, 0, 0 };
	Vector m_RightControllerPosRel = { 0, 0, 0 };
	QAngle m_RightControllerAngAbs = { 0, 0, 0 };

	Vector m_ViewmodelPosOffset = { 0, 0, 0 };
	QAngle m_ViewmodelAngOffset = { 0, 0, 0 };

	Vector m_ViewmodelPosCustomOffset = { 0, 0, 0 }; // Custom (from config) viewmodel position offset applied on top of hardcoded ones
    QAngle m_ViewmodelAngCustomOffset = { 0, 0, 0 }; // Custom (from config) viewmodel angle offset applied on top of hardcoded ones

	float m_Ipd = 0.0f;
	float m_EyeZ = 0.0f;

	Vector m_IntendedPositionOffset = { 0,0,0 };

	enum TextureID
	{
		Texture_None = -1,
		Texture_LeftEye,
		Texture_RightEye,
		Texture_HUD,
		Texture_Blank
	};

	ITexture *m_LeftEyeTexture = nullptr;
	ITexture *m_RightEyeTexture = nullptr;
	ITexture *m_HUDTexture = nullptr;
	ITexture *m_BlankTexture = nullptr;

	IDirect3DSurface9 *m_D9LeftEyeSurface = nullptr;
	IDirect3DSurface9 *m_D9RightEyeSurface = nullptr;
	IDirect3DSurface9 *m_D9HUDSurface = nullptr;
	IDirect3DSurface9 *m_D9BlankSurface = nullptr;

	SharedTextureHolder m_VKLeftEye;
	SharedTextureHolder m_VKRightEye;
	SharedTextureHolder m_VKUpscaledLeft,m_VKUpscaledRight;
	SharedTextureHolder m_VKBackBuffer;
	SharedTextureHolder m_VKHUD;
	SharedTextureHolder m_VKBlankTexture;

	bool m_IsVREnabled = false;
	bool m_IsInitialized = false;
	bool m_RenderedNewFrame = false;
	bool m_RenderedHud = false;
	bool m_CreatedVRTextures = false;
	bool m_DrawCrosshair = false;
    vr::VROverlayHandle_t m_ReticleHandle=0;
    int m_ReticleKey=-1;bool m_ReticleReady=false,m_ReticlePending=false;
    uint64_t m_ReticleRetry=0;
    std::atomic<int> m_LastPortalShot{-1};
    BodyResponse::State m_SecondaryMotion;
    void UpdateCrosshair();
    matrix3x4_t m_ContactRelative[5]{};bool m_ContactValid[5]{};
    uint64_t m_ContactSnapshot=0,m_ContactResetUntil=0;
    float m_GunWallRetraction=0;bool m_GunWallTouch=false;
    Vector m_GunWallDirection{1,0,0};
    Vector m_GunWallOffset{0,0,0};
    bool PortalContactClearance();
    void SnapshotContactPoints();
    void UpdateGunWallContact();
	TextureID m_CreatingTextureID = Texture_None;
	TextureID m_BindingEyeTexture = Texture_None;

	bool m_PressedTurn = false;
	bool m_PushingThumbstick = false;
	bool m_PointerCreated = false;

	// action set
	vr::VRActionSetHandle_t m_ActionSet = 0;
	vr::VRActionSetHandle_t m_BaseActionSet = 0;
	vr::VRActiveActionSet_t m_ActiveActionSet{};

	// actions
	vr::VRActionHandle_t m_ActionJump = 0;
	vr::VRActionHandle_t m_ActionPrimaryAttack = 0;
	vr::VRActionHandle_t m_ActionSecondaryAttack = 0;
	vr::VRActionHandle_t m_ActionReload = 0;
	vr::VRActionHandle_t m_ActionWalk = 0;
	vr::VRActionHandle_t m_ActionTurn = 0;
	vr::VRActionHandle_t m_ActionUse = 0;
	vr::VRActionHandle_t m_ActionNextItem = 0;
	vr::VRActionHandle_t m_ActionPrevItem = 0;
	vr::VRActionHandle_t m_ActionResetPosition = 0;
	vr::VRActionHandle_t m_ActionCrouch = 0;
	vr::VRActionHandle_t m_ActionFlashlight = 0;
	vr::VRActionHandle_t m_ActionActivateVR = 0;
	vr::VRActionHandle_t m_MenuSelect = 0;
	vr::VRActionHandle_t m_MenuBack = 0;
	vr::VRActionHandle_t m_MenuUp = 0;
	vr::VRActionHandle_t m_MenuDown = 0;
	vr::VRActionHandle_t m_MenuLeft = 0;
	vr::VRActionHandle_t m_MenuRight = 0;
	vr::VRActionHandle_t m_Spray = 0;
	vr::VRActionHandle_t m_Scoreboard = 0;
	vr::VRActionHandle_t m_ShowHUD = 0;
	vr::VRActionHandle_t m_Pause = 0;
	vr::VRActionHandle_t m_ActionSkeletonLeft = 0;
	vr::VRActionHandle_t m_ActionSkeletonRight = 0;
	// A relaxed hand is slightly cupped.  Pico's SteamVR bridge does not expose
	// a skeleton stream, so these values are also the procedural fallback until
	// a controller reports real curl data.
	float m_LeftFingerCurl[5] = { 0.42f, 0.50f, 0.56f, 0.52f, 0.46f };
	float m_RightFingerCurl[5] = { 0.42f, 0.50f, 0.56f, 0.52f, 0.46f };
	bool m_LeftSkeletonValid = false;
	bool m_RightSkeletonValid = false;
    bool m_LeftGripPressed = false,m_GripPressed[2]{},m_ShoulderHover[2]{},m_WeaponConsumesGrip=false;
    bool m_UseGripBlocked[2]{};
    uint64_t m_LegacyHapticUntil[2]{},m_LegacyHapticNext[2]{};
    float m_LegacyHapticAmplitude[2]{};
    uint64_t m_WeaponHapticUntil[2]{};float m_WeaponHapticAmplitude[2]{};
    void PumpLegacyHaptics();
    matrix3x4_t GetAvatarHandFrame(bool physicalLeft,bool contact=true);
    HandContact::State m_HandContact[2];
    unsigned long long m_ThrowReleaseUntil=0;
    void UpdateHandContacts();
    void AvatarFingerCurls(bool physicalLeft,float* out);
    bool InteractionAllowed();

    WeaponPolish::Proximity m_GripHover[2];
    vr::VRInputValueHandle_t m_PhysicalInputSource[2]{};
    void ControllerHaptic(int physical,float amplitude,float duration,float frequency,const char* reason);

    WeaponHandling::State m_Weapon;
    matrix3x4_t m_GunFromController{},m_GunWristFromController{};
    Vector m_ActiveGripGunLocal{};
    void UpdateWeaponHandling();
    void WeaponHaptic(int logicalHand,float strength,bool enabled);
    int WeaponHand() const{return m_Weapon.hand;}
    Vector WeaponPosition(bool support=false);
    Vector WeaponForward();
    Vector WeaponUp();
    bool WeaponTracked() const{return m_Weapon.hand==0?m_LeftControllerPose.isValid:m_RightControllerPose.isValid;}
    matrix3x4_t GetCanonicalGunFrame();
    matrix3x4_t GetPhysicalHandFrame(bool physicalLeft);

	vr::VRActionHandle_t m_ActionSupportLeft = 0;
	vr::VRActionHandle_t m_ActionSupportRight = 0;
	bool m_OptionalSupportActive = false;
	OptionalGunGrip::State m_OptionalGripState;
    SupportGrip::Latch m_SupportLatch;
    GunAnimation::State m_GunAnimation;
    GunAnimation::Grip m_AnimatedSupport;
    GunShell::Mesh m_GunShell;
	matrix3x4_t m_SupportFromController{};
	std::uint64_t m_SupportLastSeen = 0;
	bool UpdateOptionalGunSupport(matrix3x4_t *target = nullptr);
	bool m_LeftHandGunGrip = true;
	float m_LeftHandGunGripRadius = 6.0f;

	TrackedDevicePoseData m_HmdPose;
	TrackedDevicePoseData m_LeftControllerPose;
	TrackedDevicePoseData m_RightControllerPose;

	QAngle m_RotationOffset = { 0, 0, 0 };
	bool m_OverrideEyeAngles = false;
	// Snapshot the hand pose at CreateMove time. Server-side pickup callbacks
	// can run between SteamVR action samples, so they must not query live input
	// and accidentally fall back to the HMD pose.
	Vector m_GrabControllerPos = { 0, 0, 0 };
	QAngle m_GrabControllerAng = { 0, 0, 0 };
	matrix3x4_t m_GrabHandRelative;
	bool m_GrabPoseValid = false;
	matrix3x4_t m_PickupAimRelative{};
	bool m_PickupAimValid = false;
	matrix3x4_t m_PortalAimFromController{};
	std::uint64_t m_PortalAimLastSeen = 0;
	bool GetPortalAimRay(Vector& origin, Vector& direction);
	QAngle m_ServerGrabAngles = { 0, 0, 0 };
	void SnapshotGrabPose();
	bool m_UseCommandHeld = false;
    PropHold::Input m_PropHold;int m_PropCarryMode=0;
    matrix3x4_t m_PropVisualRelative{},m_PropShapeRelative{},m_PropVisualFromGrab{};Vector m_PropShapeRadii{1,1,1};
    uint64_t m_PropVisualStamp=0;
    uint64_t m_LastPhysicalPush=0;
    GunHandContact::State m_GunHandContact[2];
    int PropMode() const{return m_PropHold.hand>=0?m_PropCarryMode:(m_Graphics.autoPalmStowed&&m_Weapon.stowed?1:m_Graphics.propGrabMode);}
    bool PropThrows() const{return m_Graphics.enhancedThrow||(m_Graphics.autoPalmStowed&&PropMode()==1);}
    float ForceGain() const{return m_Graphics.sandboxForces?std::clamp(m_Graphics.sandboxForceGain,1.f,10.f):1.f;}
    float ThrowGain() const{return m_Graphics.sandboxForces?std::clamp(m_Graphics.sandboxThrowGain,1.f,5.f):1.f;}
    bool m_PropInputSupported=false;
    // Either physical controller selects props, independently of carry style.
    bool HandControlledProps() const{return m_PropInputSupported;}
    bool PropBlocksGun() const{return m_PropHold.attached&&PropHand()==WeaponHand();}
    int PropHand() const{return HandControlledProps()&&m_PropHold.hand>=0?m_PropHold.hand:m_Weapon.hand;}
    bool PropHandTracked() const{return PropHand()==0?m_LeftControllerPose.isValid:m_RightControllerPose.isValid;}
    void UpdatePropInput();
	std::chrono::steady_clock::time_point m_PrevFrameTime;

	float m_TurnSpeed = 0.15f;
	bool m_SnapTurning = false;
	float m_SnapTurnAngle = 45.0f;
	bool m_LeftHanded = false;
	vr::VRActionSetHandle_t m_LeftActionSet = 0;
	std::unordered_map<vr::VRActionHandle_t,vr::VRActionHandle_t> m_LeftActions;
	std::uint64_t m_HandSwitchSuppressUntil = 0;
	vr::VRActionHandle_t ResolveAction(vr::VRActionHandle_t action) const;
	bool HandleSettingsCommand(const char* command);
	float m_VRScale = 43.2f;
	float m_IpdScale = 1.0f;
	bool m_6DOF = true;
	float m_HudDistance = 1.3f;
	float m_HudSize = 4.0f;
	bool m_HudAlwaysVisible = false;
	int m_AimMode = 2;
	bool m_FirstPersonBody = true;
	bool m_FirstPersonBodyHideUpper = true;
	float m_FirstPersonBodyBackOffset = 8.0f;

	VR() {};
	VR(Game *game);
	int SetActionManifest(const char *fileName);
	void InstallApplicationManifest(const char *fileName);
	void Update();
	void SetScreenSizeOverride(bool bState);
	void CreateVRTextures();
	void SubmitVRTextures();
	void RepositionOverlays();
	void GetPoses();
	void UpdatePosesAndActions();
	void GetViewParameters();
	void ProcessMenuInput();
	void ProcessInput();
	VMatrix VMatrixFromHmdMatrix(const vr::HmdMatrix34_t &hmdMat);
	vr::HmdMatrix34_t VMatrixToHmdMatrix(const VMatrix &vMat);
	vr::HmdMatrix34_t GetControllerTipMatrix(vr::ETrackedControllerRole controllerRole);
	bool CheckOverlayIntersectionForController(vr::VROverlayHandle_t overlayHandle, vr::ETrackedControllerRole controllerRole);
	QAngle GetRightControllerAbsAngle();
	QAngle& GetRightControllerAbsAngleConst();
	Vector GetRightControllerAbsPos(Vector eyePosition = {0, 0, 0});
	Vector GetLeftControllerAbsPos() { return GetRightControllerAbsPos() - m_RightControllerPosRel + m_LeftControllerPosRel; }
	Vector GetRightHandAbsPos() {
		if(m_NativeModels) return NativeHandPosition(false);
		// The controller tracks the grip, while the model anchor is the wrist.
		return GetRightControllerAbsPos() + m_RightControllerForward * (m_ViewmodelPosCustomOffset.x - 2.5f)
			+ m_RightControllerRight * (m_ViewmodelPosCustomOffset.y - 1.0f)
			+ m_RightControllerUp * m_ViewmodelPosCustomOffset.z;
	}
	Vector GetLeftHandAbsPos() {
		if(m_NativeModels) return NativeHandPosition(true);
		return GetLeftControllerAbsPos() - m_LeftControllerForward * 2.5f + m_LeftControllerRight;
	}
	Vector NativeHandPosition(bool left);
	Vector GetRecommendedViewmodelAbsPos(Vector eyePosition);
	QAngle GetRecommendedViewmodelAbsAngle();
	void UpdateHMDAngles();
	void UpdateTracking();
	Vector GetViewAngle();
	Vector GetViewOrigin(Vector setupOrigin);
	void UpdateCameraCollision(Vector setupOrigin);
	Vector GetViewOriginLeft(Vector setupOrigin);
	Vector GetViewOriginRight(Vector setupOrigin);
	bool PressedDigitalAction(vr::VRActionHandle_t &actionHandle, bool checkIfActionChanged = false);
	bool GetAnalogActionData(vr::VRActionHandle_t &actionHandle, vr::InputAnalogActionData_t &analogDataOut);
	void ResetPosition();
	void GetPoseData(vr::TrackedDevicePose_t &poseRaw, TrackedDevicePoseData &poseOut);
	void ParseConfigFile();
	void WaitForConfigUpdate();
	Vector Trace(uint32_t* localPlayer);
	Vector TraceEye(uint32_t* localPlayer, Vector cameraPos, Vector eyePos, QAngle& eyeAngle);
};
