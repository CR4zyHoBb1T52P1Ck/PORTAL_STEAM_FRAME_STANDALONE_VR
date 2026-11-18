#include "hooks.h"
#include "renderpolicy.h"
#include "game.h"
#include "texture.h"
#include "sdk.h"
#include "sdk_server.h"
#include "trace.h"
#include "vr.h"
#include "offsets.h"
#include "portal1.h"
#include "debuglog.h"
#include "handpose.h"
#include "optionalgungrip.h"
#include "gunattachments.h"
#include "guneffects.h"
#include "gunray.h"
#include "portalshotfx.h"
#include "firstpersonbody.h"
#include "cameracollision.h"
#include "portalpose.h"
#include "portalcamera.h"
#include "pickuptrace.h"
#include "nativepose.h"
#include "nativefit.h"
#include "nativefingers.h"
#include "studiomaterial.h"
#include "bodyik.h"
#include "nativephysics.h"
#include "propinteraction.h"
#include "avatarpalette.h"
#include "gungrab.h"
#include "modelpath.h"
#include "modeldrawstate.h"
#include "modelinstance.h"
#include "twohandaim.h"
#include "bindposecache.h"
#include <intrin.h>
#include <iostream>
#include <mutex>

Game *Hooks::m_Game = nullptr;
VR *Hooks::m_VR = nullptr;

Hook<tGetRenderTarget> Hooks::hkGetRenderTarget = {};
Hook<tRenderView> Hooks::hkRenderView = {};
Hook<tPush3DView> Hooks::hkPush3DView = {};
Hook<tPush3DViewDepth> Hooks::hkPush3DViewDepth = {};
ITexture *Hooks::m_ActiveEyeTexture = nullptr;
Hook<tCreateMove> Hooks::hkCreateMove = {};
Hook<tEndFrame> Hooks::hkEndFrame = {};
Hook<tCalcViewModelView> Hooks::hkCalcViewModelView = {};
Hook<tCreateViewModel> Hooks::hkCreateViewModel = {};
Hook<tGetAttachmentMatrix> Hooks::hkGetAttachmentMatrix = {};
Hook<tGetAttachmentAngles> Hooks::hkGetAttachmentAngles = {};
Hook<tPortalGunEffectParameters> Hooks::hkPortalGunEffectParameters = {};
static void *s_RightViewmodelRenderable = nullptr;
static void *s_GunAttachmentRenderable = nullptr;
static const void *s_GunAttachmentModel = nullptr;
static GunAttachments::Model s_GunAttachments;
static float s_DrawnBarrelDirectionError = 0;
static float s_DrawnBarrelLineError = 0;
static float s_NativeGunBasisError = 0;
static CViewSetup s_EyeWorldView;
static thread_local PortalCamera::Frame s_PortalCamera;
static thread_local struct { bool pending=false; matrix3x4_t muzzle; } s_PortalBlast;
static PortalShotFx::LaunchHistory s_PortalLaunches;
static std::mutex s_PortalLaunchMutex;
static void *s_LeftArmRenderable = nullptr;
static bool s_PairedHandMaterialReady=false;
static AvatarPalette::Snapshot s_AvatarPalette;
static uint64_t s_AvatarFrame=0;
static bool s_NativeAvatarDirectDrawn=false;
static matrix3x4_t s_NativeAvatarSource[BodyIK::Max]{};
static bool s_NativeAvatarSourceReady=false;
static void* s_LocalWorldGunRenderable=nullptr;
static void* s_LocalWorldGunEntity=nullptr;
static matrix3x4_t s_LocalWorldGunNativeRoot{},s_LocalWorldGunNativeModel{};
static bool s_LocalWorldGunReady=false;

static void *s_LocalPlayerEntity = nullptr;
static void *s_LocalPlayerRenderable = nullptr;
static bool s_DrawingLocalPlayerBodyDirect = false;
static bool s_ActiveFirstPersonBodyPass = false;
static bool s_InlineBodyDrawEligible = false;
static bool s_BodyDrawTriggered = false;
static bool s_HasLocalPlayerBodyTransform = false;
static Vector s_LocalPlayerBodyOrigin = {0, 0, 0};
static QAngle s_LocalPlayerBodyAngles = {0, 0, 0};
static Vector s_LocalPlayerBodyDrawOffset = {0, 0, 0};
static struct { Vector origin, angles; float zNear, fov; } s_BodyExpectedView{};
static Vector s_BodyCameraCenter = {0, 0, 0};
static std::vector<bool> s_BodyViewStack;

static void ExpectBodyView(const CViewSetup &view)
{
    s_BodyExpectedView.origin = view.origin;
    s_BodyExpectedView.angles = view.angles;
    s_BodyExpectedView.zNear = view.zNear;
    s_BodyExpectedView.fov = view.fov;
}

static bool IsBodyWorldView(const CViewSetup &view, ITexture *target, ITexture *depth)
{
    return s_InlineBodyDrawEligible && !depth
        && (!target || target == Hooks::m_ActiveEyeTexture)
        && !view.m_bOrtho
        && (view.origin - s_BodyExpectedView.origin).LengthSqr() < 0.0001f
        && (view.angles - s_BodyExpectedView.angles).LengthSqr() < 0.0001f
        && fabsf(view.zNear - s_BodyExpectedView.zNear) < 0.001f
        && fabsf(view.fov - s_BodyExpectedView.fov) < 0.001f;
}
Hook<tProcessUsercmds> Hooks::hkProcessUsercmds = {};
Hook<tReadUsercmd> Hooks::hkReadUsercmd = {};
Hook<tWriteUsercmdDeltaToBuffer> Hooks::hkWriteUsercmdDeltaToBuffer = {};
Hook<tWriteUsercmd> Hooks::hkWriteUsercmd = {};
Hook<tAdjustEngineViewport> Hooks::hkAdjustEngineViewport = {};
Hook<tViewport> Hooks::hkViewport = {};
Hook<tGetViewport> Hooks::hkGetViewport = {};
Hook<tGetPrimaryAttackActivity> Hooks::hkGetPrimaryAttackActivity = {};
Hook<tDrawModelExecute> Hooks::hkDrawModelExecute = {};
Hook<tPushRenderTargetAndViewport> Hooks::hkPushRenderTargetAndViewport = {};
Hook<tPopRenderTargetAndViewport> Hooks::hkPopRenderTargetAndViewport = {};
Hook<tVgui_Paint> Hooks::hkVgui_Paint = {};
Hook<tIsSplitScreen> Hooks::hkIsSplitScreen = {};
Hook<tPrePushRenderTarget> Hooks::hkPrePushRenderTarget = {};
Hook<tGetFullScreenTexture> Hooks::hkGetFullScreenTexture = {};
Hook<tEyePosition> Hooks::hkEyePosition = {};
Hook<tWeapon_ShootPosition> Hooks::hkWeapon_ShootPosition = {};
Hook<tTraceFirePortal> Hooks::hkTraceFirePortal = {};
Hook<tDispatchEffect> Hooks::hkDispatchEffect = {};
Hook<tPortalBlastCallback> Hooks::hkPortalBlastCallback = {};
Hook<tSettingsClientCmd> Hooks::hkSettingsClientCmd = {},Hooks::hkSettingsClientCmdUnrestricted = {};

Hook<tGetModeHeight> Hooks::hkGetModeHeight = {};
Hook<tDrawSelf> Hooks::hkDrawSelf = {};
Hook<tClipTransform> Hooks::hkClipTransform = {};
Hook<tPlayerPortalled> Hooks::hkPlayerPortalled = {};
Hook<tPlayerUse> Hooks::hkPlayerUse = {};
Hook<tFindUseEntity> Hooks::hkFindUseEntity = {};
Hook<tVGui_GetHudBounds> Hooks::hkVGui_GetHudBounds = {};
Hook<tVGui_GetPanelBounds> Hooks::hkVGui_GetPanelBounds = {};
Hook<tVGUI_UpdateScreenSpaceBounds> Hooks::hkVGUI_UpdateScreenSpaceBounds = {};
Hook<tVGui_GetTrueScreenSize> Hooks::hkVGui_GetTrueScreenSize = {};
Hook<tSetBounds> Hooks::hkSetBounds = {};
Hook<tGetScreenSize> Hooks::hkGetScreenSize = {};
Hook<tPush2DView> Hooks::hkPush2DView = {};
Hook<tPopView> Hooks::hkPopView = {};
Hook<tRender> Hooks::hkRender = {};
Hook<tGetClipRect> Hooks::hkGetClipRect = {};
Hook<tGetHudSize> Hooks::hkGetHudSize = {};
Hook<tSetSize> Hooks::hkSetSize = {};

Hook<tComputeError> Hooks::hkComputeError = {};
Hook<tUpdateObject> Hooks::hkUpdateObject = {};
Hook<tUpdateObjectVM> Hooks::hkUpdateObjectVM = {};
Hook<tRotateObject> Hooks::hkRotateObject = {};
Hook<tEyeAngles> Hooks::hkEyeAngles = {};

Hook<tMatrixBuildPerspectiveX> Hooks::hkMatrixBuildPerspectiveX = {};
Hook<tGetDefaultFOV> Hooks::hkGetDefaultFOV = {};
Hook<tGetFOV> Hooks::hkGetFOV = {};
Hook<tGetViewModelFOV> Hooks::hkGetViewModelFOV = {};

Hook<tSetDrawOnlyForSplitScreenUser> Hooks::hkSetDrawOnlyForSplitScreenUser = {};
Hook<tClientThink> Hooks::hkClientThink = {};
Hook<tPrecache> Hooks::hkPrecache = {};
Hook<tCHudCrosshair_ShouldDraw> Hooks::hkCHudCrosshair_ShouldDraw = {};
Hook<tCWeaponPortalgun_FirePortal> Hooks::hkCWeaponPortalgun_FirePortal = {};

int Hooks::m_PushHUDStep = 0;
bool Hooks::m_PushedHud = false;

tCreatePingPointer Hooks::CreatePingPointer = nullptr;
tPrecacheParticleSystem Hooks::PrecacheParticleSystem = nullptr;

tUTIL_Portal_FirstAlongRay Hooks::UTIL_Portal_FirstAlongRay = nullptr;
tUTIL_IntersectRayWithPortal Hooks::UTIL_IntersectRayWithPortal = nullptr;
tUTIL_Portal_AngleTransform Hooks::UTIL_Portal_AngleTransform = nullptr;
tGetOwner Hooks::GetOwner = nullptr;
tGetFullScreenTexture Hooks::GetFullScreenTexture = nullptr;

namespace
{
	thread_local void* s_CarryPlayer = nullptr;
	struct ScopedCarryPlayer {
		void* previous = s_CarryPlayer;
		explicit ScopedCarryPlayer(void* player) { s_CarryPlayer=player; }
		~ScopedCarryPlayer() { s_CarryPlayer=previous; }
	};
	struct PickupQueryPose { void* player; Vector origin; QAngle angles; };
	thread_local PickupQueryPose* s_PickupQuery = nullptr;
	struct ScopedPickupQuery {
		PickupQueryPose* previous;
		explicit ScopedPickupQuery(PickupQueryPose& query) : previous(s_PickupQuery) { s_PickupQuery = &query; }
		~ScopedPickupQuery() { s_PickupQuery = previous; }
	};

	bool ServerHandPose(void *player, Vector &position, QAngle &angles)
	{
		auto *vr = Hooks::m_VR;
		if (!vr || !vr->m_GrabPoseValid || !Hooks::hkEyePosition.isCreated()
			|| !Hooks::hkEyeAngles.isCreated()) return false;
		Vector eye;
		const Vector *actualEye = Hooks::hkEyePosition.fOriginal(player, &eye);
		if (!actualEye) return false;
		const auto hand = PortalPose::WorldHand(vr->m_GrabHandRelative,
			*actualEye, Hooks::hkEyeAngles.fOriginal(player));
		position = PortalPose::Position(hand);
		angles = PortalPose::Angles(hand);
		return true;
	}

	bool ServerPickupAim(void *player, Vector &position, QAngle &angles)
	{
		auto *vr = Hooks::m_VR;
		if (!vr || !vr->m_PickupAimValid || !Hooks::hkEyePosition.isCreated()
			|| !Hooks::hkEyeAngles.isCreated()) return false;
		Vector eye;
		const auto *actualEye = Hooks::hkEyePosition.fOriginal(player,&eye);
		if (!actualEye) return false;
		const auto aim = PortalPose::WorldHand(vr->m_PickupAimRelative,
			*actualEye,Hooks::hkEyeAngles.fOriginal(player));
		position = PortalPose::Position(aim);
		angles = PortalPose::Angles(aim);
		return true;
	}

	int ServerEntityIndex(void *entity)
	{
		const uintptr_t target = SigScanner::GetVirtualFunction(entity,
			Portal1::VTableIndex::kServerEntity_GetRefEHandle);
		if (!target) return -1;
		using GetHandleFn = const unsigned int &(__thiscall *)(void *);
		const unsigned int handle = reinterpret_cast<GetHandleFn>(target)(entity);
		return handle == 0xffffffffu ? -1 : static_cast<int>(handle & 0xfffu);
	}

