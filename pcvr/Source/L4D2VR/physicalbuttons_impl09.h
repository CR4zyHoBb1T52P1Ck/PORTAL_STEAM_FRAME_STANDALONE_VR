#pragma once
#include "physicalbuttons09.h"
// Include from hooks.cpp after its existing anonymous-namespace helpers. Uses
// their qualified original native query and pose scope, never target Use calls.
namespace PhysicalButtonsRuntime09 {
static std::mutex mutex;
static ButtonPress09::Gate gate;
static ButtonPress09::Pulse pulse;
static uint64_t nextSample=0;
static bool queryConsumed=false;
struct SkipPair : CTraceFilter {
    void* other;
    SkipPair(void* player,void* skip):CTraceFilter(reinterpret_cast<IHandleEntity*>(player),0),other(skip){}
    bool ShouldHitEntity(IHandleEntity* entity,int) override{return entity!=m_pPassEnt&&entity!=other;}
};
inline bool Allowed() {
    auto* vr=Hooks::m_VR;auto* game=Hooks::m_Game;
    return vr&&game&&vr->m_Graphics.physicalButtons&&vr->InteractionAllowed()
        &&GetTickCount64()>=vr->m_ContactResetUntil&&GetTickCount64()-vr->m_ContactSnapshot<=120
        &&!vr->PortalContactClearance()&&!vr->m_PropHold.attached&&!vr->m_PropHold.use&&!vr->m_PropHold.release
        &&Hooks::hkFindUseEntity.isCreated()&&Hooks::hkEyePosition.isCreated()&&Hooks::hkEyeAngles.isCreated();
}
inline uint32_t Handle(void* entity) {
    const auto fn=SigScanner::GetVirtualFunction(entity,Portal1::VTableIndex::kServerEntity_GetRefEHandle);
    return fn?reinterpret_cast<const uint32_t&(__thiscall*)(void*)>(fn)(entity):0xffffffffu;
}
inline bool RedModel(void* entity) {
    auto* game=Hooks::m_Game;const int index=ServerEntityIndex(entity);
    if(!game||index<=0)return false;
    void* client=game->GetClientEntity(index);
    if(!client||!SigScanner::IsReadable(reinterpret_cast<uintptr_t>(client)+4,4))return false;
    auto* renderable=reinterpret_cast<unsigned char*>(client)+4;
    const auto fn=SigScanner::GetVirtualFunction(renderable,Portal1::VTableIndex::kClientRenderable_GetModel);
    auto* model=fn?reinterpret_cast<model_t*(__thiscall*)(void*)>(fn)(renderable):nullptr;
    auto* info=game->GetModelInfo();const char* name=model&&info?info->GetModelName(model):nullptr;
    return name&&ModelPath::Equal(name,"models/props/switch001.mdl");
}
inline bool RedDoor(void* entity) {
    static const uintptr_t table=SigScanner::FindRttiVtable("server.dll",".?AVCRotDoor@@");
    return table&&entity&&SigScanner::IsReadable(reinterpret_cast<uintptr_t>(entity),4)
        &&*reinterpret_cast<const uintptr_t*>(entity)==table;
}
inline bool Candidate(void* player,int source,ButtonPress09::Contact& out) {
    auto* vr=Hooks::m_VR;auto* game=Hooks::m_Game;
    if(!Allowed()||!player||source<0||source>=5||!vr->m_ContactValid[source]
        ||ServerEntityIndex(player)!=game->GetLocalPlayerIndex())return false;
    // Weapon/support palms must not press through their gun. Gun probes still
    // work while held; a stowed gun supplies no valid probes.
    if(source<2&&((!vr->m_Weapon.stowed&&source==vr->WeaponHand())
        ||(vr->m_OptionalSupportActive&&source!=vr->WeaponHand())||vr->m_PropHold.hand==source))return false;
    if(source>=2&&(vr->m_Weapon.stowed||!vr->WeaponTracked()))return false;
    auto* engine=game->GetServerEngineTrace();if(!engine)return false;
    Vector eye;const auto* actual=Hooks::hkEyePosition.fOriginal(player,&eye);if(!actual)return false;eye=*actual;
    const auto& eyeAngles=Hooks::hkEyeAngles.fOriginal(player);
    const auto hand=PortalPose::WorldHand(vr->m_ContactRelative[source],eye,eyeAngles);
    const auto end=PortalPose::Position(hand);auto direction=end-eye;
    if(!TwoHandAim::Finite(end)||direction.LengthSqr()>vr->m_VRScale*vr->m_VRScale*2.25f||!TwoHandAim::Unit(direction))return false;
    const float radius=vr->m_VRScale*.03f;
    Ray_t ray;ray.Init(eye,end+direction*radius);CGameTrace first{};first.fraction=1;
    CTraceFilterSkipEntity filter(reinterpret_cast<IHandleEntity*>(player),0);
    engine->TraceRay(ray,0x0601400b,&filter,&first);
    if(!first.m_pEnt||first.startsolid||first.allsolid||first.fraction>=1||!TwoHandAim::Finite(first.endpos)
        ||(first.endpos-end).LengthSqr()>radius*radius*1.78f)return false;
    const bool visual=RedModel(first.m_pEnt),door=RedDoor(first.m_pEnt);
    if(!visual&&!door)return false;
    // Retail Portal implements these red switches as a solid switch001 prop
    // paired with a tiny func_door_rotating brush. Require BOTH at this point,
    // not an arbitrary rotating door, floor switch or a remote usable entity.
    Ray_t pairRay;pairRay.Init(eye,end+direction*(radius*2));CGameTrace second{};second.fraction=1;
    SkipPair pairFilter(player,first.m_pEnt);engine->TraceRay(pairRay,0x0601400b,&pairFilter,&second);
    if(!second.m_pEnt||second.startsolid||second.allsolid||second.fraction>=1||!TwoHandAim::Finite(second.endpos)
        ||(second.endpos-first.endpos).LengthSqr()>radius*radius*4)return false;
    void* expected=door?first.m_pEnt:second.m_pEnt;
    if(!(door?RedModel(second.m_pEnt):RedDoor(second.m_pEnt)))return false;
    PickupQueryPose query{player};query.origin=first.endpos-direction*radius;
    QAngle::VectorAngles(direction,{0,0,1},query.angles);
    void* usable=nullptr;{ScopedPickupQuery scope(query);usable=Hooks::hkFindUseEntity.fOriginal(player);}
    if(usable!=expected||Handle(expected)==0xffffffffu)return false;
    out={expected,Handle(expected),source,true,true,true,true};return true;
}
inline void Reset() {
    std::lock_guard<std::mutex> lock(mutex);gate.Reset();pulse.Reset();nextSample=0;queryConsumed=false;
}
inline void Sample(void* player) {
    if(!Hooks::m_Game||ServerEntityIndex(player)!=Hooks::m_Game->GetLocalPlayerIndex())return;
    const auto now=GetTickCount64();const bool allowed=Allowed()
        &&!Hooks::m_VR->PressedDigitalAction(Hooks::m_VR->m_ActionUse);
    if(!allowed){Reset();return;}
    {std::lock_guard<std::mutex> lock(mutex);if(now<nextSample)return;nextSample=now+30;}
    ButtonPress09::Contact contacts[5];
    for(int i=0;i<5;++i)Candidate(player,i,contacts[i]);
    std::lock_guard<std::mutex> lock(mutex);
    const auto request=gate.Step(double(now)/1000,true,contacts,5);
    if(request)pulse.Queue(request,double(now)/1000);
}
inline bool Command(bool manualUse) {
    const auto now=double(GetTickCount64())/1000;
    std::lock_guard<std::mutex> lock(mutex);return pulse.Command(now,Allowed(),manualUse);
}
inline bool Query(void* player,void*& target) {
    ButtonPress09::Request request;bool hasActive=false;
    {std::lock_guard<std::mutex> lock(mutex);hasActive=bool(pulse.active);request=pulse.Query(double(GetTickCount64())/1000);if(hasActive)queryConsumed=true;}
    if(!hasActive)return false;target=nullptr;
    // An expired injected command must fail closed, never fall through to the
    // ordinary gun-ray selection where it could pick up an unrelated prop.
    if(!request)return true;
    ButtonPress09::Contact candidate;
    if(!Candidate(player,request.source,candidate)||candidate.entity!=request.entity||candidate.handle!=request.handle)return true;
    target=candidate.entity;
    Hooks::m_VR->WeaponHaptic(request.source<2?request.source:Hooks::m_VR->WeaponHand(),.45f,Hooks::m_VR->m_Graphics.contactHaptics);
    PortalVrLog("Physical red button native query verified entity=%p handle=0x%x source=%d",target,request.handle,request.source);
    return true;
}
inline void Completed(void* player) {
    if(!Hooks::m_Game||ServerEntityIndex(player)!=Hooks::m_Game->GetLocalPlayerIndex())return;
    std::lock_guard<std::mutex> lock(mutex);
    // Server ticks can precede arrival of the newly issued client command.
    // Retire only after this exact guarded native selection consumed it.
    if(queryConsumed){pulse.Completed();queryConsumed=false;}
}
}
