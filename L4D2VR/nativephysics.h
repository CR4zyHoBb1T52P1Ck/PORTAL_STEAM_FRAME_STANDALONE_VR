#pragma once
#include "sigscanner.h"
#include "portalpose.h"
#include "twohandaim.h"
#include "carryprofile.h"
namespace NativePhysics {
inline bool HeldAcrossPortal(void* serverPlayer,bool layoutVerified=false) {
    // Verified CGrabController::UpdateObject (server+468300): 12c1 is a
    // CPortal_Player field, NOT a field in the grab-controller allocation.
    const auto base=reinterpret_cast<uintptr_t>(serverPlayer);
    if(!layoutVerified||!base||base>UINTPTR_MAX-CarryProfile::PlayerAcrossOffset)return true;
    const auto at=base+CarryProfile::PlayerAcrossOffset;
    return !SigScanner::IsReadable(at,1)||*reinterpret_cast<const unsigned char*>(at)!=0;
}
struct Binding {
    uintptr_t setTarget=0,detach=0,getAttached=0;const uintptr_t* entries=nullptr;
    uintptr_t shutdownPickup=0,pickupTable=0;
    bool portalCarryLayout=false;
    static Binding Resolve(uintptr_t server) {
        Binding out;
        auto match=[&](size_t at,const unsigned char* bytes,size_t size){return server&&SigScanner::IsReadable(server+at,size)&&!memcmp(reinterpret_cast<void*>(server+at),bytes,size);};
        const unsigned char target[]={0x55,0x8b,0xec,0x8b,0x45,0x08,0x8b,0xd1,0xd9,0x00,0xd9,0x5a,0x04,0xd9,0x40,0x04,0xd9,0x5a,0x08};
        const unsigned char detach[]={0x55,0x8b,0xec,0xb8,0x08,0x10,0,0,0xe8,0x83,0x99,0x04,0,0x53,0x8b,0xd9,0x89,0x5d,0xf8,0x8b,0x93,0x50,0x20,0};
        const unsigned char attached[]={0x8b,0x91,0x50,0x20,0,0,0x83,0xfa,0xff,0x74,0x1d,0xa1};
        const unsigned char serial[]={0x8b,0xca,0x81,0xe1,0xff,0x0f,0,0,0xc1,0xea,0x0c,0x03,0xc9,0x39,0x54,0xc8,0x08,0x75,0x05,0x8b,0x44,0xc8,0x04,0xc3,0x33,0xc0,0xc3};
        const unsigned char ret[]={0xc2,0x08,0};
        if(!CarryProfile::Verify(match)||!match(0x466300,target,sizeof(target))||!match(0x466383,ret,sizeof(ret))
            ||!match(0x45fed0,detach,sizeof(detach))||!match(0x463060,attached,sizeof(attached))||!match(0x463070,serial,sizeof(serial)))return out;
        uintptr_t global=0;memcpy(&global,reinterpret_cast<void*>(server+0x46306c),4);
        if(global!=server+0x66c9fc||!SigScanner::IsReadable(global,4))return out;
        out={server+0x466300,server+0x45fed0,server+0x463060,reinterpret_cast<const uintptr_t*>(global)};
        out.portalCarryLayout=true;
        const unsigned char shutdown[]={0x55,0x8b,0xec,0x83,0xec,0x10,0x53,0x56,0x57,0x8b,0xf9,0x8b,0x87,0x88,0x23,0,0};
        const unsigned char detachCall[]={0x8d,0x8f,0x38,3,0,0,0xe8,0x35,0x99,0xff,0xff,0x8b,0x97,0xc8,0x23,0,0};
        if(match(0x466520,shutdown,sizeof(shutdown))&&match(0x466590,detachCall,sizeof(detachCall))) {
            out.pickupTable=SigScanner::FindRttiVtable("server.dll",".?AVCPlayerPickupController@@");
            if(out.pickupTable)out.shutdownPickup=server+0x466520;
        }
        return out;
    }
    void* PickupOwner(void* controller,void* player) const {
        if(!shutdownPickup||!pickupTable||!controller||!player)return nullptr;
        const uintptr_t parent=reinterpret_cast<uintptr_t>(controller)-0x338;
        if(!SigScanner::IsReadable(parent,4)||!SigScanner::IsReadable(parent+0x23c8,4))return nullptr;
        return *reinterpret_cast<const uintptr_t*>(parent)==pickupTable&&*reinterpret_cast<void* const*>(parent+0x23c8)==player?reinterpret_cast<void*>(parent):nullptr;
    }
    void* Entity(unsigned handle) const {
        if(!entries||handle==0xffffffff)return nullptr;
        const uintptr_t row=*entries+(handle&0xfff)*16;
        if(!*entries||!SigScanner::IsReadable(row,16))return nullptr;
        unsigned serial=0;uintptr_t entity=0;memcpy(&serial,reinterpret_cast<void*>(row+8),4);memcpy(&entity,reinterpret_cast<void*>(row+4),4);
        return serial==(handle>>12)&&SigScanner::IsReadable(entity,4)?reinterpret_cast<void*>(entity):nullptr;
    }
    void* Attached(void* controller) const {
        if(!getAttached||!SigScanner::IsReadable(reinterpret_cast<uintptr_t>(controller)+0x2050,4))return nullptr;
        return reinterpret_cast<void*(__thiscall*)(void*)>(getAttached)(controller);
    }
};
inline void* Physics(void* entity) {
    const auto at=reinterpret_cast<uintptr_t>(entity)+0x1e0;
    return entity&&SigScanner::IsReadable(at,4)?*reinterpret_cast<void**>(at):nullptr;
}
inline uintptr_t Method(void* object,int slot) {
    const auto function=SigScanner::GetVirtualFunction(object,slot);HMODULE owner=nullptr;
    return function&&SigScanner::IsExecutable(function)&&GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCSTR>(function),&owner)
        &&owner==GetModuleHandleA("vphysics.dll")?function:0;
}
inline bool Position(void* object,matrix3x4_t& pose) {
    // Native AttachObject calls +0xbc with position/angles output pointers.
    const auto target=Method(object,47);if(!target)return false;
    Vector p;QAngle a;reinterpret_cast<void(__thiscall*)(void*,Vector*,QAngle*)>(target)(object,&p,&a);
    pose=PortalPose::Frame(p,a);return TwoHandAim::Finite(p)&&std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);
}
inline bool Velocity(void* object,const Vector& linear,const Vector& localAngular) {
    // Native PhysForceClearVelocity calls +0xc4; native drop clamp uses +0xcc
    // GetVelocity and +0xd0 AddVelocity. Verified against installed server.dll.
    const auto target=Method(object,49);if(!target)return false;
    reinterpret_cast<void(__thiscall*)(void*,const Vector*,const Vector*)>(target)(object,&linear,&localAngular);return true;
}
inline bool ContactProfile() {
    // Match the owned retail VPhysics build before using additional public
    // IPhysicsObject methods. GetPosition47/SetVelocity49 are independently
    // verified against this Portal server; Source SDK defines the same table.
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleA("vphysics.dll"));
    if(!SigScanner::IsReadable(base,sizeof(IMAGE_DOS_HEADER)))return false;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>4096||!SigScanner::IsReadable(base+dos->e_lfanew,sizeof(IMAGE_NT_HEADERS)))return false;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    return nt->Signature==IMAGE_NT_SIGNATURE&&nt->FileHeader.TimeDateStamp==0x674532f1&&nt->OptionalHeader.SizeOfImage==0xeb000;
}
inline bool Push(void* object,const Vector& impulse,const Vector& position) {
    if(!TwoHandAim::Finite(impulse)||!TwoHandAim::Finite(position)||impulse.LengthSqr()<.0001f)return false;
    static const bool profile=ContactProfile();if(!profile)return false;
    // Public IPhysicsObject: IsMoveable10, Wake24, ApplyForceOffset60. An offset
    // impulse lets native mass/inertia determine tipping, rather than replacing
    // velocities or moving an object/player directly.
    const auto movable=Method(object,10),wake=Method(object,24),force=Method(object,60);
    if(!movable||!wake||!force||!reinterpret_cast<bool(__thiscall*)(void*)>(movable)(object))return false;
    reinterpret_cast<void(__thiscall*)(void*)>(wake)(object);
    reinterpret_cast<void(__thiscall*)(void*,const Vector&,const Vector&)>(force)(object,impulse,position);return true;
}
}