    NativePhysics::Binding s_PropBinding;
    using SetPropTarget=void(__thiscall*)(void*,const Vector&,const QAngle&);
    using DetachProp=void(__thiscall*)(void*,bool);
    Hook<SetPropTarget> s_SetPropTarget;
    Hook<DetachProp> s_DetachProp;
    PropInteraction::Carry s_PropCarry;
    void* s_PropPlayer=nullptr;
    void* s_PropController=nullptr;
    thread_local bool s_GripRelease=false;
    struct PropReleaseVelocity {
        bool ready=false;unsigned handle=0xffffffff;void* entity=nullptr;
        Vector linear{0,0,0},angular{0,0,0};
    };
    thread_local PropReleaseVelocity s_ReleaseVelocity;
    void ApplyPropReleaseVelocity() {
        const auto pending=s_ReleaseVelocity;s_ReleaseVelocity={};
        if(!pending.ready||s_PropBinding.Entity(pending.handle)!=pending.entity)return;
        const bool applied=NativePhysics::Velocity(NativePhysics::Physics(pending.entity),pending.linear,pending.angular);
        auto* vr=Hooks::m_VR;
        PortalVrLog("Grip release throw applied=%d entity=%p speedMPS=%f spinDPS=%f",applied,pending.entity,sqrtf(pending.linear.LengthSqr())/vr->m_VRScale,sqrtf(pending.angular.LengthSqr()));
    }
    thread_local void* s_PropUpdate=nullptr;
    static double PropTime(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
    bool PropHandPose(matrix3x4_t& pose) {
        Vector p;QAngle a;
        if(!s_PropPlayer||!Hooks::m_VR->InteractionAllowed()||!ServerHandPose(s_PropPlayer,p,a))return false;
        pose=PortalPose::Frame(p,a);return PropInteraction::Valid(pose);
    }
    void __fastcall SetPropTargetHook(void* controller,void*,const Vector& position,const QAngle& angles) {
        auto* vr=Hooks::m_VR;
        if(controller==s_PropUpdate&&vr&&vr->m_Graphics.propGrabMode==1&&vr->InteractionAllowed()) {
            // Native portal carry has a separate transformed target. Preserve
            // it while the object is on the other side of a linked portal.
            const auto portalFlag=reinterpret_cast<uintptr_t>(controller)+0x12c1;
            const bool across=SigScanner::IsReadable(portalFlag,1)&&*reinterpret_cast<unsigned char*>(portalFlag);
            void* entity=s_PropBinding.Attached(controller);matrix3x4_t hand,object;
            if(!across&&entity&&PropHandPose(hand)&&NativePhysics::Position(NativePhysics::Physics(entity),object)) {
                const auto target=s_PropCarry.Target(controller,entity,1,vr->PropHand(),PropTime(),hand,object,vr->m_VRScale);
                s_SetPropTarget.fOriginal(controller,PortalPose::Position(target),PortalPose::Angles(target));return;
            }
            s_PropCarry.Reset();
        }
        s_SetPropTarget.fOriginal(controller,position,angles);
    }
    void __fastcall DetachPropHook(void* controller,void*,bool clearVelocity) {
        auto* vr=Hooks::m_VR;void* entity=s_PropBinding.Attached(controller);
        unsigned handle=0xffffffff;
        Vector linear{0,0,0},angular{0,0,0};matrix3x4_t object,hand;
        const auto now=GetTickCount64();
        bool apply=s_GripRelease&&vr&&vr->m_PropHold.throwRelease&&vr->m_Graphics.enhancedThrow&&vr->m_ThrowReleaseUntil&&now<=vr->m_ThrowReleaseUntil
            &&vr->InteractionAllowed()&&entity&&controller==s_PropCarry.controller&&entity==s_PropCarry.entity
            &&vr->PropHand()==s_PropCarry.hand&&PropHandPose(hand);
        if(apply) {
            s_PropCarry.motion.Push(PropTime(),hand,vr->m_VRScale);
            const auto get=SigScanner::GetVirtualFunction(entity,Portal1::VTableIndex::kServerEntity_GetRefEHandle);
            if(get)handle=reinterpret_cast<const unsigned&(__thiscall*)(void*)>(get)(entity);
            apply=s_PropCarry.motion.Velocity(PropTime(),linear,angular)&&NativePhysics::Position(NativePhysics::Physics(entity),object);
        }
        // Let native code restore mass, damping, collision flags and controller
        // ownership first. Never retain or call a stale IPhysicsObject pointer.
        s_DetachProp.fOriginal(controller,clearVelocity);
        if(apply&&s_PropBinding.Entity(handle)==entity) {
            linear=PropInteraction::CenterVelocity(linear,angular,PortalPose::Position(hand),PortalPose::Position(object));
            linear=PropInteraction::Limit(linear*vr->m_Graphics.throwGain,vr->m_VRScale*vr->m_Graphics.throwMaxMPS);
            angular=PropInteraction::Limit(angular*(vr->m_Graphics.throwSpin/100),1440);
            const Vector local{TwoHandAim::Dot(angular,TwoHandAim::Axis(object,0)),TwoHandAim::Dot(angular,TwoHandAim::Axis(object,1)),TwoHandAim::Dot(angular,TwoHandAim::Axis(object,2))};
            // Defer until PickupController::Shutdown has also completed its
            // native use-entity, weapon and ownership cleanup.
            s_ReleaseVelocity={true,handle,entity,linear,local};
        }
        if(controller==s_PropCarry.controller){s_PropCarry.Reset();s_PropPlayer=nullptr;if(vr)vr->m_ThrowReleaseUntil=0;}
        if(controller==s_PropController){s_PropController=nullptr;if(vr)vr->m_PropHold.Detached();}
    }
	constexpr bool kEnableAllHooks = true;
	constexpr bool kEnableClientModeHooks = true;

	template <typename T>
	bool CreateHookAt(Hook<T> &hook, uintptr_t address, LPVOID detour, const char *name, bool required)
	{
		if (!address)
		{
			if (required)
			{
				std::string error = "Required hook target not found: ";
				error += name;
				Game::errorMsg(error.c_str());
			}
			return false;
		}

		return hook.createHook(reinterpret_cast<LPVOID>(address), detour) == 0;
	}

	template <typename T>
	void EnableIfCreated(Hook<T> &hook)
	{
		if (hook.isCreated())
			hook.enableHook();
	}

	uintptr_t FindPortalPlayerFunction(size_t index)
	{
		// CPortal_Player owns the complete object vtable in Portal 1, but the
		// two accessors used by the pickup code are inherited from CBasePlayer.
		// Try the derived table first and retain the base-table fallback for
		// builds whose RTTI is stripped or laid out differently.
		for (const char *typeName : { ".?AVCPortal_Player@@", ".?AVCBasePlayer@@" })
		{
			const uintptr_t function = SigScanner::FindRttiVtableFunction(
				"server.dll", typeName, index);
			if (function)
				return function;
		}
		return 0;
	}

	struct StudioBodyView
	{
		const unsigned char *header = nullptr;
		int count = 0;
		int boneOffset = 0;
		int modelLength = 0;
	};

	bool GetStudioBodyView(void *state, StudioBodyView &view)
	{
		if (!state || !SigScanner::IsReadable(reinterpret_cast<uintptr_t>(state), sizeof(void *)))
			return false;

		const auto *header = *reinterpret_cast<const unsigned char * const *>(state);
		if (!header || !SigScanner::IsReadable(reinterpret_cast<uintptr_t>(header), 164))
			return false;

		const int modelLength = *reinterpret_cast<const int *>(header + 76);
		const int count = *reinterpret_cast<const int *>(header + 156);
		const int boneOffset = *reinterpret_cast<const int *>(header + 160);
		if (modelLength <= 0 || count <= 0 || count > FirstPersonBody::MaxBones
			|| boneOffset < 164 || boneOffset > modelLength
			|| modelLength - boneOffset < count * 216
			|| !SigScanner::IsReadable(reinterpret_cast<uintptr_t>(header + boneOffset), count * 216))
			return false;

		view.header = header;
		view.count = count;
		view.boneOffset = boneOffset;
		view.modelLength = modelLength;
		return true;
	}

	const char *GetStudioBoneName(const StudioBodyView &view, int index)
	{
		if (index < 0 || index >= view.count)
			return nullptr;

		const auto *bone = view.header + view.boneOffset + index * 216;
		const int nameOffset = *reinterpret_cast<const int *>(bone);
		if (nameOffset <= 0 || nameOffset >= view.modelLength - (bone - view.header))
			return nullptr;

		const auto *name = bone + nameOffset;
		return SigScanner::IsReadable(reinterpret_cast<uintptr_t>(name), 1)
			? reinterpret_cast<const char *>(name) : nullptr;
	}

	int FindStudioBone(const StudioBodyView &view, const char *wanted)
	{
		if (!wanted)
			return -1;

		for (int i = 0; i < view.count; ++i)
		{
			const char *name = GetStudioBoneName(view, i);
			if (name && !_stricmp(name, wanted))
				return i;
		}

		return -1;
	}

	bool IsPlayerModelName(const char *name)
	{
		if (!name)
			return false;

		if (!_strnicmp(name, "models/", 7))
			name += 7;

		// This intentionally accepts any player model, including the custom
		// Chell model supplied by the VPK.  It does not replace or rewrite the
		// model; it only identifies the local player's normal draw call.
		return !_strnicmp(name, "player/", 7);
	}

	bool IsLocalPlayerBody(const ModelRenderInfo_t &info)
	{
		if (!Hooks::m_Game || !Hooks::m_VR || !s_ActiveFirstPersonBodyPass
			|| !Hooks::m_VR->m_IsVREnabled || !Hooks::m_VR->m_FirstPersonBody
            || !info.pModel)
			return false;

		IModelInfo *modelInfo = Hooks::m_Game->GetModelInfo();
		const char *modelName = modelInfo ? modelInfo->GetModelName(info.pModel) : nullptr;
		if (!IsPlayerModelName(modelName))
			return false;

        if(!s_DrawingLocalPlayerBodyDirect&&!(Hooks::m_VR->m_Graphics.bodyIK&&Hooks::m_VR->m_Graphics.bodyPortalPose))return false;
		const int localIndex = Hooks::m_Game->GetLocalPlayerIndex();
		return (localIndex > 0 && info.entity_index == localIndex)
			|| info.pRenderable == s_LocalPlayerRenderable
			|| info.pRenderable == s_LocalPlayerEntity;
	}

	bool TranslateFirstPersonBodyBones(void *state, const matrix3x4_t *source,
		matrix3x4_t *result)
	{
		StudioBodyView view;
		if (!source || !result || !GetStudioBodyView(state, view)
			|| !SigScanner::IsReadable(reinterpret_cast<uintptr_t>(source), view.count * sizeof(matrix3x4_t)))
			return false;

		memcpy(result, source, view.count * sizeof(matrix3x4_t));
		if (!s_DrawingLocalPlayerBodyDirect) return true;
		// Align the model's eye midpoint under the center camera, never one
		// stereo eye. Horizontal correction preserves the animated foot height.
		Vector offset = s_LocalPlayerBodyDrawOffset;
		bool hasEyeAttachment = false;
		// Procedural QC eyes are skinned to the head, so optional eye bones
		// may no longer be evaluated. Use the model's authored eyes attachment.
		if (view.modelLength >= 248 && SigScanner::IsReadable(
			reinterpret_cast<uintptr_t>(view.header), view.modelLength)) {
			const int attachmentCount = *reinterpret_cast<const int *>(view.header + 240);
			const int attachmentOffset = *reinterpret_cast<const int *>(view.header + 244);
			if (attachmentCount > 0 && attachmentCount <= 256 && attachmentOffset >= 248
				&& attachmentOffset <= view.modelLength - attachmentCount * 92) {
				for (int i = 0; i < attachmentCount; ++i) {
					const int at = attachmentOffset + i * 92;
					const auto *attachment = view.header + at;
					const int nameOffset = *reinterpret_cast<const int *>(attachment);
					const int bone = *reinterpret_cast<const int *>(attachment + 8);
					if (nameOffset <= 0 || nameOffset > view.modelLength - at - 5
						|| bone < 0 || bone >= view.count
						|| memcmp(attachment + nameOffset, "eyes", 5)) continue;
					const auto &local = *reinterpret_cast<const matrix3x4_t *>(attachment + 12);
					const auto world = HandPose::Concat(source[bone], local);
					const Vector eyes(world[0][3], world[1][3], world[2][3]);
					if (std::isfinite(eyes.x) && std::isfinite(eyes.y) && std::isfinite(eyes.z)) {
						offset = FirstPersonBody::HorizontalCameraOffset(eyes, s_BodyCameraCenter);
						hasEyeAttachment = true;
					}
					break;
				}
			}
		}
		const int leftEye = FindStudioBone(view, "LeftEye");
		const int rightEye = FindStudioBone(view, "RightEye");
		if (!hasEyeAttachment && leftEye >= 0 && rightEye >= 0) {
			const Vector eyes((source[leftEye][0][3] + source[rightEye][0][3]) * 0.5f,
				(source[leftEye][1][3] + source[rightEye][1][3]) * 0.5f,
				(source[leftEye][2][3] + source[rightEye][2][3]) * 0.5f);
			offset = FirstPersonBody::HorizontalCameraOffset(eyes, s_BodyCameraCenter);
		}
		// Set the first-person torso behind the camera without moving hands,
		// collision, or the complete player model seen through portals.
		const auto bodyAngles=s_PortalCamera.Unmap(QAngle(s_BodyExpectedView.angles.x,
			s_BodyExpectedView.angles.y,s_BodyExpectedView.angles.z));
		offset += FirstPersonBody::BackwardOffset(bodyAngles.y,
			Hooks::m_VR->m_FirstPersonBodyBackOffset);
		for (int i = 0; i < view.count; ++i) {
			result[i][0][3] += offset.x;
			result[i][1][3] += offset.y;
		}

		return true;
	}

	bool BuildFirstPersonBodyBones(void *state, const matrix3x4_t *source,
		matrix3x4_t *result, const ModelRenderInfo_t &)
	{
		StudioBodyView view;
		if (!source || !result || !GetStudioBodyView(state, view)
			|| !TranslateFirstPersonBodyBones(state, source, result))
			return false;

        if(Hooks::m_VR->m_Graphics.bodyIK) {
            static BodyIK::Rig rig;static const unsigned char* header=nullptr;static int length=0,checksum=0;static bool valid=false;
            const int currentChecksum=*reinterpret_cast<const int*>(view.header+8);
            if(header!=view.header||length!=view.modelLength||checksum!=currentChecksum) {
                header=view.header;length=view.modelLength;checksum=currentChecksum;
                valid=view.modelLength<=64*1024*1024&&SigScanner::IsReadable(reinterpret_cast<uintptr_t>(view.header),view.modelLength)&&rig.Read(view.header,view.modelLength);
                s_AvatarPalette.valid=false;
                PortalVrLog("Native whole-body rig valid=%d bones=%d leftWrist=%d rightWrist=%d",valid,view.count,rig.arm[0][2],rig.arm[1][2]);
            }
            if(valid) {
                const auto& settings=Hooks::m_VR->m_Graphics;
                if(!s_AvatarPalette.Current(s_AvatarFrame,header)) {
                    // The source palette was frozen before any portal draw.
                    // Never request bone recomputation while a model is drawing.
                    static thread_local bool building=false;
                    if(building)return false;
                    struct Guard{bool& v;Guard(bool& b):v(b){v=true;}~Guard(){v=false;}} guard(building);
                    const matrix3x4_t* native=s_NativeAvatarSourceReady?s_NativeAvatarSource:source;
                    if(!s_NativeAvatarSourceReady&&!s_DrawingLocalPlayerBodyDirect)return false;
                    matrix3x4_t posed[BodyIK::Max];
                    std::memcpy(posed,native,view.count*sizeof(matrix3x4_t));
                    const auto bodyAngles=s_PortalCamera.Unmap(QAngle(s_BodyExpectedView.angles.x,s_BodyExpectedView.angles.y,s_BodyExpectedView.angles.z));
                    const bool fitEnabled=settings.bodyFit&&BodyCalibration::Valid({settings.bodyEyeCM,settings.bodySpanCM,settings.bodyWidthCM,settings.bodyDropCM});
                    const auto offset=FirstPersonBody::HorizontalCameraOffset(rig.Eyes(native),s_BodyCameraCenter)
                        +(fitEnabled?Vector(0,0,0):FirstPersonBody::BackwardOffset(bodyAngles.y,Hooks::m_VR->m_FirstPersonBodyBackOffset));
                    for(int i=0;i<view.count;++i){posed[i][0][3]+=offset.x;posed[i][1][3]+=offset.y;}
                    const matrix3x4_t hands[2]={Hooks::m_VR->GetAvatarHandFrame(true),Hooks::m_VR->GetAvatarHandFrame(false)};
                    float contactCurls[2][5];Hooks::m_VR->AvatarFingerCurls(true,contactCurls[0]);Hooks::m_VR->AvatarFingerCurls(false,contactCurls[1]);
                    const float* curls[2]={contactCurls[0],contactCurls[1]};
                    const bool primaryLeft=(Hooks::m_VR->WeaponHand()==0)!=Hooks::m_VR->m_LeftHanded;
                    const bool gripping[2]={!Hooks::m_VR->m_Weapon.stowed&&(primaryLeft||Hooks::m_VR->m_OptionalSupportActive),
                        !Hooks::m_VR->m_Weapon.stowed&&(!primaryLeft||Hooks::m_VR->m_OptionalSupportActive)};
                    const bool fitted=fitEnabled;
                    const bool palm=settings.controllerHands&&settings.handAnchor==1;
                    const float handScale=settings.bodyVolume?settings.handSize/100:1,wristScale=settings.bodyVolume?settings.wristWidth/100:1;
                    const float fittedReach=fitted?rig.FitUpperBody(posed,s_BodyCameraCenter,Hooks::m_VR->m_HmdForward,Hooks::m_VR->m_VRScale,
                        settings.bodySpanCM,settings.bodyWidthCM,settings.bodyDropCM,settings.bodyNeckBackCM,settings.bodyYaw,settings.bodyArmScale,Hooks::m_VR->StandingEyeHeightCM()-settings.bodyEyeCM,settings.bodyUpright,palm,handScale,settings.palmDepth/100,settings.rigidTorso,settings.torsoHeight/100,settings.bodyClearanceCM):settings.bodyArmScale;
                    const auto headTarget=TwoHandAim::Position(posed[rig.head])+(fitted?Vector(0,0,0):(s_BodyCameraCenter-rig.Eyes(posed)));
                    const auto headPose=PortalPose::Frame(s_BodyCameraCenter,Hooks::m_VR->m_HmdAngAbs);
                    if(!rig.SolveNative(posed,headTarget,hands,Hooks::m_VR->m_HmdForward,fittedReach,settings.bodyIKInfluence,
                        settings.bodyAutoReach,settings.fingerTracking?curls:nullptr,gripping,&headPose,settings.fingerRange/100,settings.wristTwist/100,palm,handScale,wristScale,settings.palmDepth/100,fitted&&settings.rigidTorso))return false;
                    if(settings.bodyVolume)rig.Volume(posed,settings.upperArmWidth/100,settings.forearmWidth/100,settings.wristWidth/100,settings.handSize/100,settings.torsoWidth/100,settings.wristCuff/100);
                    s_AvatarPalette.Store(s_AvatarFrame,header,native[0],posed,view.count);
                    static unsigned audits=0;
                    if((audits++%600)==0)PortalVrLog("Whole-body pose serial=%llu bones=%d leftWristError=%f rightWristError=%f autoReach=%d influence=%f",s_AvatarFrame,view.count,
                        sqrtf((TwoHandAim::Position(posed[rig.arm[0][2]])-TwoHandAim::Position(hands[0])).LengthSqr()),
                        sqrtf((TwoHandAim::Position(posed[rig.arm[1][2]])-TwoHandAim::Position(hands[1])).LengthSqr()),settings.bodyAutoReach,settings.bodyIKInfluence);
                    if((audits%600)==1&&palm)for(int h=0;h<2;++h) {
                        Vector grip;for(int r=0;r<3;++r)grip[r]=posed[rig.arm[h][2]][r][3]+(posed[rig.arm[h][2]][r][0]*rig.gripLocal[h].x+posed[rig.arm[h][2]][r][1]*rig.gripLocal[h].y+posed[rig.arm[h][2]][r][2]*rig.gripLocal[h].z)*(settings.palmDepth/100);
                        PortalVrLog("Palm grip target side=%d error=%f",h,sqrtf((grip-TwoHandAim::Position(hands[h])).LengthSqr()));
                    }
                }
                if(!s_AvatarPalette.Current(s_AvatarFrame,header))return false;
                s_AvatarPalette.Copy(result,source[0],s_DrawingLocalPlayerBodyDirect);
                // First-person-only visibility. Portal copies retain the entire
                // same solved Chell, including native wrists, fingers and head.
                if(s_DrawingLocalPlayerBodyDirect) {
                    int roots[3],n=0;if(Hooks::m_VR->m_FirstPersonBodyHideUpper)roots[n++]=rig.neck;
                    const bool primaryLeft=(Hooks::m_VR->WeaponHand()==0)!=Hooks::m_VR->m_LeftHanded;
                    for(int h=0;h<2;++h) {
                        const bool logicalLeft=(h==0)!=Hooks::m_VR->m_LeftHanded;
                        bool visible=settings.arms!=2&&(logicalLeft?settings.leftArm:settings.rightArm);
                        if(Hooks::m_VR->m_OptionalSupportActive&&settings.showSupportHand&&(h==0)!=primaryLeft)visible=true;
                        if(Hooks::m_VR->m_Weapon.stowed&&settings.holsterHands)visible=true;
                        if(!visible)roots[n++]=rig.arm[h][0];
                    }
                    FirstPersonBody::CollapseBranchesAtRoots(result,rig.parents,rig.count,roots,n);
                    // Full arms retains every authored skin weight. Hands-only
                    // remains an explicit visibility choice, never an IK seam.
                    if(settings.arms==1)for(int h=0;h<2;++h) {
                        const auto wrist=TwoHandAim::Position(result[rig.arm[h][2]]);
                        const auto point=HandPose::Frame({0,0,0},{0,0,0},{0,0,0},wrist);
                        for(int i=0;i<rig.count;++i)if(rig.below[rig.arm[h][0]][i]&&!rig.below[rig.arm[h][2]][i])result[i]=point;
                    }
                    s_NativeAvatarDirectDrawn=true;
                }
                return true;
            }
        }

		int parents[FirstPersonBody::MaxBones]{};
		for (int i = 0; i < view.count; ++i)
		{
			const auto *bone = view.header + view.boneOffset + i * 216;
			parents[i] = *reinterpret_cast<const int *>(bone + 4);
		}

		int hiddenRoots[3]{};
		int hiddenRootCount = 0;
		// Keep the chest and upper torso visible when looking down. Hide only
		// the head and untracked arms, closing each at its own attachment.
		for (const char *candidate : {"neck", "clavicle_L", "clavicle_R"})
		{
			const int root = FindStudioBone(view, candidate);
			if (root >= 0 && hiddenRootCount < 3)
				hiddenRoots[hiddenRootCount++] = root;
		}

		if (hiddenRootCount == 0)
			return s_DrawingLocalPlayerBodyDirect
				&& s_LocalPlayerBodyDrawOffset.LengthSqr() > 0.0001f;

		FirstPersonBody::CollapseBranchesAtRoots(result, parents, view.count,
			hiddenRoots, hiddenRootCount);
		return true;
	}

	bool GetRenderableTransform(void *renderable, Vector &origin, QAngle &angles)
	{
		if (!renderable)
			return false;

		const uintptr_t originTarget = SigScanner::GetVirtualFunction(
			renderable, Portal1::VTableIndex::kClientRenderable_GetRenderOrigin);
		const uintptr_t anglesTarget = SigScanner::GetVirtualFunction(
			renderable, Portal1::VTableIndex::kClientRenderable_GetRenderAngles);
		if (!originTarget || !anglesTarget)
			return false;

		using GetVectorFn = const Vector &(__thiscall *)(void *);
		using GetAnglesFn = const QAngle &(__thiscall *)(void *);
		const Vector &renderOrigin = reinterpret_cast<GetVectorFn>(originTarget)(renderable);
		const QAngle &renderAngles = reinterpret_cast<GetAnglesFn>(anglesTarget)(renderable);
		if (!std::isfinite(renderOrigin.x) || !std::isfinite(renderOrigin.y)
			|| !std::isfinite(renderOrigin.z) || !std::isfinite(renderAngles.x)
			|| !std::isfinite(renderAngles.y) || !std::isfinite(renderAngles.z))
			return false;

		origin = renderOrigin;
		angles = renderAngles;
		return true;
	}

	int DrawLocalPlayerBodyDirect()
	{
		if (!Hooks::m_Game || !Hooks::m_VR || !s_ActiveFirstPersonBodyPass
            || !Hooks::m_VR->m_IsVREnabled || !Hooks::m_VR->m_FirstPersonBody || !Hooks::m_Game->IsInGame())
			return 0;
		// The first-person copy is useful below the headset, but must never
		// obscure forward aim. Portal/mirror world models keep their usual draw.
		const auto bodyAngles=s_PortalCamera.Unmap(QAngle(s_BodyExpectedView.angles.x,
			s_BodyExpectedView.angles.y,s_BodyExpectedView.angles.z));
		if (!Hooks::m_VR->m_Graphics.bodyIK&&!FirstPersonBody::LookingDown(bodyAngles.x))
			return 0;

		void *player = Hooks::m_Game->GetLocalPortalPlayer();
		if (!player)
			return 0;

		// Portal keeps the renderable interface in the secondary subobject used
		// by the existing viewmodel path. GetModel is safe to call here, but its
		// DrawModel method still applies the first-person visibility rejection.
		// Use the engine model-render interface for the actual submission so the
		// normal model, animation, materials, and lighting remain engine-owned.
		void *renderable = static_cast<unsigned char *>(player) + 4;
		const uintptr_t getModelTarget = SigScanner::GetVirtualFunction(
			renderable, Portal1::VTableIndex::kClientRenderable_GetModel);
		if (!getModelTarget)
			return 0;

		using GetModelFn = model_t *(__thiscall *)(void *);
		model_t *model = reinterpret_cast<GetModelFn>(getModelTarget)(renderable);
		if (!model)
			return 0;

		IModelRender *modelRender = Hooks::m_Game->GetModelRender();
		if (!modelRender)
			return 0;

		// Read the renderable's live transform first. This remains valid even
		// when Source suppresses the normal first-person player draw, and follows
		// crouching, portals, and elevators without a guessed eye-height offset.
		// The ModelRenderInfo transform is the safe fallback for engines that do
		// not expose these renderable accessors.
		Vector origin = Hooks::m_VR->m_SetupOrigin - Vector(0, 0, 64.0f);
		QAngle angles = Hooks::m_VR->m_HmdAngAbs;
		QAngle renderAngles;
		if (GetRenderableTransform(renderable, origin, renderAngles))
		{
			// Keep the model transform paired with the live bone palette. The HMD
			// can look independently of the player's body; rotating the model to
			// HMD yaw while retaining Source's bones makes the mesh shear/drift.
			angles = renderAngles;
		}
		else if (s_HasLocalPlayerBodyTransform)
		{
			origin = s_LocalPlayerBodyOrigin;
			angles = s_LocalPlayerBodyAngles;
		}

		// The camera moves by the tracked HMD displacement while Source's player
		// entity remains at its locomotion origin. Apply the same horizontal
		// displacement to the temporary bone palette so room-scale movement cannot
		// leave the body behind. Include the collision correction because the eye
		// view can be pushed away from the desired tracked position near a wall.
		// Do not apply Z: leaning/crouching must not lift the feet off the floor.
		// The desktop mirror uses the ordinary world view, so it receives no
		// tracked offset.
		Vector trackingOffset = {0, 0, 0};
		if (Hooks::m_ActiveEyeTexture && Hooks::m_VR->m_6DOF)
		{
			trackingOffset = Hooks::m_VR->m_HmdPosRelative;
			trackingOffset += Hooks::m_VR->m_CameraCollisionOffset;
			trackingOffset.z = 0.0f;
		}
		const int entityIndex = Hooks::m_Game->GetLocalPlayerIndex();
		if (entityIndex <= 0)
			return 0;

		s_LocalPlayerEntity = player;
		s_LocalPlayerRenderable = renderable;
		const bool wasDrawingDirect = s_DrawingLocalPlayerBodyDirect;
		const Vector previousDrawOffset = s_LocalPlayerBodyDrawOffset;
		s_LocalPlayerBodyDrawOffset = trackingOffset;
		s_DrawingLocalPlayerBodyDirect = true;
		const int result = modelRender->DrawModel(
			1, renderable, ModelInstance::Uncached, entityIndex, model, origin, angles, 0, 0, 0);
		s_DrawingLocalPlayerBodyDirect = wasDrawingDirect;
		s_LocalPlayerBodyDrawOffset = previousDrawOffset;
		static int diagnosticDraws = 0;
		if (diagnosticDraws++ < 8)
		{
			const char *modelName = Hooks::m_Game->GetModelInfo()
				? Hooks::m_Game->GetModelInfo()->GetModelName(model) : nullptr;
			PortalVrLog("First-person direct draw pass=%d eye=%p model=%s player=%p renderable=%p result=%d origin=%f,%f,%f offset=%f,%f,%f angles=%f,%f,%f",
				s_ActiveFirstPersonBodyPass,
				Hooks::m_ActiveEyeTexture, modelName ? modelName : "<unknown>",
				player, renderable, result,
				origin.x, origin.y, origin.z,
				trackingOffset.x, trackingOffset.y, trackingOffset.z,
				angles.x, angles.y, angles.z);
		}
		return result;
	}
}

Hooks::Hooks(Game *game)
{
	PortalVrLog("Hooks::Hooks start");

	const MH_STATUS initStatus = MH_Initialize();
	if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED)
	{
		Game::errorMsg("Failed to init MinHook");
	}
	PortalVrLog("MinHook initialized");

