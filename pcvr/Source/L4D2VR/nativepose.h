#pragma once
#include "sigscanner.h"

namespace NativePose {
inline uintptr_t PortalGunEffectsDraw(uintptr_t client){
    // Retained owned-client evidence: void thiscall(weapon,bool worldModel),
    // sprite array at984, loop calls verified GetEffectParameters, RET4.
    const unsigned char entry[]={0x55,0x8b,0xec,0x83,0xec,0x1c,0x53,0x8b,0xd1,0x33,0xdb,0x8b,0x4d,0x08,0x56,0x57,0x89,0x55,0xfc,0x8d,0xba,0x84,0x09,0,0};
    const unsigned char tail[]={0x5f,0x5e,0x5d,0xc2,0x04,0};
    if(!client||!SigScanner::IsReadable(client+0x24b490,sizeof(entry))||!SigScanner::IsReadable(client+0x24b591,sizeof(tail))
        ||memcmp(reinterpret_cast<void*>(client+0x24b490),entry,sizeof(entry))||memcmp(reinterpret_cast<void*>(client+0x24b591),tail,sizeof(tail)))return 0;
    return client+0x24b490;
}
struct ViewmodelDepthBinding {uintptr_t target=0,caller=0;};
inline ViewmodelDepthBinding ViewmodelDepthClear(uintptr_t client) {
    // Owned retail DrawViewModels: query -> either ClearBuffers(false,true,false)
    // or DepthRange(0,.1). Only the query at this exact call site may be changed.
    const unsigned char entry[]={0xe8,0x5b,0xb8,0xe9,0xff,0x80,0xb8,0xa8,0x15,0,0,0,0x75,0x2d,0x8b,0x90,0x58,0x16,0,0,0x83,0xfa};
    const unsigned char call[]={0xe8,0x19,0x91,0x04,0,0x8b,0x75,0xcc,0x84,0xc0,0xd9,0xee,0x8b,0xce,0x0f,0x94,0xc0,0x88,0x45,0x0f,0x84,0xc0,0x8b,0x06,0x75,0x0d,0x6a,0,0x6a,1,0x6a,0,0xdd,0xd8,0xff,0x50,0x30};
    if(!client||!SigScanner::IsReadable(client+0x229d80,sizeof(entry))||!SigScanner::IsReadable(client+0x1e0c62,sizeof(call))
        ||memcmp(reinterpret_cast<void*>(client+0x229d80),entry,sizeof(entry))||memcmp(reinterpret_cast<void*>(client+0x1e0c62),call,sizeof(call)))return {};
    return {client+0x229d80,client+0x1e0c67};
}
inline uintptr_t PortalGunEffectParameters(uintptr_t client) {
    // Installed Portal: six stack arguments (index, color, size, material,
    // position, worldModel); the first-person branch formats position for FOV.
    const unsigned char entry[]={0x55,0x8b,0xec,0x83,0xec,0x10,0xa1};
    const unsigned char query[]={0x8b,0x46,0x04,0x8d,0x55,0xf0,0x8d,0x4e,0x04,
        0x8b,0x75,0x18,0x52,0x56,0xff,0x75,0x08,0xff,0x90,0x94,0,0,0,
        0x80,0x7d,0x1c,0,0x75,0x14,0x6a,0x01,0x56,0xe8,0xcb,0xc3,0xe7,0xff,
        0x83,0xc4,0x08,0x5b,0x5f,0x5e,0x8b,0xe5,0x5d,0xc2,0x18,0};
    return client && SigScanner::IsReadable(client+0x24b5b0,sizeof(entry))
        && SigScanner::IsReadable(client+0x24b690,sizeof(query))
        && !memcmp(reinterpret_cast<void*>(client+0x24b5b0),entry,sizeof(entry))
        && !memcmp(reinterpret_cast<void*>(client+0x24b690),query,sizeof(query))
        ? client+0x24b5b0 : 0;
}
inline uintptr_t PortalBlastCallback(uintptr_t client) {
    // Verify both callback registration and its color/timing/entity reads.
    const unsigned char entry[]={0x55,0x8b,0xec,0x51,0x53,0x56,0x57,0x8b,0x7d,0x08,
        0x68,0x40,0x05,0,0,0x80,0x7f,0x58,0x01,0xf3,0x0f,0x10,0x47,0x38,0x8b,0x5f,0x50};
    const unsigned char arguments[]={0xd9,0x45,0x08,0x8d,0x47,0x24,0x51,0xd9,0x1c,0x24,
        0x50,0x8d,0x47,0x0c,0x8b,0xce,0x50,0x57,0x53,0xff,0x75,0xfc,
        0xe8,0x9d,0xfd,0xff,0xff,0x5f,0x5e,0x5b,0x8b,0xe5,0x5d,0xc3};
    if(!client || !SigScanner::IsReadable(client+0x6ea00,10)
        || !SigScanner::IsReadable(client+0x22f830,sizeof(entry))
        || !SigScanner::IsReadable(client+0x22f888,sizeof(arguments))
        || !SigScanner::IsReadable(client+0x3fb71c,sizeof("PortalBlast"))) return 0;
    const auto* registration=reinterpret_cast<const unsigned char*>(client+0x6ea00);
    uintptr_t callback=0,name=0;
    memcpy(&callback,registration+1,4);memcpy(&name,registration+6,4);
    return registration[0]==0x68 && registration[5]==0x68
        && callback==client+0x22f830 && name==client+0x3fb71c
        && !memcmp(reinterpret_cast<void*>(name),"PortalBlast",sizeof("PortalBlast"))
        && !memcmp(reinterpret_cast<void*>(callback),entry,sizeof(entry))
        && !memcmp(reinterpret_cast<void*>(client+0x22f888),arguments,sizeof(arguments)) ? callback : 0;
}
inline uintptr_t PortalBlastDispatch(uintptr_t server) {
    // Verify the native FirePortal -> DispatchEffect("PortalBlast", data)
    // call and the CEffectData constructor's last field before copying 0x88.
    const unsigned char entry[]={0x55,0x8b,0xec,0x83,0xec,0x20,0x56,0x8d,0x4d,0xe0};
    const unsigned char constructorEnd[]={0x89,0x81,0x84,0,0,0,0x8b,0xc1,0xc3};
    if(!server || !SigScanner::IsReadable(server+0x46ae2d,0x13)
        || !SigScanner::IsReadable(server+0x47b610,sizeof(entry))
        || !SigScanner::IsReadable(server+0x453a9,sizeof(constructorEnd))
        || !SigScanner::IsReadable(server+0x614c4c,sizeof("PortalBlast"))) return 0;
    const auto* call=reinterpret_cast<const unsigned char*>(server+0x46ae2d);
    uintptr_t name=0;int relative=0;
    memcpy(&name,call+2,4);memcpy(&relative,call+15,4);
    return call[0]==0x50 && call[1]==0x68 && name==server+0x614c4c
        && call[14]==0xe8 && server+0x46ae40+relative==server+0x47b610
        && !memcmp(reinterpret_cast<void*>(server+0x614c4c),"PortalBlast",sizeof("PortalBlast"))
        && !memcmp(reinterpret_cast<void*>(server+0x47b610),entry,sizeof(entry))
        && !memcmp(reinterpret_cast<void*>(server+0x453a9),constructorEnd,sizeof(constructorEnd))
        ? server+0x47b610 : 0;
}
inline uintptr_t CarryDirectionReturn(uintptr_t update) {
    // Portal 1 UpdateObject's initial EyeAngles read drives its carry ray.
    // Later EyeAngles reads must keep roll for the object's orientation.
    const unsigned char call[]={0x8b,7,0x8b,0xcf,0x8b,0x80,0x0c,2,0,0,0xff,0xd0,0xd9,0xee};
    return update && SigScanner::IsReadable(update+0xee,sizeof(call))
        && !memcmp(reinterpret_cast<void*>(update+0xee),call,sizeof(call)) ? update+0xfa : 0;
}
inline uintptr_t ViewmodelProjectionReturn(uintptr_t client) {
    // DrawViewModels replaces the eye aspect with desktop GetScreenAspectRatio.
    // Match the installed instructions before identifying its Push3DView call.
    const unsigned char setup[]={0xf3,0x0f,0x10,0x47,0x60,0xf3,0x0f,0x11,0x85,0x4c,0xff,0xff,0xff,
        0xf3,0x0f,0x10,0x47,0x64,0xf3,0x0f,0x11,0x85,0x50,0xff,0xff,0xff,
        0xf3,0x0f,0x10,0x47,0x3c,0xf3,0x0f,0x11,0x85,0x2c,0xff,0xff,0xff,
        0x8b,1,0x8b,0x80,0x7c,1,0,0,0xff,0xd0,0x8b,0x77,0x1c,0x33,0xc0,
        0xc7,0x45,0x0c,0,0,0,0,0xd9,0x9d,0x60,0xff,0xff,0xff};
    const unsigned char push[]={0x8d,0x85,0xf4,0xfe,0xff,0xff,0x6a,0,0x50,0xff,0x96,0x94,0,0,0};
    return client && SigScanner::IsReadable(client+0x1e0bca,0x98)
        && !memcmp(reinterpret_cast<void*>(client+0x1e0bca),setup,sizeof(setup))
        && !memcmp(reinterpret_cast<void*>(client+0x1e0c53),push,sizeof(push)) ? client+0x1e0c62 : 0;
}
}
