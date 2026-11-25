#pragma once
#include "portaltrace.h"
namespace ActiveWeapon {
struct Binding {
    uintptr_t* entries=nullptr;
    static Binding Resolve(uintptr_t base) {
        const auto trace=PortalTrace::Binding::Resolve(base);
        if(!trace.entityList||!SigScanner::IsReadable(base+0xb0a32,27)||!SigScanner::IsReadable(base+0x39f59c,16))return {};
        // Verified DT_BaseCombatCharacter RecvProp constructor: 4-byte
        // m_hActiveWeapon at 0xde8. No game function is called to resolve it.
        const unsigned char offset[]={0x6a,4,0x68,0xe8,0x0d,0,0,0x68};
        if(std::memcmp(reinterpret_cast<void*>(base+0xb0a37),offset,sizeof(offset))||
            *reinterpret_cast<uintptr_t*>(base+0xb0a3f)!=base+0x39f59c||
            std::memcmp(reinterpret_cast<void*>(base+0x39f59c),"m_hActiveWeapon",16))return {};
        return {trace.entityList};
    }
    void* Handle(uint32_t handle) const {
        if(!entries||handle==0xffffffff||!SigScanner::IsReadable(reinterpret_cast<uintptr_t>(entries),4)||!SigScanner::IsReadable(*entries,16))return nullptr;
        const uintptr_t entry=*entries+(handle&0xfff)*16;
        if(!SigScanner::IsReadable(entry,12)||*reinterpret_cast<const uint32_t*>(entry+8)!=(handle>>12))return nullptr;
        const auto object=*reinterpret_cast<const uintptr_t*>(entry+4);
        return SigScanner::IsReadable(object,8)?reinterpret_cast<void*>(object):nullptr;
    }
    void* Local(void* player) const {
        if(!player||!SigScanner::IsReadable(reinterpret_cast<uintptr_t>(player)+0xde8,4))return nullptr;
        return Handle(*reinterpret_cast<const uint32_t*>(static_cast<const unsigned char*>(player)+0xde8));
    }
};
}