	m_Game = game;
	m_VR = m_Game->m_VR;

	m_PushHUDStep = -999;
	m_PushedHud = true;

	if (!kEnableAllHooks)
	{
		PortalVrLog("All hooks disabled for diagnostic run");
		return;
	}

	initSourceHooks();
	PortalVrLog("initSourceHooks complete render=%d createMove=%d getViewModelFov=%d", hkRenderView.isCreated(), hkCreateMove.isCreated(), hkGetViewModelFOV.isCreated());

	EnableIfCreated(hkCalcViewModelView);
	EnableIfCreated(hkCreateMove);
	EnableIfCreated(hkRenderView);
	EnableIfCreated(hkPush3DView);
	EnableIfCreated(hkPush2DView);
	EnableIfCreated(hkPopView);
	EnableIfCreated(hkPush3DViewDepth);
	EnableIfCreated(hkTraceFirePortal);
	EnableIfCreated(hkDispatchEffect);
	EnableIfCreated(hkPortalBlastCallback);
	EnableIfCreated(hkSettingsClientCmd);
	EnableIfCreated(hkSettingsClientCmdUnrestricted);
	EnableIfCreated(hkCWeaponPortalgun_FirePortal);
	EnableIfCreated(hkEyePosition);
	EnableIfCreated(hkWeapon_ShootPosition);
	EnableIfCreated(hkComputeError);
	EnableIfCreated(hkUpdateObject);
    EnableIfCreated(s_SetPropTarget);EnableIfCreated(s_DetachProp);
	EnableIfCreated(hkUpdateObjectVM);
	EnableIfCreated(hkRotateObject);
	EnableIfCreated(hkEyeAngles);
	EnableIfCreated(hkGetViewModelFOV);
	EnableIfCreated(hkCreateViewModel);
	EnableIfCreated(hkGetAttachmentMatrix);
	EnableIfCreated(hkGetAttachmentAngles);
	EnableIfCreated(hkPortalGunEffectParameters);
	EnableIfCreated(hkDrawModelExecute);
	EnableIfCreated(hkPlayerPortalled);
	EnableIfCreated(hkPlayerUse);
	EnableIfCreated(hkFindUseEntity);
	EnableIfCreated(hkCHudCrosshair_ShouldDraw);
	PortalVrLog("Hooks enabled");
}

Hooks::~Hooks()
{
	if (MH_Uninitialize() != MH_OK)
	{
		Game::errorMsg("Failed to uninitialize MinHook");
	}
}


int Hooks::initSourceHooks()
{
	// VEngineClient013: both entry points are used by GameUI menu commands.
	void* settingsEngine=m_Game->GetEngineClient();
	CreateHookAt(hkSettingsClientCmd,SigScanner::GetVirtualFunction(settingsEngine,7),
		reinterpret_cast<LPVOID>(&dSettingsClientCmd),"VR menu ClientCmd",false);
	CreateHookAt(hkSettingsClientCmdUnrestricted,SigScanner::GetVirtualFunction(settingsEngine,106),
		reinterpret_cast<LPVOID>(&dSettingsClientCmdUnrestricted),"VR menu ClientCmd_Unrestricted",false);
	const bool hasOffsets = m_Game->m_Offsets != nullptr;
	CreateHookAt(hkPortalGunEffectParameters,
		NativePose::PortalGunEffectParameters(reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"))),
		reinterpret_cast<LPVOID>(&dPortalGunEffectParameters), "Portal1::PortalGunEffectParameters", false);
	if (hasOffsets) {
		CreateHookAt(hkGetAttachmentMatrix, m_Game->m_Offsets->GetAttachmentMatrix.address,
			reinterpret_cast<LPVOID>(&dGetAttachmentMatrix), "Portal1::GetAttachment(matrix)", false);
		CreateHookAt(hkGetAttachmentAngles, m_Game->m_Offsets->GetAttachmentAngles.address,
			reinterpret_cast<LPVOID>(&dGetAttachmentAngles), "Portal1::GetAttachment(angles)", false);
	}
	CreateHookAt(hkCreateViewModel,
		SigScanner::FindRttiVtableFunction("server.dll", ".?AVCBasePlayer@@", 323),
		reinterpret_cast<LPVOID>(&dCreateViewModel), "Portal1::CreateViewModel", false);
	void *modelRender = m_Game->GetInterface("engine.dll", Portal1::Interfaces::kModelRender, false);
	CreateHookAt(hkDrawModelExecute, SigScanner::GetVirtualFunction(modelRender, 19),
		reinterpret_cast<LPVOID>(&dDrawModelExecute), "IVModelRender::DrawModelExecute", false);
	void *engineRenderView = m_Game->GetInterface("engine.dll", Portal1::Interfaces::kEngineRenderView);
	CreateHookAt(hkPush3DViewDepth, SigScanner::GetVirtualFunction(engineRenderView, 37),
		reinterpret_cast<LPVOID>(&dPush3DViewDepth), "IVRenderView::Push3DView(depth)", true);
	CreateHookAt(hkPush3DView, SigScanner::GetVirtualFunction(engineRenderView, 38),
		reinterpret_cast<LPVOID>(&dPush3DView), "IVRenderView::Push3DView", true);
	CreateHookAt(hkPush2DView, SigScanner::GetVirtualFunction(engineRenderView, 39),
		reinterpret_cast<LPVOID>(&dPush2DView), "IVRenderView::Push2DView", true);
	CreateHookAt(hkPopView, SigScanner::GetVirtualFunction(engineRenderView, 40),
		reinterpret_cast<LPVOID>(&dPopView), "IVRenderView::PopView", true);
	IViewRender *clientViewRender = m_Game->GetClientViewRender();
	IClientMode *clientMode = m_Game->GetClientMode();

	PortalVrLog(
		"initSourceHooks objects clientMode=%p clientViewRender=%p",
		clientMode,
		clientViewRender);

	const uintptr_t renderViewTarget = clientViewRender
		? SigScanner::GetVirtualFunction(clientViewRender, Portal1::VTableIndex::kViewRender_RenderView)
		: 0;
	CreateHookAt(
		hkRenderView,
		renderViewTarget,
		reinterpret_cast<LPVOID>(&dRenderView),
		"Portal1::CViewRender::RenderView",
		true);

	if (!hasOffsets)
		PortalVrLog("Offsets unavailable, enabling minimal hook set only");

	CreateHookAt(
		hkCalcViewModelView,
		SigScanner::FindRttiVtableFunction("client.dll", ".?AVC_Portal_Player@@", 224),
		reinterpret_cast<LPVOID>(&dCalcViewModelView),
		"Portal1::CalcViewModelView",
		false);

	if (kEnableClientModeHooks)
	{
		const uintptr_t createMoveTarget = clientMode
			? SigScanner::GetVirtualFunction(clientMode, Portal1::VTableIndex::kClientMode_CreateMove)
			: 0;
		const uintptr_t getViewModelFovTarget = clientMode
			? SigScanner::GetVirtualFunction(clientMode, Portal1::VTableIndex::kClientMode_GetViewModelFOV)
			: 0;
		CreateHookAt(
			hkCreateMove,
			createMoveTarget,
			reinterpret_cast<LPVOID>(&dCreateMove),
			"Portal1::ClientModePortalNormal::CreateMove",
			true);
		CreateHookAt(
			hkGetViewModelFOV,
			getViewModelFovTarget,
			reinterpret_cast<LPVOID>(&dGetViewModelFOV),
			"Portal1::ClientModePortalNormal::GetViewModelFOV",
			false);
	}
	else
	{
		PortalVrLog("ClientMode hooks disabled for diagnostic run");
	}

	if (hasOffsets && m_Game->m_Offsets->TraceFirePortalServer.valid)
		CreateHookAt(hkTraceFirePortal, m_Game->m_Offsets->TraceFirePortalServer.address, reinterpret_cast<LPVOID>(&dTraceFirePortal), "TraceFirePortalServer", false);
	CreateHookAt(hkDispatchEffect,NativePose::PortalBlastDispatch(m_Game->m_BaseServer),
		reinterpret_cast<LPVOID>(&dDispatchEffect),"Portal1::PortalBlastDispatch",false);
	CreateHookAt(hkPortalBlastCallback,NativePose::PortalBlastCallback(m_Game->m_BaseClient),
		reinterpret_cast<LPVOID>(&dPortalBlastCallback),"Portal1::PortalBlastCallback",false);
	if (hasOffsets) {
		CreateHookAt(hkPlayerUse, SigScanner::FindRttiVtableFunction("server.dll",
			".?AVCPortal_Player@@", Portal1::VTableIndex::kPortalPlayer_PlayerUse),
			reinterpret_cast<LPVOID>(&dPlayerUse), "Portal1::PlayerUse", false);
		CreateHookAt(hkFindUseEntity, SigScanner::FindRttiVtableFunction("server.dll",
			".?AVCPortal_Player@@", Portal1::VTableIndex::kPortalPlayer_FindUseEntity),
			reinterpret_cast<LPVOID>(&dFindUseEntity), "Portal1::FindUseEntity", false);
		PortalVrLog("Near-contact pickup hook target=%p", hkFindUseEntity.pTarget);
		const uintptr_t eyePositionTarget = FindPortalPlayerFunction(
			Portal1::VTableIndex::kPortalPlayer_EyePosition);
		const uintptr_t shootPositionTarget = FindPortalPlayerFunction(
			Portal1::VTableIndex::kPortalPlayer_WeaponShootPosition);
		const uintptr_t eyeAnglesTarget = FindPortalPlayerFunction(
			Portal1::VTableIndex::kPortalPlayer_EyeAngles);
		CreateHookAt(hkEyePosition,
			eyePositionTarget,
			reinterpret_cast<LPVOID>(&dEyePosition), "Portal1::EyePosition", false);
		CreateHookAt(hkWeapon_ShootPosition,
			shootPositionTarget ? shootPositionTarget : m_Game->m_Offsets->Weapon_ShootPosition.address,
			reinterpret_cast<LPVOID>(&dWeapon_ShootPosition), "Portal1::WeaponShootPosition", false);
		CreateHookAt(hkComputeError, m_Game->m_Offsets->ComputeError.address,
			reinterpret_cast<LPVOID>(&dComputeError), "Portal1::ComputeGrabError", false);
		CreateHookAt(hkUpdateObject, m_Game->m_Offsets->UpdateObject.address,
			reinterpret_cast<LPVOID>(&dUpdateObject), "Portal1::UpdateGrabObject", false);
        s_PropBinding=NativePhysics::Binding::Resolve(m_Game->m_BaseServer);
        CreateHookAt(s_SetPropTarget,s_PropBinding.setTarget,reinterpret_cast<LPVOID>(&SetPropTargetHook),"Portal1::PropTarget",false);
        CreateHookAt(s_DetachProp,s_PropBinding.detach,reinterpret_cast<LPVOID>(&DetachPropHook),"Portal1::PropDetach",false);
        m_VR->m_PropInputSupported=s_PropBinding.shutdownPickup&&s_SetPropTarget.isCreated()&&s_DetachProp.isCreated();
        PortalVrLog("Interaction-20 grip ownership ready=%d pickupShutdown=%p table=%p",m_VR->m_PropInputSupported,s_PropBinding.shutdownPickup,s_PropBinding.pickupTable);
        PortalVrLog("Physics-16 native prop binding target=%p detach=%p attached=%p ready=%d",s_PropBinding.setTarget,s_PropBinding.detach,s_PropBinding.getAttached,s_SetPropTarget.isCreated()&&s_DetachProp.isCreated());
		CreateHookAt(hkUpdateObjectVM, m_Game->m_Offsets->UpdateObjectVM.address,
			reinterpret_cast<LPVOID>(&dUpdateObjectVM), "Portal1::UpdateGrabObjectVM", false);
		CreateHookAt(hkRotateObject, m_Game->m_Offsets->RotateObject.address,
			reinterpret_cast<LPVOID>(&dRotateObject), "Portal1::RotateGrabObject", false);
		CreateHookAt(hkEyeAngles,
			eyeAnglesTarget ? eyeAnglesTarget : m_Game->m_Offsets->EyeAngles.address,
			reinterpret_cast<LPVOID>(&dEyeAngles), "Portal1::EyeAngles", false);
	}

	// The inherited FirePortal detour has Portal 2's ABI. Portal 1 aiming is
	// handled by the verified eight-argument TraceFirePortal hook above.

	CreatePingPointer = (hasOffsets ? reinterpret_cast<tCreatePingPointer>(m_Game->m_Offsets->CreatePingPointer.address) : nullptr);
	PrecacheParticleSystem = hasOffsets && m_Game->m_Offsets->PrecacheParticleSystem.valid ? (tPrecacheParticleSystem)m_Game->m_Offsets->PrecacheParticleSystem.address : nullptr;
	GetOwner = hasOffsets && m_Game->m_Offsets->GetOwner.valid ? (tGetOwner)m_Game->m_Offsets->GetOwner.address : nullptr;
	GetFullScreenTexture = hasOffsets && m_Game->m_Offsets->GetFullScreenTexture.valid ? (tGetFullScreenTexture)m_Game->m_Offsets->GetFullScreenTexture.address : nullptr;
	CreateHookAt(
		hkPlayerPortalled,
		(hasOffsets ? m_Game->m_Offsets->PlayerPortalled.address : 0),
		reinterpret_cast<LPVOID>(&dPlayerPortalled),
		"Portal1::C_Portal_Player::PlayerPortalled",
		false);
	CreateHookAt(
		hkCHudCrosshair_ShouldDraw,
		(hasOffsets ? m_Game->m_Offsets->CHudCrosshair_ShouldDraw.address : 0),
		reinterpret_cast<LPVOID>(&dCHudCrosshair_ShouldDraw),
		"Portal1::CHudCrosshair::ShouldDraw",
		false);
	PortalVrLog(
		"initSourceHooks targets render=%p createMove=%p getViewModelFov=%p calcViewModel=%p traceFirePortal=%p eyePosition=%p shoot=%p computeError=%p update=%p eyeAngles=%p playerPortalled=%p crosshair=%p playerUse=%p",
		hkRenderView.pTarget,
		hkCreateMove.pTarget,
		hkGetViewModelFOV.pTarget,
		hkCalcViewModelView.pTarget,
		hkTraceFirePortal.pTarget,
		hkEyePosition.pTarget,
		hkWeapon_ShootPosition.pTarget,
		hkComputeError.pTarget,
		hkUpdateObject.pTarget,
		hkEyeAngles.pTarget,
		hkPlayerPortalled.pTarget,
		hkCHudCrosshair_ShouldDraw.pTarget,
		hkPlayerUse.pTarget);
	return 1;
} 

bool __fastcall Hooks::dCHudCrosshair_ShouldDraw(void* ecx, void* edx) {
	bool shouldDraw = hkCHudCrosshair_ShouldDraw.fOriginal(ecx);

	m_VR->m_DrawCrosshair = shouldDraw;

	return ((m_VR->m_AimMode == 1) ? shouldDraw : false);
}

void __fastcall Hooks::dPrecache(void* ecx, void* edx) {
	hkPrecache.fOriginal(ecx);
	if (PrecacheParticleSystem)
		PrecacheParticleSystem("robot_point_beam");
}

void __fastcall Hooks::dClientThink(void* ecx, void* edx) {
	hkClientThink.fOriginal(ecx);
}

void __fastcall Hooks::dSetDrawOnlyForSplitScreenUser(void* ecx, void* edx, int nSlot) {
	hkSetDrawOnlyForSplitScreenUser.fOriginal(ecx, -1);
}

ITexture *__fastcall Hooks::dGetFullScreenTexture()
{
	ITexture *result = hkGetFullScreenTexture.fOriginal();
	return result;
}

ITexture* __fastcall Hooks::dGetRenderTarget(void* ecx, void* edx)
{
	ITexture* result = hkGetRenderTarget.fOriginal(ecx);
	return result;
}


void __fastcall Hooks::dPush3DView(void *ecx, void *edx, const CViewSetup &view, int flags, ITexture *target, void *frustum)
{
    if(m_ActiveEyeTexture) {
        const char* name=target?target->GetName():nullptr;
        if(RenderPolicy::UseEyeTarget(m_VR->m_ActiveLegacyEffects,name))target=m_ActiveEyeTexture;
        static unsigned logged=0;
        if(logged++<6)PortalVrLog("Eye world target native=%s actual=%s size=%dx%d view=%dx%d direct=%d",name?name:"null",target?target->GetName():"null",target?target->GetActualWidth():0,target?target->GetActualHeight():0,view.width,view.height,!m_VR->m_ActiveLegacyEffects);
    }
    hkPush3DView.fOriginal(ecx, view, flags, target, frustum);
    s_BodyViewStack.push_back(IsBodyWorldView(view, target, nullptr));
}

void __fastcall Hooks::dPush3DViewDepth(void *ecx, void *edx, const CViewSetup &view, int flags, ITexture *target, void *frustum, ITexture *depth)
{
    if(m_ActiveEyeTexture&&RenderPolicy::UseEyeTarget(m_VR->m_ActiveLegacyEffects,target?target->GetName():nullptr))target=m_ActiveEyeTexture;
    CViewSetup corrected = view;
    static const uintptr_t viewmodelCall = NativePose::ViewmodelProjectionReturn(m_Game->m_BaseClient);
    if (m_ActiveEyeTexture && viewmodelCall
        && reinterpret_cast<uintptr_t>(_ReturnAddress()) == viewmodelCall) {
        // The gun uses world-space tracked bones. Match the complete active
        // eye camera, including origin, angles and off-center projection.
        memcpy(&corrected,&s_EyeWorldView,sizeof(corrected));
        corrected.zNear = view.zNear;
        corrected.zFar = view.zFar;
        static unsigned logged = 0;
        if (logged++ < 4) PortalVrLog("Viewmodel eye camera originalAspect=%f eyeAspect=%f originalFov=%f eyeFov=%f originDelta=%f angleDelta=%f",
            view.m_flAspectRatio,corrected.m_flAspectRatio,view.fov,corrected.fov,
            sqrtf((view.origin-corrected.origin).LengthSqr()),sqrtf((view.angles-corrected.angles).LengthSqr()));
    }
    hkPush3DViewDepth.fOriginal(ecx, corrected, flags, target, frustum, depth);
    s_BodyViewStack.push_back(IsBodyWorldView(corrected, target, depth));
}

void __fastcall Hooks::dRenderView(void *ecx, void *edx, CViewSetup &originalSetup, int nClearFlags, int whatToDraw)
{
    CViewSetup setup = originalSetup;
	static bool loggedRenderViewEntry = false;
	if (!m_Game->TryResolveVrInterfaces())
		return hkRenderView.fOriginal(ecx, setup, nClearFlags, whatToDraw);

	if (!loggedRenderViewEntry)
	{
		PortalVrLog("dRenderView first entry ecx=%p flags=%d draw=%d view=%dx%d fov=%f origin=%f,%f,%f", ecx, nClearFlags, whatToDraw, setup.width, setup.height, setup.fov, setup.origin.x, setup.origin.y, setup.origin.z);
		loggedRenderViewEntry = true;
	}

	if (!m_VR->m_CreatedVRTextures)
		return hkRenderView.fOriginal(ecx, setup, nClearFlags, whatToDraw);

    if(!m_Game->IsInGame()) {
        // Source is replacing models and entity handles. Only its normal load
        // view may render; no recursive eye or direct avatar draw is eligible.
        s_InlineBodyDrawEligible=s_ActiveFirstPersonBodyPass=s_DrawingLocalPlayerBodyDirect=false;
        s_BodyViewStack.clear();s_BodyDrawTriggered=false;
        s_NativeAvatarSourceReady=s_NativeAvatarDirectDrawn=s_AvatarPalette.valid=false;
        s_LocalPlayerEntity=s_LocalPlayerRenderable=nullptr;s_HasLocalPlayerBodyTransform=false;
        s_LocalWorldGunReady=false;s_LocalWorldGunEntity=s_LocalWorldGunRenderable=nullptr;
        s_RightViewmodelRenderable=s_LeftArmRenderable=s_GunAttachmentRenderable=nullptr;
        s_GunAttachmentModel=nullptr;s_PortalCamera={};s_PortalBlast.pending=false;
        s_PropCarry.Reset();s_PropPlayer=s_PropController=nullptr;m_VR->m_PropHold.Detached();
        m_VR->m_RenderedNewFrame=false;m_VR->m_GrabPoseValid=m_VR->m_PickupAimValid=false;
        return hkRenderView.fOriginal(ecx,setup,nClearFlags,whatToDraw);
    }

	//VPanel* g_pFullscreenRootPanel = *(VPanel**)(m_Game->m_Offsets->g_pFullscreenRootPanel.address);

	IMaterialSystem* matSystem = m_Game->GetMaterialSystem();
	if (!matSystem)
		return hkRenderView.fOriginal(ecx, setup, nClearFlags, whatToDraw);

	CViewSetup desktopView = setup;
	static const auto portalTrace = PortalTrace::Binding::Resolve(m_Game->m_BaseClient);
	static const bool portalCameraSupported = PortalCamera::Supported(m_Game->m_BaseClient);
	const auto portalCamera = PortalCamera::Read(m_Game->GetLocalPortalPlayer(),portalTrace,
		portalCameraSupported && m_VR->m_IsVREnabled);
	PortalCamera::Scope portalCameraScope(s_PortalCamera,portalCamera);
	// CalcPortalView already transformed the native origin. Add roomscale and
	// stereo offsets in player space, then map the complete camera and models.
	Vector position = portalCamera.Unmap(setup.origin);

	m_VR->m_SetupOrigin = position;
	m_VR->UpdateCameraCollision(position);

	Vector hmdAngle = m_VR->GetViewAngle();
	static int renderPoseLogCounter = 0;
	if ((++renderPoseLogCounter % 120) == 0)
	{
		PortalVrLog("Render view pose hmd=%f,%f,%f setup=%f,%f,%f",
			hmdAngle.x, hmdAngle.y, hmdAngle.z,
			setup.angles.x, setup.angles.y, setup.angles.z);
	}
	m_Game->SetViewAngles(QAngle(hmdAngle.x, hmdAngle.y, hmdAngle.z));

	float aspect = setup.m_flAspectRatio;

	setup.x = 0;
	setup.y = 0;
	setup.width = m_VR->m_RenderWidth;
	setup.height = m_VR->m_RenderHeight;
	setup.m_nUnscaledWidth = m_VR->m_RenderWidth;
	setup.m_nUnscaledHeight = m_VR->m_RenderHeight;
	setup.fov = m_VR->m_Fov;
	setup.fovViewmodel = m_VR->m_Fov;
	setup.m_flAspectRatio = m_VR->m_Aspect;
	setup.m_bOrtho = false;
	setup.m_bOffCenter = false;
	setup.m_bViewToProjectionOverride = false;
	RenderPolicy::PrepareEye(setup,m_VR->m_ActiveLegacyEffects);
	setup.zNear = CameraCollision::NearClip;
	setup.zNearViewmodel = 2;
	const auto eyeAngles=portalCamera.Map(QAngle(hmdAngle.x,hmdAngle.y,hmdAngle.z));
	setup.angles = Vector(eyeAngles.x,eyeAngles.y,eyeAngles.z);

	if (!m_VR->m_CreatedVRTextures)
	{
		setup.origin = portalCamera.Map(m_VR->GetViewOrigin(position));
		hkRenderView.fOriginal(ecx, setup, nClearFlags, whatToDraw);
		m_PushedHud = false;
		m_VR->m_RenderedNewFrame = true;
		return;
	}

	CViewSetup leftEyeView = setup;
	CViewSetup rightEyeView = setup;
	const auto eyeRenderStart=std::chrono::steady_clock::now();

	// Left eye CViewSetup
	leftEyeView.origin = portalCamera.Map(m_VR->GetViewOriginLeft(position));
	// Freeze both eyes before either Source render pass can mutate game state.
	rightEyeView.origin = portalCamera.Map(m_VR->GetViewOriginRight(position));
    const Vector frozenCameraCenter=m_VR->GetViewOrigin(position);
    ++s_AvatarFrame;s_NativeAvatarDirectDrawn=false;s_NativeAvatarSourceReady=false;
    if(m_VR->m_Graphics.bodyIK&&m_VR->m_FirstPersonBody) {
        if(auto* player=m_Game->GetLocalPortalPlayer()) {
            auto* renderable=reinterpret_cast<unsigned char*>(player)+4;
            const auto setupBones=SigScanner::GetVirtualFunction(renderable,Portal1::VTableIndex::kClientRenderable_SetupBones);
            using SetupBones=bool(__thiscall*)(void*,matrix3x4_t*,int,int,float);
            s_NativeAvatarSourceReady=setupBones&&reinterpret_cast<SetupBones>(setupBones)(renderable,s_NativeAvatarSource,BodyIK::Max,0x7ff00,0.f);
        }
    }
	// Capture only the local active weapon before recursive portal rendering.
    // Same known renderable interface and SetupBones slot as the player.
    s_LocalWorldGunReady=false;s_LocalWorldGunRenderable=nullptr;s_LocalWorldGunEntity=nullptr;
    if(m_VR->m_Graphics.portalGunPose)if(auto* player=m_Game->GetLocalPortalPlayer()) {
        auto* weapon=m_Game->GetLocalPortalWeapon();
        if(weapon&&SigScanner::IsReadable(reinterpret_cast<uintptr_t>(weapon)+4,sizeof(void*))) {
            auto* renderable=reinterpret_cast<unsigned char*>(weapon)+4;
            const auto getModel=SigScanner::GetVirtualFunction(renderable,Portal1::VTableIndex::kClientRenderable_GetModel);
            using Model=model_t*(__thiscall*)(void*);
            auto* model=getModel?reinterpret_cast<Model>(getModel)(renderable):nullptr;
            const char* name=model&&m_Game->GetModelInfo()?m_Game->GetModelInfo()->GetModelName(model):nullptr;
            static unsigned captureLogs=0;
            if(captureLogs++<8)PortalVrLog("World gun source active=%p model=%s",weapon,name?name:"<null>");
            if(name&&ModelPath::Equal(name,"models/weapons/w_portalgun.mdl")) {
                Vector origin;QAngle angles;
                s_LocalWorldGunReady=GetRenderableTransform(renderable,origin,angles);
                if(s_LocalWorldGunReady){s_LocalWorldGunNativeModel=PortalPose::Frame(origin,angles);s_LocalWorldGunRenderable=renderable;s_LocalWorldGunEntity=weapon;}
            }
        }
    }
    static bool wasPortalCamera = false;
	if (wasPortalCamera != portalCamera.transformed) {
		PortalVrLog("Portal eye camera transformed=%d supported=%d native=%f,%f,%f player=%f,%f,%f eye=%f,%f,%f angles=%f,%f,%f",
			portalCamera.transformed,portalCameraSupported,desktopView.origin.x,desktopView.origin.y,desktopView.origin.z,
			position.x,position.y,position.z,leftEyeView.origin.x,leftEyeView.origin.y,leftEyeView.origin.z,
			leftEyeView.angles.x,leftEyeView.angles.y,leftEyeView.angles.z);
		wasPortalCamera=portalCamera.transformed;
	}
    static int loggedEye = 0;
    if (++loggedEye == 120) PortalVrLog("Eye view fov=%f aspect=%f pos=%f,%f,%f angle=%f,%f,%f near=%f far=%f ortho=%d projectionOverride=%d",
        leftEyeView.fov,leftEyeView.m_flAspectRatio,leftEyeView.origin.x,leftEyeView.origin.y,leftEyeView.origin.z,
        leftEyeView.angles.x,leftEyeView.angles.y,leftEyeView.angles.z,leftEyeView.zNear,leftEyeView.zFar,leftEyeView.m_bOrtho,leftEyeView.m_bViewToProjectionOverride);

	IMatRenderContext* rndrContext = matSystem->GetRenderContext();
	if (!rndrContext)
		return hkRenderView.fOriginal(ecx, setup, nClearFlags, whatToDraw);

	m_VR->m_BindingEyeTexture = VR::Texture_LeftEye;
	rndrContext->PushRenderTargetAndViewport(
		m_VR->m_LeftEyeTexture,
		0,
		0,
		static_cast<int>(m_VR->m_RenderWidth),
		static_cast<int>(m_VR->m_RenderHeight));
	m_VR->m_BindingEyeTexture = VR::Texture_None;
	m_ActiveEyeTexture = m_VR->m_LeftEyeTexture;
	memcpy(&s_EyeWorldView,&leftEyeView,sizeof(s_EyeWorldView));
	s_HasLocalPlayerBodyTransform = false;
	s_BodyDrawTriggered = false;
	s_InlineBodyDrawEligible = true;
	s_ActiveFirstPersonBodyPass = true;
		ExpectBodyView(leftEyeView);
		s_BodyCameraCenter = frozenCameraCenter;
		hkRenderView.fOriginal(ecx, leftEyeView, nClearFlags, whatToDraw & ~RENDERVIEW_DRAWHUD);
	s_ActiveFirstPersonBodyPass = false;
	s_InlineBodyDrawEligible = false;
	m_ActiveEyeTexture = nullptr;
	rndrContext->PopRenderTargetAndViewport();
	
	// The right eye shares the already frozen head orientation and camera origin.

	m_VR->m_BindingEyeTexture = VR::Texture_RightEye;
	rndrContext->PushRenderTargetAndViewport(
		m_VR->m_RightEyeTexture,
		0,
		0,
		static_cast<int>(m_VR->m_RenderWidth),
		static_cast<int>(m_VR->m_RenderHeight));
	m_VR->m_BindingEyeTexture = VR::Texture_None;
	m_ActiveEyeTexture = m_VR->m_RightEyeTexture;
	memcpy(&s_EyeWorldView,&rightEyeView,sizeof(s_EyeWorldView));
	s_HasLocalPlayerBodyTransform = false;
	s_BodyDrawTriggered = false;
	s_InlineBodyDrawEligible = true;
	s_ActiveFirstPersonBodyPass = true;
		ExpectBodyView(rightEyeView);
		s_BodyCameraCenter = frozenCameraCenter;
		hkRenderView.fOriginal(ecx, rightEyeView, nClearFlags, whatToDraw & ~RENDERVIEW_DRAWHUD);
	s_ActiveFirstPersonBodyPass = false;
	s_InlineBodyDrawEligible = false;
	m_ActiveEyeTexture = nullptr;
	rndrContext->PopRenderTargetAndViewport();

	m_PushedHud = false;
	rndrContext->Release();
	m_VR->m_LastEyeRenderMs=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-eyeRenderStart).count();
	m_VR->m_LastMirrorRenderMs=0;

	/*rndrContext = matSystem->GetRenderContext();

	ITexture* fullscreenTxt = rndrContext->GetRenderTarget();

	Rect_t srcRect;
	srcRect.x = setup.x;
	srcRect.y = setup.y;
	srcRect.width = 1920;
	srcRect.height = 1080;

	rndrContext->SetRenderTarget(m_VR->m_RightEyeTexture);
	rndrContext->CopyRenderTargetToTextureEx(fullscreenTxt, 0, &srcRect, &srcRect);

	rndrContext->SetRenderTarget(NULL);
	rndrContext->Release();*/

	if (m_VR->m_RenderWindow) {
		const auto mirrorStart=std::chrono::steady_clock::now();
		s_HasLocalPlayerBodyTransform = false;
		s_BodyDrawTriggered = false;
		s_InlineBodyDrawEligible = true;
		s_ActiveFirstPersonBodyPass = true;
		ExpectBodyView(desktopView);
		s_BodyCameraCenter = position;
		hkRenderView.fOriginal(ecx, desktopView, nClearFlags, whatToDraw);
		s_ActiveFirstPersonBodyPass = false;
		s_InlineBodyDrawEligible = false;
		m_VR->m_LastMirrorRenderMs=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-mirrorStart).count();
	}


	m_VR->m_RenderedNewFrame = true;
}

bool __fastcall Hooks::dCreateMove(void *ecx, void *edx, float flInputSampleTime, CUserCmd *cmd)
{
	if (!cmd)
		return hkCreateMove.fOriginal(ecx, flInputSampleTime, cmd);

	if (!cmd->command_number)
		return hkCreateMove.fOriginal(ecx, flInputSampleTime, cmd);

    const bool originalResult = hkCreateMove.fOriginal(ecx, flInputSampleTime, cmd);
    if (!m_VR->m_IsVREnabled)
        return originalResult;
	if(!m_VR->m_SystemFocus.Allowed() || m_VR->m_GraphicsOpen || m_Game->IsCursorVisible()) {
		cmd->forwardmove=cmd->sidemove=cmd->upmove=0;
		cmd->buttons&=~(IN_ATTACK|IN_ATTACK2|IN_USE|IN_JUMP|IN_DUCK|IN_RELOAD|IN_FORWARD|IN_BACK|IN_MOVELEFT|IN_MOVERIGHT);
		return originalResult;
	}
	if (m_VR->m_IsVREnabled)
	{
		// Keep IN_USE and the sampled hand on the same tick. PlayerUse and the
		// grab solver scope the hand override; portal physics must keep seeing
		// the real player eye pose. Command view angles remain HMD-driven.
        if(m_VR->m_Weapon.stowed||m_VR->m_Weapon.fireBlocked||m_VR->m_PropHold.attached)cmd->buttons&=~(IN_ATTACK|IN_ATTACK2);
		const bool useHeld = m_VR->PressedDigitalAction(m_VR->m_ActionUse);
		m_VR->SnapshotGrabPose();
		cmd->buttons = useHeld ? (cmd->buttons | IN_USE) : (cmd->buttons & ~IN_USE);
		cmd->viewangles = m_VR->m_HmdAngAbs;
		static bool lastUseHeld = false;
		if (useHeld != lastUseHeld) {
			PortalVrLog("Controller use state=%d origin=%f,%f,%f angle=%f,%f,%f cmdButtons=0x%X",
				useHeld, m_VR->m_GrabControllerPos.x, m_VR->m_GrabControllerPos.y,
				m_VR->m_GrabControllerPos.z, cmd->viewangles.x, cmd->viewangles.y,
				cmd->viewangles.z, cmd->buttons);
			lastUseHeld = useHeld;
		}

		vr::InputAnalogActionData_t analogActionData;
		if (m_VR->m_Graphics.movement && m_VR->GetAnalogActionData(m_VR->m_ActionWalk, analogActionData)) {
			// Run toward other guy
			cmd->buttons &= ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT);

			float forward=NativeFit::Axis(analogActionData.y,m_VR->m_Graphics.moveDeadzone)*MAX_LINEAR_SPEED*m_VR->m_Graphics.moveSpeed;
			float side=NativeFit::Axis(analogActionData.x,m_VR->m_Graphics.moveDeadzone)*MAX_LINEAR_SPEED*m_VR->m_Graphics.moveSpeed;
			if(m_VR->m_Graphics.moveDirection==1) {
				const float angle=DEG2RAD(m_VR->m_LeftControllerAngAbs.y-cmd->viewangles.y),oldForward=forward;
				forward=oldForward*cosf(angle)+side*sinf(angle);side=side*cosf(angle)-oldForward*sinf(angle);
			}
			cmd->forwardmove+=forward;cmd->sidemove+=side;

			// We'll only be moving fwd or sideways
			cmd->upmove = 0.0f;

			if (cmd->forwardmove > 0.0f)
			{
				cmd->buttons |= IN_FORWARD;
			}
			else if (cmd->forwardmove < 0.0f)
			{
				cmd->buttons |= IN_BACK;
			}

			if (cmd->sidemove > 0.0f)
			{
				cmd->buttons |= IN_MOVELEFT;
			}
			else if (cmd->sidemove < 0.0f)
			{
				cmd->buttons |= IN_MOVERIGHT;
			}

		}

		if (m_VR->m_RoomscaleActive)
		{
			// How much have we moved since last CreateMove?
			Vector setupOriginToHMD = (m_VR->m_HmdPosRelativeRaw - m_VR->m_HmdPosRelativeRawPrev) * m_VR->m_VRScale; //m_VR->m_HmdPosRelative - m_VR->m_HmdPosRelativePrev;
			m_VR->m_HmdPosRelativeRawPrev = m_VR->m_HmdPosRelativeRaw;

			setupOriginToHMD.z = 0;
			float distance = VectorLength(setupOriginToHMD);
			if (distance > 0)
			{
				float forwardSpeed = DotProduct2D(setupOriginToHMD, m_VR->m_HmdForward);
				float sideSpeed = DotProduct2D(setupOriginToHMD, m_VR->m_HmdRight);
				cmd->forwardmove += distance * forwardSpeed;
				cmd->sidemove += distance * sideSpeed;

				// Let's update the position and the previous too
				/*m_VR->m_HmdPosRelative -= setupOriginToHMD;
				m_VR->m_HmdPosRelativePrev = m_VR->m_HmdPosRelative;*/

				/*m_VR->m_Center += m_VR->m_HmdPosRelativeRaw - m_VR->m_HmdPosRelativeRawPrev;
				m_VR->m_HmdPosRelativeRawPrev = m_VR->m_HmdPosRelativeRaw;*/

				//m_VR->ResetPosition();
			}
		}
	}

	return false;
}

void __fastcall Hooks::dEndFrame(void *ecx, void *edx)
{
	return hkEndFrame.fOriginal(ecx);
}

void __fastcall Hooks::dCreateViewModel(void *ecx, void *edx, int index)
{
    hkCreateViewModel.fOriginal(ecx, index);
    if (index == 0 && m_VR->m_IsVREnabled) {
        // Use Source's second owned viewmodel. The game manages its lifetime,
        // networking and save data, and gun pickup can keep the left arm visible.
        hkCreateViewModel.fOriginal(ecx, 1);
        PortalVrLog("Created independent left-arm viewmodel");
    }
}

void __fastcall Hooks::dSettingsClientCmd(void* self,void*,const char* command) {
	if(m_VR && m_VR->HandleSettingsCommand(command)) return;
	hkSettingsClientCmd.fOriginal(self,command);
}
void __fastcall Hooks::dSettingsClientCmdUnrestricted(void* self,void*,const char* command) {
	if(m_VR && m_VR->HandleSettingsCommand(command)) return;
	hkSettingsClientCmdUnrestricted.fOriginal(self,command);
}

namespace {
    bool TrackedGunAttachment(void *renderable, int number, matrix3x4_t& output) {
        auto *vr = Hooks::m_VR;
        if (!vr || !vr->m_IsVREnabled || !vr->WeaponTracked()
            || !renderable || renderable != s_RightViewmodelRenderable
            || renderable != s_GunAttachmentRenderable || number < 1
            || number > s_GunAttachments.count) return false;
        const auto getModel = SigScanner::GetVirtualFunction(renderable,Portal1::VTableIndex::kClientRenderable_GetModel);
        const auto setupBones = SigScanner::GetVirtualFunction(renderable,Portal1::VTableIndex::kClientRenderable_SetupBones);
        using GetModelFn = const void *(__thiscall *)(void *);
        using SetupBonesFn = bool (__thiscall *)(void *,matrix3x4_t *,int,int,float);
        if (!getModel || !setupBones
            || reinterpret_cast<GetModelFn>(getModel)(renderable) != s_GunAttachmentModel) return false;
        // The original successful attachment query just computed these native
        // bones. Copy them; never write transformed matrices into Source's cache.
        static thread_local bool resolving = false;
        if (resolving) return false;
        struct Guard { bool& value; Guard(bool& v):value(v) { value=true; } ~Guard() { value=false; } } guard(resolving);
        matrix3x4_t nativeBones[128];
        constexpr int usedByAttachment = 0x200;
        if (!reinterpret_cast<SetupBonesFn>(setupBones)(renderable,nativeBones,128,usedByAttachment,0.0f)) return false;
        const auto controller = vr->GetGunControllerFrame();
        if (!s_GunAttachments.Resolve(number,controller,nativeBones,output)) return false;
        s_PortalCamera.Map(&output,1);
        static int logged = 0;
        if (number == 1 && logged++ < 12)
            PortalVrLog("VR muzzle attachment aligned origin=%f,%f,%f",output[0][3],output[1][3],output[2][3]);
        return true;
    }
}

bool __fastcall Hooks::dGetAttachmentMatrix(void *ecx, void *edx, int number, matrix3x4_t& matrix) {
    const bool result = hkGetAttachmentMatrix.fOriginal(ecx,number,matrix);
    if (result) TrackedGunAttachment(ecx,number,matrix);
    return result;
}

bool __fastcall Hooks::dGetAttachmentAngles(void *ecx, void *edx, int number, Vector& origin, QAngle& angles) {
    const bool result = hkGetAttachmentAngles.fOriginal(ecx,number,origin,angles);
    matrix3x4_t matrix;
    if (result && TrackedGunAttachment(ecx,number,matrix)) {
        origin = PortalPose::Position(matrix);
        angles = PortalPose::Angles(HandPose::RigidOrientation(matrix));
        GunEffects::PositionScope::Capture(origin);
    }
    return result;
}

void __fastcall Hooks::dPortalGunEffectParameters(void *ecx, void *, int index, void *color,
    float *size, void **material, Vector& position, bool worldModel) {
    GunEffects::PositionScope scope(position, m_VR && m_VR->m_IsVREnabled && !worldModel);
    hkPortalGunEffectParameters.fOriginal(ecx,index,color,size,material,position,worldModel);
    // Native claw/core sprites are drawn separately from the skinned gun.
    // Hiding gun bones alone leaves these emissive quads floating in space.
    if(m_VR&&m_VR->m_IsVREnabled&&(!worldModel||ecx==s_LocalWorldGunEntity)&&(m_VR->m_Weapon.stowed||!m_VR->m_Graphics.gun)) {
        if(size)*size=0;
        return;
    }
    Vector nativePosition;
    if (scope.Restore(&nativePosition)) {
        static int logged = 0;
        if (logged++ < 18) PortalVrLog("Gun effect anchored index=%d removedProjectionOffset=%f position=%f,%f,%f",
            index,sqrtf((nativePosition-position).LengthSqr()),position.x,position.y,position.z);
    }
}

void __fastcall Hooks::dCalcViewModelView(void *ecx, void *edx, const Vector &eyePosition, const QAngle &eyeAngles)
{
	s_LeftArmRenderable = nullptr;
	s_RightViewmodelRenderable = nullptr;
	if (m_VR->m_IsVREnabled && m_Game->m_Offsets->GetViewModel.valid) {
		using GetViewModelFn = void *(__thiscall *)(void *, int, bool);
		for (int index = 0; index < 2; ++index) {
			void *vm = reinterpret_cast<GetViewModelFn>(m_Game->m_Offsets->GetViewModel.address)(ecx, index, false);
			if (vm) {
				if (index == 0) {
					s_RightViewmodelRenderable = static_cast<unsigned char *>(vm) + 4;
					const auto getModel = SigScanner::GetVirtualFunction(s_RightViewmodelRenderable,
						Portal1::VTableIndex::kClientRenderable_GetModel);
					using GetModelFn = const void *(__thiscall *)(void *);
					if (!getModel || reinterpret_cast<GetModelFn>(getModel)(s_RightViewmodelRenderable) != s_GunAttachmentModel) {
						m_VR->m_PortalAimLastSeen = 0;
						m_VR->m_SupportLastSeen = 0;
					}
				}
				if (index == 1) s_LeftArmRenderable = static_cast<unsigned char *>(vm) + 4;
				using GetWeaponFn = void *(__thiscall *)(void *);
				const auto getWeapon = SigScanner::GetVirtualFunction(vm, 209);
				void *weapon = getWeapon ? reinterpret_cast<GetWeaponFn>(getWeapon)(vm) : nullptr;
				if (getWeapon && !weapon) {
					using SetWeaponModelFn = void (__thiscall *)(void *, const char *, void *);
					using RemoveEffectsFn = void (__thiscall *)(void *, int);
					const auto setModel = SigScanner::GetVirtualFunction(vm, 202);
					const auto removeEffects = SigScanner::GetVirtualFunction(vm, 208);
					if (setModel && removeEffects) {
						void *renderable = static_cast<unsigned char *>(vm) + 4;
					const auto getModel = SigScanner::GetVirtualFunction(
						renderable, Portal1::VTableIndex::kClientRenderable_GetModel);
						using GetModelFn = model_t *(__thiscall *)(void *);
						model_t *model = getModel ? reinterpret_cast<GetModelFn>(getModel)(renderable) : nullptr;
						const char *name = model && m_Game->GetModelInfo() ? m_Game->GetModelInfo()->GetModelName(model) : nullptr;
						if (!name || _stricmp(name, "models/weapons/v_hands.mdl"))
							reinterpret_cast<SetWeaponModelFn>(setModel)(vm, "models/weapons/v_hands.mdl", nullptr);
						// Keep custom hands independent of the world-player skeleton.
						// Reference-pose rendering also avoids the legacy idle animation.
						reinterpret_cast<RemoveEffectsFn>(removeEffects)(vm, 1 | 32);
					}
				}
				static int loggedModel = 0;
				if (loggedModel++ < 3) PortalVrLog("Viewmodel entity=%p weapon=%p", vm, weapon);
			}
		}
	}
	if (!s_RightViewmodelRenderable) m_VR->m_SupportLastSeen = 0;
	static int logged = 0;
	if (logged++ < 3) PortalVrLog("Controller viewmodel update player=%p hand=%f,%f,%f", ecx,
		m_VR->m_RightControllerPosRel.x,m_VR->m_RightControllerPosRel.y,m_VR->m_RightControllerPosRel.z);
	Vector vecNewOrigin = eyePosition;
	QAngle vecNewAngles = eyeAngles;

	//std::cout << "dCalcViewModelView: (" << m_VR->m_IsVREnabled << ")\n";

	if (m_VR->m_IsVREnabled)
	{
		vecNewOrigin = m_VR->GetRecommendedViewmodelAbsPos(eyePosition);
		vecNewAngles = m_VR->GetRecommendedViewmodelAbsAngle();
	}


	return hkCalcViewModelView.fOriginal(ecx, vecNewOrigin, vecNewAngles);
}

float __fastcall Hooks::dProcessUsercmds(void *ecx, void *edx, edict_t *player, void *buf, int numcmds, int totalcmds, int dropped_packets, bool ignore, bool paused)
{
	Server_BaseEntity *pPlayer = (Server_BaseEntity*)player->m_pUnk->GetBaseEntity();

	if (pPlayer)
	{
		int index = ServerEntityIndex(pPlayer);
		m_Game->m_CurrentUsercmdID = index;
	}

	return hkProcessUsercmds.fOriginal(ecx, player, buf, numcmds, totalcmds, dropped_packets, ignore, paused);
}

int Hooks::dWriteUsercmd(bf_write *buf, CUserCmd *to, CUserCmd *from)
{
	auto result =  hkWriteUsercmd.fOriginal(buf, to, from);

	// Let's write our stuff into the buffer
	if (m_VR->m_IsVREnabled)
	{
        Vector controllerPos=m_VR->WeaponPosition();
        QAngle controllerAngles=PickupTrace::CarryAngles(m_VR->WeaponForward(),m_VR->WeaponUp());

		buf->WriteChar(-2);
		buf->WriteBitVec3Coord(controllerPos);
		buf->WriteBitAngles(controllerAngles);
	}

	return result;
}

int Hooks::dReadUsercmd(bf_read *buf, CUserCmd* move, CUserCmd* from)
{
	auto result = hkReadUsercmd.fOriginal(buf, move, from);

	int i = m_Game->m_CurrentUsercmdID;
	auto& vrPlayer = m_Game->m_PlayersVRInfo[i];

	auto pos = buf->Tell();
	int res = buf->ReadChar();

	// This means we got a VR player on the other side
	if (res == -2)
	{
		vrPlayer.isUsingVR = true;
		buf->ReadBitVec3Coord(vrPlayer.controllerPos);
		buf->ReadBitAngles(vrPlayer.controllerAngle);
	}
	else {
		vrPlayer.isUsingVR = false;
		buf->Seek(pos);
	}

	return result;
}


void Hooks::dAdjustEngineViewport(int &x, int &y, int &width, int &height)
{
	width = m_VR->m_RenderWidth;
	height = m_VR->m_RenderHeight;

	hkAdjustEngineViewport.fOriginal(x, y, width, height);
}

void Hooks::dGetViewport(void *ecx, void *edx, int &x, int &y, int &width, int &height)
{
	hkGetViewport.fOriginal(ecx, x, y, width, height);

	width = m_VR->m_RenderWidth;
	height = m_VR->m_RenderHeight;
}

int Hooks::dGetPrimaryAttackActivity(void *ecx, void *edx, void *meleeInfo)
{
	return hkGetPrimaryAttackActivity.fOriginal(ecx, meleeInfo);
}

static void DrawTrackedModel(void *ecx, void *state, const ModelRenderInfo_t& info,
    matrix3x4_t* bones, int count)
{
    if(!s_PortalCamera.transformed)
        return Hooks::hkDrawModelExecute.fOriginal(ecx,state,info,bones);
    s_PortalCamera.Map(bones,count);
    auto mapped=info;
    mapped.origin=s_PortalCamera.Map(info.origin);
    mapped.angles=s_PortalCamera.Map(info.angles);
    matrix3x4_t modelToWorld;
    if(info.pModelToWorld) {
        modelToWorld=HandPose::Concat(s_PortalCamera.toLinked,*info.pModelToWorld);
        mapped.pModelToWorld=&modelToWorld;
    }
    Vector lightingOrigin;
    if(info.pLightingOrigin) {
        lightingOrigin=s_PortalCamera.Map(*info.pLightingOrigin);
        mapped.pLightingOrigin=&lightingOrigin;
    }
    ModelDrawState::State mappedState;
    const bool mappedStateValid=state&&info.pModelToWorld&&SigScanner::IsReadable(reinterpret_cast<uintptr_t>(state),sizeof(mappedState))
        &&ModelDrawState::Copy(state,mappedState,*reinterpret_cast<void**>(state),info.pRenderable,modelToWorld);
    Hooks::hkDrawModelExecute.fOriginal(ecx,mappedStateValid?&mappedState:state,mapped,bones);
}

void Hooks::dDrawModelExecute(void *ecx, void *edx, void *state, const ModelRenderInfo_t &info, void *pCustomBoneToWorld)
{
    if(!m_VR||!m_VR->m_IsVREnabled||!m_Game||!m_Game->IsInGame())
        return hkDrawModelExecute.fOriginal(ecx,state,info,pCustomBoneToWorld);
	// Work on a copy: Source shares its cached matrices with attachments and
	// other eyes. Rewriting that cache would accumulate the controller transform.
    const auto *bones = static_cast<const matrix3x4_t *>(pCustomBoneToWorld);
	const bool localPlayerBody = IsLocalPlayerBody(info);
    if(m_VR->m_IsVREnabled&&info.pModel&&m_Game->GetModelInfo()) {
        const auto* path=m_Game->GetModelInfo()->GetModelName(info.pModel);
        static std::vector<std::string> seen;
        if(path&&std::strstr(path,"portalgun")&&seen.size()<12&&std::find(seen.begin(),seen.end(),path)==seen.end()) {
            seen.emplace_back(path);PortalVrLog("Gun model path=%s customBones=%p entity=%d renderable=%p sourceReady=%d",path,pCustomBoneToWorld,info.entity_index,info.pRenderable,s_LocalWorldGunReady);
        }
    }
	if (localPlayerBody && !s_DrawingLocalPlayerBodyDirect)
	{
		s_LocalPlayerBodyOrigin = info.origin;
		s_LocalPlayerBodyAngles = info.angles;
		s_HasLocalPlayerBodyTransform = true;
	}


	if (localPlayerBody && s_DrawingLocalPlayerBodyDirect)
	{
		static int directDrawExecuteLogCounter = 0;
		if (directDrawExecuteLogCounter++ < 16)
		{
			IModelInfo *modelInfo = m_Game ? m_Game->GetModelInfo() : nullptr;
			const char *modelName = modelInfo && info.pModel
				? modelInfo->GetModelName(info.pModel) : nullptr;
			PortalVrLog("First-person direct DrawModelExecute eye=%p model=%s customBones=%p",
				m_ActiveEyeTexture, modelName ? modelName : "<unknown>",
				pCustomBoneToWorld);
		}
	}

	if (localPlayerBody && (m_VR->m_FirstPersonBodyHideUpper||m_VR->m_Graphics.bodyIK))
	{
		matrix3x4_t bodyBones[FirstPersonBody::MaxBones];
		const bool built = BuildFirstPersonBodyBones(state, bones, bodyBones, info);
		static int diagnosticBodyDraws = 0;
		if (diagnosticBodyDraws++ < 8)
		{
			IModelInfo *modelInfo = m_Game ? m_Game->GetModelInfo() : nullptr;
			const char *modelName = modelInfo && info.pModel
				? modelInfo->GetModelName(info.pModel) : nullptr;
			PortalVrLog("First-person body candidate eye=%p model=%s entity=%d renderable=%p origin=%f,%f,%f built=%d",
				m_ActiveEyeTexture, modelName ? modelName : "<unknown>",
				info.entity_index, info.pRenderable,
				info.origin.x, info.origin.y, info.origin.z, built);
		}
		StudioBodyView view;
        if (built && GetStudioBodyView(state,view)) {
            if(!s_DrawingLocalPlayerBodyDirect)return hkDrawModelExecute.fOriginal(ecx,state,info,bodyBones);
            return DrawTrackedModel(ecx,state,info,bodyBones,view.count);
        }
	}

	if (localPlayerBody && s_DrawingLocalPlayerBodyDirect)
	{
		matrix3x4_t bodyBones[FirstPersonBody::MaxBones];
		StudioBodyView view;
		if (TranslateFirstPersonBodyBones(state, bones, bodyBones) && GetStudioBodyView(state,view))
			return DrawTrackedModel(ecx,state,info,bodyBones,view.count);
	}

    if (m_VR->m_IsVREnabled && state) {
        const auto *hdr = *reinterpret_cast<const unsigned char **>(state);
        if (hdr && SigScanner::IsReadable(reinterpret_cast<uintptr_t>(hdr), 164)) {
            const char *name = reinterpret_cast<const char *>(hdr + 12);
            const int count = *reinterpret_cast<const int *>(hdr + 156);
            if(count==1&&ModelPath::Equal(name,"weapons/w_portalgun.mdl")) {
                static unsigned diagnostics=0;
                const bool owned=s_LocalWorldGunReady&&(info.pRenderable==s_LocalWorldGunRenderable||
                    (info.entity_index>0&&m_Game->GetClientEntityList()&&m_Game->GetClientEntityList()->GetClientEntity(info.entity_index)==s_LocalWorldGunEntity));
                if(diagnostics++<8)PortalVrLog("Portal world gun candidate owned=%d entity=%d sourceReady=%d renderable=%p",owned,info.entity_index,s_LocalWorldGunReady,info.pRenderable);
                if(owned&&m_VR->m_Graphics.portalGunPose&&m_VR->m_PortalAimLastSeen) {
                    if(!m_VR->m_Graphics.gun||(m_VR->m_Weapon.stowed&&!m_VR->m_Graphics.backGun))return;
                    const int length=*reinterpret_cast<const int*>(hdr+76),offset=*reinterpret_cast<const int*>(hdr+160);
                    if(length>=248&&length<=64*1024*1024&&SigScanner::IsReadable(reinterpret_cast<uintptr_t>(hdr),length)) {
                        static BindPoseCache::Entry cache;
                        const auto* bind=cache.Get(hdr,length,1,offset,*reinterpret_cast<const int*>(hdr+8));
                        if(bind) {
                            const auto trackedModel=HandPose::Concat(HandPose::Concat(m_VR->m_Weapon.stowed?m_VR->GetHolsteredGunFrame():m_VR->GetGunControllerFrame(),m_VR->m_GunFromController),GunGrab::WorldFit());
                            const auto pass=info.pModelToWorld?*info.pModelToWorld:PortalPose::Frame(info.origin,info.angles);
                            const auto model=GunGrab::InPass(s_LocalWorldGunNativeModel,pass,trackedModel);
                            matrix3x4_t tracked=HandPose::Concat(model,bind[0]);auto mapped=info;
                            mapped.origin=TwoHandAim::Position(model);mapped.pModelToWorld=&model;
                            const Vector lighting=mapped.origin;mapped.pLightingOrigin=&lighting;
                            ModelDrawState::State trackedState;
                            const bool stateMapped=SigScanner::IsReadable(reinterpret_cast<uintptr_t>(state),sizeof(trackedState))&&ModelDrawState::Copy(state,trackedState,hdr,info.pRenderable,model);
                            static unsigned stateLogs=0;if(stateLogs++<8)PortalVrLog("Tracked world gun stateMapped=%d stowed=%d back=%d",stateMapped,m_VR->m_Weapon.stowed,m_VR->m_Graphics.backGun);
                            return hkDrawModelExecute.fOriginal(ecx,stateMapped?&trackedState:state,mapped,&tracked);
                        }
                    }
                }
            }
            const auto kind = HandPose::Identify(name, count);
            const bool gun = kind == HandPose::Model::Gun;
            const bool hands = kind == HandPose::Model::Hands;
            if ((gun || hands) && bones && SigScanner::IsReadable(reinterpret_cast<uintptr_t>(bones), count * sizeof(matrix3x4_t))
                && m_VR->m_RightControllerForward.LengthSqr() > 0.9f) {
                // The secondary native mesh contains both physical hands. Draw it once,
                // including while the weapon is stowed or held in the left hand.
                const bool nativeWhole=m_VR->m_Graphics.bodyIK&&s_NativeAvatarDirectDrawn&&s_AvatarPalette.valid&&s_AvatarPalette.serial==s_AvatarFrame;
                if(hands&&nativeWhole)return;
                const bool bothHands=hands&&m_VR->m_NativeModels&&s_LeftArmRenderable;
                if(bothHands&&info.pRenderable!=s_LeftArmRenderable)return;
                if(hands&&m_VR->m_NativeModels&&!bothHands&&!NativeFit::HandModelVisible(info.pRenderable==s_LeftArmRenderable,m_VR->m_Graphics))return;
                if(m_VR->m_NativeModels) {
                    static bool loggedHands=false,loggedGun=false;
                    auto& logged=gun?loggedGun:loggedHands;
                    if(!logged) {
                        const int length=*reinterpret_cast<const int*>(hdr+76);
                        const auto materialPath=length>=232&&length<=64*1024*1024&&SigScanner::IsReadable(reinterpret_cast<uintptr_t>(hdr),length)?StudioMaterial::First(hdr,length):std::string();
                        const char* path=materialPath.c_str();
                        auto* system=m_Game->GetMaterialSystem();auto* material=system?system->FindMaterial(path,"Model textures",true):nullptr;
                        PortalVrLog("Native Chell hand material gun=%d path=%s error=%d mapping=%dx%d",gun,path,!material||material->IsErrorMaterial(),material?material->GetMappingWidth():0,material?material->GetMappingHeight():0);
                        if(hands)s_PairedHandMaterialReady=material&&!material->IsErrorMaterial();
                        logged=true;
                    }
                }
                matrix3x4_t tracked[128];
                memcpy(tracked, bones, count * sizeof(matrix3x4_t));
                // Use the model's actual bind skeleton for skinning the arms.
                // The inherited player/viewmodel animation can displace wrists
                // relative to these meshes. Gun parts retain their animation below.
                const int boneOffset = *reinterpret_cast<const int *>(hdr + 160);
                const int modelLength = *reinterpret_cast<const int *>(hdr + 76);
                if (boneOffset < 164 || modelLength < boneOffset || modelLength - boneOffset < count * 216
                    || !SigScanner::IsReadable(reinterpret_cast<uintptr_t>(hdr + boneOffset), count * 216))
                    return hkDrawModelExecute.fOriginal(ecx, state, info, pCustomBoneToWorld);
                static thread_local BindPoseCache::Entry referenceCache[2];
                const int checksum=*reinterpret_cast<const int*>(hdr+8);
                const auto* reference=referenceCache[gun?1:0].Get(hdr,modelLength,count,boneOffset,checksum);
                if(!reference)return hkDrawModelExecute.fOriginal(ecx,state,info,pCustomBoneToWorld);

                const Vector rightPosition = m_VR->WeaponPosition();
                const auto rightTarget = m_VR->GetHandFrame(false);
                if (gun) {
                    // The gun's local +Z is its barrel, +Y is up. Align its
                    // grip with the controller and retain animated gun parts.
                    matrix3x4_t source = reference[24];
                    for (int row = 0; row < 3; ++row) source[row][3] = reference[8][row][3];
                    const auto target = m_VR->GetGunControllerFrame();
                    const auto fittedGun=HandPose::FitGunToPalm(reference[24],m_VR->m_NativeModels);
                    const auto gunTarget=HandPose::Reanchor(fittedGun,source,target);
                    m_VR->m_GunFromController=HandPose::Concat(HandPose::InverseRigid(source),fittedGun);
                    m_VR->m_GunWristFromController=HandPose::Concat(m_VR->m_GunFromController,
                        HandPose::Concat(HandPose::InverseRigid(reference[24]),reference[8]));
                    if(!m_VR->m_NativeModels||!s_LeftArmRenderable||!s_PairedHandMaterialReady) {
                        auto wristTarget=rightTarget;
                        if(m_VR->m_NativeModels&&m_VR->m_Graphics.gripHand) {
                            wristTarget=TwoHandAim::GripWrist(reference[8],reference[24],gunTarget);
                            const auto& s=m_VR->m_Graphics;
                            const auto position=NativeFit::Offset(TwoHandAim::Position(wristTarget),TwoHandAim::Axis(target,2),
                                -TwoHandAim::Axis(target,0),TwoHandAim::Axis(target,1),s.rightX,s.rightY,s.rightZ,m_VR->m_VRScale);
                            wristTarget=NativeFit::Rotate(wristTarget,s.rightPitch,s.rightYaw,s.rightRoll);
                            for(int r=0;r<3;++r)wristTarget[r][3]=position[r];
                        }
                        for(int i=0;i<24;++i)tracked[i]=m_VR->m_NativeModels?
                            HandPose::Reanchor(reference[i],reference[8],wristTarget):HandPose::Reanchor(reference[i],source,target);
                    }
                    static const void* metadataHeader=nullptr;
                    static int metadataChecksum=0,metadataLength=0;static bool metadataNative=false,metadataValid=false,socketValid=false;
                    static matrix3x4_t cachedSocket;
                    if(metadataHeader!=hdr||metadataChecksum!=checksum||metadataLength!=modelLength||metadataNative!=m_VR->m_NativeModels) {
                        metadataHeader=hdr;metadataChecksum=checksum;metadataLength=modelLength;metadataNative=m_VR->m_NativeModels;
                        const bool readable=modelLength>=248&&modelLength<=64*1024*1024&&SigScanner::IsReadable(reinterpret_cast<uintptr_t>(hdr),modelLength);
                        metadataValid=readable&&s_GunAttachments.Read(hdr,modelLength,reference,m_VR->m_NativeModels);
                        socketValid=readable&&OptionalGunGrip::ReadSocket(hdr,modelLength,cachedSocket,m_VR->m_NativeModels);
                        PortalVrLog("Gun metadata cached model=%p checksum=%d attachments=%d socket=%d",hdr,checksum,metadataValid,socketValid);
                    }
                    if (info.pRenderable == s_RightViewmodelRenderable) {
                        s_GunAttachmentRenderable = nullptr;
                        m_VR->m_PortalAimLastSeen = 0;
                        if (metadataValid) {
                            s_GunAttachmentRenderable = info.pRenderable;
                            s_GunAttachmentModel = info.pModel;
                            if (s_GunAttachments.hasBarrel) {
                                m_VR->m_PortalAimFromController = s_GunAttachments.barrelFromController;
                                m_VR->m_PortalAimLastSeen = GetTickCount64();
                            } else m_VR->m_PortalAimLastSeen = 0;
                        }
                    }
                    for (int i = 24; i < count; ++i) tracked[i] = HandPose::Reanchor(bones[i], bones[24], gunTarget);
                    if (info.pRenderable == s_GunAttachmentRenderable && s_GunAttachments.hasBarrel) {
                        // Compare the actual submitted skeleton with the shot
                        // line; synthetic rigid-bone tests cannot expose a
                        // native animation or viewmodel basis distortion.
                        const auto& muzzle = s_GunAttachments.attachments[0];
                        const auto drawn = HandPose::Concat(tracked[muzzle.bone],muzzle.local);
                        Vector drawOrigin,drawDirection,aimOrigin,aimDirection;
                        if (GunRay::FromBarrel(drawn,rightPosition,drawOrigin,drawDirection)
                            && m_VR->GetPortalAimRay(aimOrigin,aimDirection)) {
                            s_DrawnBarrelDirectionError = sqrtf((drawDirection-aimDirection).LengthSqr());
                            s_DrawnBarrelLineError = sqrtf((drawOrigin-aimOrigin).LengthSqr());
                        }
                        s_NativeGunBasisError = 0;
                        for (int r=0;r<3;++r) for (int c=0;c<3;++c) {
                            float dot=0;
                            for (int k=0;k<3;++k) dot+=bones[24][k][r]*bones[24][k][c];
                            s_NativeGunBasisError=std::fmax(s_NativeGunBasisError,fabsf(dot-(r==c?1.f:0.f)));
                        }
                    }
                    if(m_VR->m_NativeModels&&!s_LeftArmRenderable&&m_VR->m_Graphics.fingerTracking)NativeFingers::Gun(reference,tracked,m_VR->m_RightFingerCurl,m_VR->m_Graphics.fingerRange/100);
                    else if(!m_VR->m_NativeModels)HandPose::ApplyGunGrip(reference,tracked,m_VR->m_RightFingerCurl);
                    if (socketValid) {
                        // Keep the socket relative to the gun controller, so
                        // either eye and either draw order use today's tracking.
                        if(!m_VR->m_OptionalSupportActive)m_VR->m_SupportFromController=HandPose::Concat(m_VR->m_GunFromController,cachedSocket);
                        m_VR->m_SupportLastSeen = GetTickCount64();
                    } else {
                        m_VR->m_SupportLastSeen = 0;
                        m_VR->m_OptionalGripState.active = false;
                        m_VR->m_OptionalSupportActive = false;
                    }
                } else {
                    const auto leftTarget=m_VR->m_NativeModels?m_VR->GetAvatarHandFrame(true):m_VR->GetHandFrame(true);
                    const auto physicalRight=m_VR->m_NativeModels?m_VR->GetAvatarHandFrame(false):rightTarget;
                    const bool primaryPhysicalLeft=(m_VR->WeaponHand()==0)!=m_VR->m_LeftHanded;
                    const bool supporting=m_VR->m_OptionalSupportActive;
                    HandPose::AlignBareArms(reference, tracked, leftTarget, physicalRight);
                    static const float supportCurl[5] = {0.30f, 0.40f, 0.45f, 0.45f, 0.45f};
                    if(m_VR->m_NativeModels&&m_VR->m_Graphics.fingerTracking) {
                        const float* leftCurl=m_VR->m_LeftHanded?m_VR->m_RightFingerCurl:m_VR->m_LeftFingerCurl;
                        const float* rightCurl=m_VR->m_LeftHanded?m_VR->m_LeftFingerCurl:m_VR->m_RightFingerCurl;
                        NativeFingers::TrackedHand(reference,tracked,leftCurl,true,!m_VR->m_Weapon.stowed&&(primaryPhysicalLeft||supporting),m_VR->m_Graphics.fingerRange/100);
                        NativeFingers::TrackedHand(reference,tracked,rightCurl,false,!m_VR->m_Weapon.stowed&&(!primaryPhysicalLeft||supporting),m_VR->m_Graphics.fingerRange/100);
                    }
                    else if(!m_VR->m_NativeModels)HandPose::ApplyFingerCurl(reference, tracked,
                        supporting ? supportCurl : m_VR->m_LeftFingerCurl, m_VR->m_RightFingerCurl);
                    if (s_LeftArmRenderable&&!bothHands) {
                        const bool leftOnly = info.pRenderable == s_LeftArmRenderable;
                        const Vector hiddenAt = leftOnly ? m_VR->GetLeftHandAbsPos() : rightPosition;
                        const auto collapsed = HandPose::Frame({0,0,0},{0,0,0},{0,0,0},hiddenAt);
                        for (int i = 0; i < count; ++i)
                            if (i < 5 || (leftOnly ? i >= 24 : i < 24)) tracked[i] = collapsed;
                    }

                }
                static int logged = 0;
                if (logged++ < 6) PortalVrLog("Hand-anchored model=%s wrist=%f,%f,%f", name,
                    tracked[gun ? 8 : 27][0][3],tracked[gun ? 8 : 27][1][3],tracked[gun ? 8 : 27][2][3]);
                if(m_VR->m_NativeModels) {
                    if(bothHands) {
                        const auto& s=m_VR->m_Graphics;
                        bool showLeft=s.arms!=2&&(m_VR->m_LeftHanded?s.rightArm:s.leftArm);
                        bool showRight=s.arms!=2&&(m_VR->m_LeftHanded?s.leftArm:s.rightArm);
                        const bool primaryLeft=(m_VR->WeaponHand()==0)!=m_VR->m_LeftHanded;
                        if(m_VR->m_OptionalSupportActive&&s.showSupportHand)(primaryLeft?showRight:showLeft)=true;
                        if(m_VR->m_Weapon.stowed&&s.holsterHands)showLeft=showRight=true;
                        NativeFit::BothHandsVisibility(tracked,s.bodyIK?1:s.arms,showLeft,showRight);
                    } else {
                        auto s=m_VR->m_Graphics;
                        if(gun&&(nativeWhole||(s_LeftArmRenderable&&s_PairedHandMaterialReady)))s.arms=2; // hands live in the paired native mesh
                        if(gun&&m_VR->m_Weapon.stowed)s.gun=false;
                        NativeFit::Visibility(tracked,count,gun,info.pRenderable==s_LeftArmRenderable,s);
                    }
                }
                return DrawTrackedModel(ecx,state,info,tracked,count);
            }
        }
    }
    return hkDrawModelExecute.fOriginal(ecx, state, info, pCustomBoneToWorld);
}

void Hooks::dPushRenderTargetAndViewport(void *ecx, void *edx, ITexture *pTexture, ITexture *pDepthTexture, int nViewX, int nViewY, int nViewW, int nViewH)
{
	if (m_VR->m_CreatedVRTextures && !m_PushedHud)
	{
		auto *materialSystem = m_Game->GetMaterialSystem();
		if (!materialSystem)
			return hkPushRenderTargetAndViewport.fOriginal(ecx, pTexture, pDepthTexture, nViewX, nViewY, nViewW, nViewH);

		pTexture = m_VR->m_HUDTexture;

		//pTexture = m_VR->m_RightEyeTexture;

		IMatRenderContext *renderContext = materialSystem->GetRenderContext();
		renderContext->ClearBuffers(false, true, true);
		renderContext->Release();

		hkPushRenderTargetAndViewport.fOriginal(ecx, pTexture, pDepthTexture, nViewX, nViewY, nViewW, nViewH);

		renderContext = materialSystem->GetRenderContext();
		renderContext->OverrideAlphaWriteEnable(true, true);
		renderContext->ClearColor4ub(0, 0, 0, 0);
		renderContext->ClearBuffers(true, false);
		renderContext->Release();

		m_VR->m_RenderedHud = true;
		m_PushedHud = true;
	}
	else
	{
		hkPushRenderTargetAndViewport.fOriginal(ecx, pTexture, pDepthTexture, nViewX, nViewY, nViewW, nViewH);
	}
}

void Hooks::dPopRenderTargetAndViewport(void *ecx, void *edx)
{
	if (!m_VR->m_CreatedVRTextures)
		return hkPopRenderTargetAndViewport.fOriginal(ecx);

	//std::cout << "dPopRenderTargetAndViewport: " << m_PushHUDStep << "\n";

	m_PushHUDStep = 0;

	if (m_PushedHud)
	{
		if (IMaterialSystem *materialSystem = m_Game->GetMaterialSystem())
		{
			IMatRenderContext* renderContext = materialSystem->GetRenderContext();
			renderContext->OverrideAlphaWriteEnable(false, true);
			renderContext->ClearColor4ub(0, 0, 0, 255);
			renderContext->Release();
		}
	}

	hkPopRenderTargetAndViewport.fOriginal(ecx);
}

void Hooks::dVGui_Paint(void *ecx, void *edx, int mode)
{
	if (!m_VR->m_CreatedVRTextures || m_Game->IsCursorVisible())
		return hkVgui_Paint.fOriginal(ecx, mode | (m_Game->IsCursorVisible()?PAINT_UIPANELS:0));

	//std::cout << "dVGui_Paint\n";

	if (m_PushedHud)
		mode = PAINT_UIPANELS | PAINT_INGAMEPANELS;

	hkVgui_Paint.fOriginal(ecx, mode);
}

int Hooks::dIsSplitScreen()
{
	//std::cout << "dIsSplitScreen: " << m_PushHUDStep << "\n";

	if (m_PushHUDStep == 0)
		++m_PushHUDStep;
	else
		m_PushHUDStep = -999;

	return hkIsSplitScreen.fOriginal();
}

DWORD *Hooks::dPrePushRenderTarget(void *ecx, void *edx, int a2)
{
	//std::cout << "dPrePushRenderTarget: " << m_PushHUDStep << "\n";

	if (m_PushHUDStep == 1)
		++m_PushHUDStep;
	else
		m_PushHUDStep = -999;

	return hkPrePushRenderTarget.fOriginal(ecx, a2);
}

Vector* Hooks::dWeapon_ShootPosition(void* ecx, void* edx, Vector* shootPos)
{
	Vector* result = hkWeapon_ShootPosition.fOriginal(ecx, shootPos);

	if (!result || !m_Game || !m_VR)
		return result;

	const int localIndex = m_Game->GetLocalPlayerIndex();
	const int index = ServerEntityIndex(ecx);
	if (index < 0 || index >= static_cast<int>(m_Game->m_PlayersVRInfo.size()))
		return result;

	const auto &vrPlayer = m_Game->m_PlayersVRInfo[index];

	if (m_VR->m_IsVREnabled && localIndex > 0 && localIndex == index
		&& m_VR->WeaponTracked()) {
		Vector hand;
		QAngle angles;
		if (ServerHandPose(ecx, hand, angles)) *result = hand;
	}
	else if (vrPlayer.isUsingVR)
	{
		*result = vrPlayer.controllerPos;
	}

	return result;
}

void* Hooks::dCWeaponPortalgun_FirePortal(void* ecx, void* edx, bool isSecondaryPortal, Vector* pVector) {
	auto result = hkCWeaponPortalgun_FirePortal.fOriginal(ecx, isSecondaryPortal, pVector);

	return result;
}

float __fastcall Hooks::dTraceFirePortal(void* ecx, void* edx, bool secondary,
    const Vector& start, const Vector& direction, void* trace,
    Vector& finalPosition, QAngle& finalAngles, int placedBy, bool test)
{
    Vector shotStart = start;
    Vector shotDirection = direction;
    Vector aimStart, aimDirection;
    if (!test) s_PortalBlast.pending=false;
    if (placedBy == 2 && m_VR->GetPortalAimRay(aimStart,aimDirection)) {
        shotStart = aimStart;
        shotDirection = aimDirection;
        if (!test && m_VR->m_PortalAimLastSeen) {
            const auto controller = m_VR->GetGunControllerFrame();
            s_PortalBlast.muzzle = HandPose::Concat(controller,m_VR->m_PortalAimFromController);
            s_PortalBlast.pending = true;
        }
        static int logged = 0;
        if (!test && logged++ < 20)
            PortalVrLog("Controller portal shot secondary=%d hand=%f,%f,%f direction=%f,%f,%f headDirection=%f,%f,%f",
                secondary,shotStart.x,shotStart.y,shotStart.z,
                shotDirection.x,shotDirection.y,shotDirection.z,direction.x,direction.y,direction.z);
    }
    const float result = hkTraceFirePortal.fOriginal(ecx, secondary, shotStart, shotDirection, trace,
        finalPosition, finalAngles, placedBy, test);
    static unsigned diagnosed=0;
    if (!test && placedBy==2 && diagnosed++<2000 && trace) {
        const auto& hit=*static_cast<const CGameTrace*>(trace);
        const auto delta=hit.endpos-shotStart;
        const float along=delta.x*shotDirection.x+delta.y*shotDirection.y+delta.z*shotDirection.z;
        const float lineError=sqrtf((delta-shotDirection*along).LengthSqr());
        PortalVrLog("Portal aim audit color=%s result=%f roll=%f cacheAge=%llu drawDirectionError=%f drawLineError=%f nativeBasisError=%f traceLineError=%f placementShift=%f origin=%f,%f,%f direction=%f,%f,%f hit=%f,%f,%f final=%f,%f,%f normal=%f,%f,%f",
            secondary?"orange":"blue",result,m_VR->WeaponHand()==0?m_VR->m_LeftControllerAngAbs.z:m_VR->m_RightControllerAngAbs.z,
            m_VR->m_PortalAimLastSeen?GetTickCount64()-m_VR->m_PortalAimLastSeen:0,
            s_DrawnBarrelDirectionError,s_DrawnBarrelLineError,s_NativeGunBasisError,lineError,
            sqrtf((finalPosition-hit.endpos).LengthSqr()),shotStart.x,shotStart.y,shotStart.z,
            shotDirection.x,shotDirection.y,shotDirection.z,hit.endpos.x,hit.endpos.y,hit.endpos.z,
            finalPosition.x,finalPosition.y,finalPosition.z,hit.plane.normal.x,hit.plane.normal.y,hit.plane.normal.z);
    }
    return result;
}

void __cdecl Hooks::dDispatchEffect(const char* name,const void* data)
{
    // Native FirePortal builds the blast after TraceFirePortal returns. Its
    // unchanged locals still describe head aim, even when placement was fixed.
    // Only replace this verified call's launch frame; retain its destination.
    if (s_PortalBlast.pending && m_VR->m_IsVREnabled
        && reinterpret_cast<uintptr_t>(_ReturnAddress())==m_Game->m_BaseServer+0x46ae40
        && name && !strcmp(name,"PortalBlast")) {
        s_PortalBlast.pending=false;
        if (data && SigScanner::IsReadable(reinterpret_cast<uintptr_t>(data),sizeof(PortalShotFx::Data))) {
            PortalShotFx::Data corrected;
            memcpy(&corrected,data,sizeof(corrected));
            const auto before=corrected;
            if (PortalShotFx::Align(corrected,s_PortalBlast.muzzle)) {
                {
                    std::lock_guard<std::mutex> lock(s_PortalLaunchMutex);
                    s_PortalLaunches.Record(corrected,GetTickCount64());
                }
                Vector originalDirection,direction;
                QAngle::AngleVectors(before.angles,&originalDirection,nullptr,nullptr);
                QAngle::AngleVectors(corrected.angles,&direction,nullptr,nullptr);
                static unsigned reports=0;
                if (reports++<2000) PortalVrLog("Portal blast aligned color=%u originShift=%f directionChange=%f muzzle=%f,%f,%f target=%f,%f,%f direction=%f,%f,%f",
                    PortalShotFx::Color(corrected),
                    sqrtf((before.origin-corrected.origin).LengthSqr()),sqrtf((originalDirection-direction).LengthSqr()),
                    corrected.origin.x,corrected.origin.y,corrected.origin.z,corrected.start.x,corrected.start.y,corrected.start.z,
                    direction.x,direction.y,direction.z);
                return hkDispatchEffect.fOriginal(name,&corrected);
            }
        }
    }
    hkDispatchEffect.fOriginal(name,data);
}

void __cdecl Hooks::dPortalBlastCallback(const void* data)
{
    // Restore the matched shot's exact launch after lossy effect serialization.
    // Retain the received target, timing, color and all other engine metadata.
    if (m_VR->m_IsVREnabled && data
        && SigScanner::IsReadable(reinterpret_cast<uintptr_t>(data),PortalShotFx::ClientPayloadSize)) {
        static unsigned reports=0;
        PortalShotFx::Data effect{};
        memcpy(&effect,data,PortalShotFx::ClientPayloadSize);
        Vector direction;QAngle::AngleVectors(effect.angles,&direction,nullptr,nullptr);
        if (reports++<2000) PortalVrLog("Portal blast received color=%u muzzle=%f,%f,%f target=%f,%f,%f direction=%f,%f,%f",
            PortalShotFx::Color(effect),effect.origin.x,effect.origin.y,effect.origin.z,
            effect.start.x,effect.start.y,effect.start.z,direction.x,direction.y,direction.z);
        bool restored;
        {
            std::lock_guard<std::mutex> lock(s_PortalLaunchMutex);
            restored=s_PortalLaunches.Restore(effect,GetTickCount64());
        }
        if(restored) {
            QAngle::AngleVectors(effect.angles,&direction,nullptr,nullptr);
            if(reports<=2000) PortalVrLog("Portal blast precision restored color=%u muzzle=%f,%f,%f direction=%f,%f,%f",
                PortalShotFx::Color(effect),effect.origin.x,effect.origin.y,effect.origin.z,
                direction.x,direction.y,direction.z);
            return hkPortalBlastCallback.fOriginal(&effect);
        }
    }
    hkPortalBlastCallback.fOriginal(data);
}

void __fastcall Hooks::dPlayerPortalled(void* ecx, void* edx, void* portal)
{
	// The verified Portal 1 function copies the portal's VMatrix at +0x864.
	// Copy before calling the original; only the local player owns our tracking.
	matrix3x4_t transform;
	const auto matrixAddress = reinterpret_cast<uintptr_t>(portal) + 0x864;
	const bool apply = portal && m_VR && m_Game && m_VR->m_IsVREnabled
		&& ecx == m_Game->GetLocalPortalPlayer()
		&& SigScanner::IsReadable(matrixAddress, sizeof(transform));
	if (apply) memcpy(&transform, reinterpret_cast<const void *>(matrixAddress), sizeof(transform));
	hkPlayerPortalled.fOriginal(ecx, portal);
	if (!apply) return;

	const float yaw = PortalPose::UprightYawDelta(transform, m_VR->m_HmdAngAbs);
	m_VR->m_RotationOffset.y = std::remainder(m_VR->m_RotationOffset.y + yaw, 360.0f);
	m_VR->m_SetupOrigin = PortalPose::Position(HandPose::Concat(transform,
		PortalPose::Frame(m_VR->m_SetupOrigin, {0,0,0})));
	s_PropCarry.Reset();s_PropPlayer=nullptr;
	m_VR->m_PropHold.CancelThrow();
	m_VR->m_HandContact[0]=m_VR->m_HandContact[1]={};m_VR->m_ThrowReleaseUntil=0;
	m_VR->m_CameraCollisionOffset = {0,0,0};
	m_VR->m_CameraBlocked = false;
	m_VR->m_CalibrationDrift.Reset();
	m_VR->m_CalibrationSuppressUntil=GetTickCount64()+2000;
	// Update both hands and the roomscale offset in this callback, before a
	// render or pickup update can consume an entry-side pose. No distance gate.
	m_VR->UpdateTracking();
	m_VR->SnapshotGrabPose();
	PortalVrLog("Player portalled: tracking yaw delta=%f origin=%f,%f,%f", yaw,
		m_VR->m_SetupOrigin.x, m_VR->m_SetupOrigin.y, m_VR->m_SetupOrigin.z);
}

void __fastcall Hooks::dPlayerUse(void* ecx, void* edx)
{
	if(m_VR->m_PropHold.release&&m_Game->GetLocalPlayerIndex()>0&&ServerEntityIndex(ecx)==m_Game->GetLocalPlayerIndex()) {
        void* parent=s_PropBinding.PickupOwner(s_PropController,ecx);
        if(parent) {
            m_VR->SnapshotGrabPose();s_PropPlayer=ecx;s_ReleaseVelocity={};s_GripRelease=true;
            reinterpret_cast<void(__thiscall*)(void*,bool)>(s_PropBinding.shutdownPickup)(parent,false);
            s_GripRelease=false;ApplyPropReleaseVelocity();
            s_PropCarry.Reset();s_PropPlayer=s_PropController=nullptr;m_VR->m_PropHold.Detached();m_VR->m_ThrowReleaseUntil=0;
            PortalVrLog("Native pickup shutdown completed on grip release");
            return;
        }
        m_VR->m_PropHold.Detached();s_PropCarry.Reset();s_PropPlayer=s_PropController=nullptr;
    }
	const bool previous = m_VR->m_OverrideEyeAngles;
	m_VR->m_OverrideEyeAngles = true;
	hkPlayerUse.fOriginal(ecx);
	m_VR->m_OverrideEyeAngles = previous;
}

void* __fastcall Hooks::dFindUseEntity(void* ecx, void* edx)
{
	if (!m_Game || !m_VR || !m_VR->m_IsVREnabled
		|| !m_VR->m_OverrideEyeAngles || !m_VR->m_GrabPoseValid
		|| m_Game->GetLocalPlayerIndex() <= 0
		|| ServerEntityIndex(ecx) != m_Game->GetLocalPlayerIndex())
		return hkFindUseEntity.fOriginal(ecx);

	PickupQueryPose selection{ecx};
	const bool aligned = ServerPickupAim(ecx,selection.origin,selection.angles);
	void* entity;
	if (aligned) {
		// Only selection sees the barrel ray. Keep native reach, usability,
		// visibility, and portal checks; never return a raw trace hit as usable.
		ScopedPickupQuery scope(selection);
		entity = hkFindUseEntity.fOriginal(ecx);
	} else entity = hkFindUseEntity.fOriginal(ecx);
	static unsigned aimReports=0;
	if (aligned && aimReports++<256) {
		Vector visibleOrigin,visibleDirection;
		if (m_VR->GetPortalAimRay(visibleOrigin,visibleDirection)) {
			Vector serverDirection;
			QAngle::AngleVectors(selection.angles,&serverDirection,nullptr,nullptr);
			PortalVrLog("Pickup barrel query entity=%p origin=%f,%f,%f direction=%f,%f,%f visibleOriginDelta=%f visibleDirectionDelta=%f",
				entity,selection.origin.x,selection.origin.y,selection.origin.z,
				serverDirection.x,serverDirection.y,serverDirection.z,
				sqrtf((selection.origin-visibleOrigin).LengthSqr()),
				sqrtf((serverDirection-visibleDirection).LengthSqr()));
		}
	}
    if(entity&&m_VR->m_Graphics.propGrabMode==1&&s_PropBinding.getAttached) {
        matrix3x4_t object;Vector hand;QAngle angles;
        if(NativePhysics::Position(NativePhysics::Physics(entity),object)&&ServerHandPose(ecx,hand,angles)
            &&(PortalPose::Position(object)-hand).LengthSqr()>powf(m_VR->m_Graphics.handGrabReachCM*m_VR->m_VRScale/100,2))entity=nullptr;
    }
    if (entity) return entity;

	Vector hand, eye;
	QAngle handAngles;
	if (!ServerHandPose(ecx, hand, handAngles)) return nullptr;
	const Vector* actualEye = hkEyePosition.fOriginal(ecx, &eye);
	auto* traceEngine = m_Game->GetServerEngineTrace();
	if (!actualEye || !traceEngine) return nullptr;
	eye = *actualEye;
	Ray_t sweep;
	sweep.Init(eye, hand, {-2,-2,-2}, {2,2,2});
	CGameTrace trace;
	CTraceFilterSkipEntity filter(reinterpret_cast<IHandleEntity*>(ecx), 0);
	// Same contents as Portal 1 FindUseEntity, with server entities throughout.
	traceEngine->TraceRay(sweep, 0x0601400b, &filter, &trace);
	PickupQueryPose query{ecx};
	if (!trace.m_pEnt || ServerEntityIndex(trace.m_pEnt) <= 0
		|| !PickupTrace::ContactQuery(eye, hand, trace.fraction,
			trace.startsolid, trace.allsolid, query.origin, query.angles)) return nullptr;

	// Only the second selection query sees this temporary origin. Attachment,
	// held-object physics and portal traversal keep using the real controller.
	{
		ScopedPickupQuery scope(query);
		entity = hkFindUseEntity.fOriginal(ecx);
	}
	const bool matched = entity == trace.m_pEnt;
	static unsigned int reports = 0;
	if (reports++ < 32)
		PortalVrLog("Near-contact pickup entity=%p matched=%d handDistance=%f",
			trace.m_pEnt, matched, sqrtf((hand-trace.endpos).LengthSqr()));
	return matched ? entity : nullptr;
}

int Hooks::dGetModeHeight(void* ecx, void* edx) {
	//std::cout << "dGetModeHeight\n";
	return m_VR->m_RenderHeight;
}

bool Hooks::dClipTransform(const Vector& point, Vector* pScreen)
{
	return hkClipTransform.fOriginal(point, pScreen);
}

bool Hooks::ScreenTransform(const Vector& point, Vector* pScreen, int width, int height)
{
	bool retval = hkClipTransform.fOriginal(point, pScreen);

	pScreen->x = 0.5f * (pScreen->x + 1.0f) * width;
	pScreen->y = 0.5f * (-pScreen->y + 1.0f) * height;

	return retval;
}

int __fastcall Hooks::dDrawSelf(void* ecx, void* edx, int x, int y, int w, int h, const void* clr, float flApparentZ) {
	//std::cout << "dDrawSelf - X: " << x << ", Y: " << y << ", W: " << w << ", H: " << h << ", Z: " << flApparentZ << "\n";

	//int playerIndex = m_Game->m_EngineClient->GetLocalPlayer();

	//auto viewport = m_Game->m_ClientMode->GetViewport();

	int newX = x;
	int	newY = y;

	if (m_VR->m_IsVREnabled)
	{
		int windowWidth, windowHeight;
		if (IMaterialSystem *materialSystem = m_Game->GetMaterialSystem())
		{
			IMatRenderContext *renderContext = materialSystem->GetRenderContext();
			renderContext->GetWindowSize(windowWidth, windowHeight);
			renderContext->Release();
		}
		else
		{
			windowWidth = m_VR->m_RenderWidth;
			windowHeight = m_VR->m_RenderHeight;
		}

		Vector screen = { 0, 0, 0 };

		//Vector vec = m_VR->m_AimPos - m_VR->GetRightControllerAbsPos();

		//newZ = 1.0 / sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);

		ScreenTransform(m_VR->m_AimPos, &screen, m_VR->m_RenderWidth, m_VR->m_RenderHeight);

		int offsetX = x - (windowWidth * 0.5f);
		int offsetY = y - (windowHeight * 0.5f);

		newX = screen.x + offsetX;
		newY = screen.y + offsetY;
	}

	return hkDrawSelf.fOriginal(ecx, newX, newY, w, h, clr, flApparentZ);
}

void __cdecl Hooks::dVGui_GetHudBounds(int slot, int& x, int& y, int& w, int& h) {
	if (m_VR->m_IsVREnabled && !m_Game->IsCursorVisible())
	{
		x = y = 0;
		w = m_VR->m_RenderWidth;
		h = m_VR->m_RenderHeight;
	} else {
		hkVGui_GetHudBounds.fOriginal(slot, x, y, w, h);
	}

	//std::cout << "dVGui_GetHudBounds - X: " << x << ", Y: " << y << ", W: " << w << ", H: " << h << "\n";
}

void __cdecl Hooks::dVGui_GetPanelBounds(int slot, int& x, int& y, int& w, int& h) {
	if (m_VR->m_IsVREnabled && !m_Game->IsCursorVisible())
	{
		x = y = 0;
		w = m_VR->m_RenderWidth;
		h = m_VR->m_RenderHeight;
	}
	else {
		hkVGui_GetPanelBounds.fOriginal(slot, x, y, w, h);
	}

	//std::cout << "dVGui_GetPanelBounds - X: " << x << ", Y: " << y << ", W: " << w << ", H: " << h << "\n";
}

void __cdecl Hooks::dVGUI_UpdateScreenSpaceBounds(int nNumSplits, int sx, int sy, int sw, int sh) {
	hkVGUI_UpdateScreenSpaceBounds.fOriginal(nNumSplits, sx, sy, m_VR->m_RenderWidth, m_VR->m_RenderHeight);
}

void __cdecl Hooks::dVGui_GetTrueScreenSize(int &w, int &h) {
	w = m_VR->m_RenderWidth;
	h = m_VR->m_RenderHeight;
}

void __fastcall Hooks::dGetScreenSize(void* ecx, void* edx, int& wide, int& tall) {
	//hkGetScreenSize.fOriginal(ecx, wide, tall);
	wide = m_VR->m_RenderWidth;
	tall = m_VR->m_RenderHeight;
}

void __cdecl Hooks::dGetHudSize(int& w, int& h) {
	w = m_VR->m_RenderWidth;
	h = m_VR->m_RenderHeight;
}

void __fastcall Hooks::dPush2DView(void* ecx, void* edx, const CViewSetup& view, int nFlags, ITexture* pRenderTarget, void* frustumPlanes) {
	hkPush2DView.fOriginal(ecx, view, nFlags, pRenderTarget, frustumPlanes);
	s_BodyViewStack.push_back(false);
}

void __fastcall Hooks::dPopView(void* ecx, void* edx, void* frustumPlanes) {
	if (!s_BodyViewStack.empty()) {
		const bool mainWorldView = s_BodyViewStack.back();
		if (mainWorldView && s_InlineBodyDrawEligible && !s_BodyDrawTriggered) {
			s_BodyDrawTriggered = true;
			s_ActiveFirstPersonBodyPass = true;
			DrawLocalPlayerBodyDirect();
			s_ActiveFirstPersonBodyPass = false;
		}
		s_BodyViewStack.pop_back();
	}
	hkPopView.fOriginal(ecx, frustumPlanes);
}

void __fastcall Hooks::dRender(void* ecx, void* edx, vrect_t* rect) {
	//std::cout << "dRender - X: " << rect->x << ", Y: " << rect->y << ", W: " << rect->width << ", H: " << rect->height  << "\n";

	return hkRender.fOriginal(ecx, rect);
}

void __fastcall Hooks::dSetBounds(void* ecx, void* edx, int x, int y, int w, int h) {
	std::cout << "dSetBounds - X: " << x << ", Y: " << y << ", W: " << w << ", H: " << h << "\n";

	hkSetBounds.fOriginal(ecx, x, y, m_VR->m_RenderWidth, m_VR->m_RenderHeight);
}

void __fastcall Hooks::dSetSize(void* ecx, void* edx, int wide, int tall) {
	hkSetSize.fOriginal(ecx, wide, tall);

	//std::cout << "dSetSize - Wide: " << wide << ", Tall: " << tall  << "\n";
}

void __fastcall Hooks::dGetClipRect(void* ecx, void* edx, int& x0, int& y0, int& x1, int& y1) {
	hkGetClipRect.fOriginal(ecx, x0, y0, x1, y1);

	//std::cout << "dGetClipRect - X: " << x0 << ", Y: " << y0 << ", W: " << x1 << ", H: " << y1  << "\n";
}

Vector* __fastcall Hooks::dEyePosition(void* ecx, void* edx, Vector* eyePos)
{
	Vector* result = hkEyePosition.fOriginal(ecx, eyePos);

	if (!result || !m_Game || !m_VR)
		return result;
	if (s_PickupQuery && s_PickupQuery->player == ecx) {
		*result = s_PickupQuery->origin;
		return result;
	}

	const int localIndex = m_Game->GetLocalPlayerIndex();
	const int index = ServerEntityIndex(ecx);
	const bool isLocalPlayer = localIndex > 0 && localIndex == index;
	if (m_VR->m_IsVREnabled && isLocalPlayer
		&& m_VR->m_GrabPoseValid
		&& m_VR->m_OverrideEyeAngles)
	{
		Vector hand;
		QAngle angles;
		if (ServerHandPose(ecx, hand, angles)) *result = hand;
		static bool logged = false;
		if (!logged)
		{
			PortalVrLog("Scoped controller EyePosition override origin=%f,%f,%f",
				result->x, result->y, result->z);
			logged = true;
		}
	}

	return result;
}

float __fastcall Hooks::dComputeError(void* ecx, void* edx) {
	const bool wasTrue = m_VR->m_OverrideEyeAngles;
	m_VR->m_OverrideEyeAngles = true;
	const float value = hkComputeError.fOriginal(ecx);
	m_VR->m_OverrideEyeAngles = wasTrue;
	return value;
}

bool __fastcall Hooks::dUpdateObject(void* ecx, void* edx, void* pPlayer, float flError) {
	const bool wasTrue = m_VR->m_OverrideEyeAngles;
	ScopedCarryPlayer carry(pPlayer);

	m_VR->m_OverrideEyeAngles = true;

    const bool local=m_VR->InteractionAllowed()&&ServerEntityIndex(pPlayer)==m_Game->GetLocalPlayerIndex();
    const bool observe=local&&(m_VR->m_Graphics.propGrabMode==1||m_VR->m_Graphics.enhancedThrow)&&s_PropBinding.getAttached;
    const auto previous=s_PropUpdate;s_PropUpdate=observe?ecx:nullptr;
    if(observe) {
        s_PropPlayer=pPlayer;void* entity=s_PropBinding.Attached(ecx);matrix3x4_t hand,object;
        if(entity&&s_PropBinding.PickupOwner(ecx,pPlayer)){s_PropController=ecx;m_VR->m_PropHold.Attach();}
        if(entity&&PropHandPose(hand)&&NativePhysics::Position(NativePhysics::Physics(entity),object)) {
            static unsigned observations=0;
            if(s_PropCarry.controller!=ecx&&observations++<32)PortalVrLog("Native prop carry controller=%p entity=%p mode=%d hand=%d initialGapCM=%f",ecx,entity,m_VR->m_Graphics.propGrabMode,m_VR->PropHand(),sqrtf((PortalPose::Position(object)-PortalPose::Position(hand)).LengthSqr())/m_VR->m_VRScale*100);
            s_PropCarry.Target(ecx,entity,m_VR->m_Graphics.propGrabMode,m_VR->PropHand(),PropTime(),hand,object,m_VR->m_VRScale);
        } else s_PropCarry.Reset();
    } else if(s_PropCarry.controller==ecx)s_PropCarry.Reset();
    const bool value = hkUpdateObject.fOriginal(ecx, pPlayer, flError);
    s_PropUpdate=previous;
    if(!value&&s_PropCarry.controller==ecx)s_PropCarry.Reset();
	if (value && m_VR->m_IsVREnabled && ServerEntityIndex(pPlayer)==m_Game->GetLocalPlayerIndex())
		m_VR->m_LastCarryUpdate = GetTickCount64();

	m_VR->m_OverrideEyeAngles = wasTrue;

	return value;
}

bool __fastcall Hooks::dUpdateObjectVM(void* ecx, void* edx, void* pPlayer, float flError) {
	bool wasTrue = m_VR->m_OverrideEyeAngles;

	m_VR->m_OverrideEyeAngles = true;

	bool value = hkUpdateObjectVM.fOriginal(ecx, pPlayer, flError);
	if (value && m_VR->m_IsVREnabled && ServerEntityIndex(pPlayer)==m_Game->GetLocalPlayerIndex())
		m_VR->m_LastCarryUpdate = GetTickCount64();

	if (!wasTrue)
		m_VR->m_OverrideEyeAngles = false;

	return value;
}

// This function is apparently not used by Portal 2, remove?
void __fastcall Hooks::dRotateObject(void* ecx, void* edx, void* pPlayer, float fRotAboutUp, float fRotAboutRight, bool bUseWorldUpInsteadOfPlayerUp) {
	bool wasTrue = m_VR->m_OverrideEyeAngles;

	m_VR->m_OverrideEyeAngles = true;

	hkRotateObject.fOriginal(ecx, pPlayer, fRotAboutUp, fRotAboutRight, bUseWorldUpInsteadOfPlayerUp);

	if (!wasTrue)
		m_VR->m_OverrideEyeAngles = false;
}

// This is CPlayerBase, do we also need to hook CPortalPlayer? can the same function be used by both?
// This works for release, but why was it crashing before??? TODO: buy a c++ book...
QAngle& __fastcall Hooks::dEyeAngles(void* ecx, void* edx) {
	if (s_PickupQuery && s_PickupQuery->player == ecx) return s_PickupQuery->angles;
	if (m_VR && m_Game) {
		const int localIndex = m_Game->GetLocalPlayerIndex();
		const int index = ServerEntityIndex(ecx);
		if (index >= 0 && index < static_cast<int>(m_Game->m_PlayersVRInfo.size()))
		{
			auto &vrPlayer = m_Game->m_PlayersVRInfo[index];
			if (m_VR->m_OverrideEyeAngles
				&& m_VR->m_IsVREnabled && localIndex > 0 && localIndex == index
				&& m_VR->m_GrabPoseValid)
			{
				static bool logged = false;
				if (!logged)
				{
					const QAngle &controller = m_VR->m_GrabControllerAng;
					PortalVrLog("Controller EyeAngles override angle=%f,%f,%f",
						controller.x, controller.y, controller.z);
					logged = true;
				}
				Vector hand;
				if (ServerHandPose(ecx, hand, m_VR->m_ServerGrabAngles)) {
					static const uintptr_t directionCall = NativePose::CarryDirectionReturn(
						m_Game->m_Offsets->UpdateObject.address);
					if (s_CarryPlayer==ecx && directionCall
						&& reinterpret_cast<uintptr_t>(_ReturnAddress())==directionCall) {
						m_VR->m_ServerGrabAngles=PickupTrace::CarryDirectionAngles(m_VR->m_ServerGrabAngles);
						static bool loggedCarry=false;
						if (!loggedCarry) { PortalVrLog("Carry ray roll isolated; prop orientation retains wrist roll"); loggedCarry=true; }
					}
					return m_VR->m_ServerGrabAngles;
				}
				return hkEyeAngles.fOriginal(ecx);
			}
			if (m_VR->m_OverrideEyeAngles && vrPlayer.isUsingVR)
				return vrPlayer.controllerAngle;
		}
	}

	return hkEyeAngles.fOriginal(ecx);
}

int __fastcall Hooks::dGetDefaultFOV(void* ecx, void* edx) {
	return m_VR->m_Fov;
}

double __fastcall Hooks::dGetFOV(void* ecx, void* edx) {
	return m_VR->m_Fov;
}

float __fastcall Hooks::dGetViewModelFOV(void* ecx, void* edx) {
	return m_VR->m_Fov;
}
