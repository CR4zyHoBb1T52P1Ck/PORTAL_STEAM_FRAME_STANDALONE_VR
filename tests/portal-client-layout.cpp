#include <Windows.h>
#include <cassert>
#include "sdk/sdk.h"
#include "portaltrace.h"
#include "activeweapon.h"
// Optional installed-binary check: map code/relocations without running the
// game's DLL initializer or any game function outside its engine context.
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    HMODULE client = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!client) return 3;
    const auto binding = PortalTrace::Binding::Resolve(reinterpret_cast<uintptr_t>(client));
    const auto *base = reinterpret_cast<const unsigned char *>(client);
    const unsigned char matrixPrologue[]={0x55,0x8b,0xec,0x56,0x8b,0x75,8,0x57,0x8b,0xf9,0x83,0xfe,1,0x7c,0x38};
    const unsigned char anglesPrologue[]={0x55,0x8b,0xec,0x56,0x57,0x8b,0x7d,8,0x8b,0xf1,0x83,0xff,1,0x7c,0x6f};
    const unsigned char retMatrix[]={0xc2,8,0}, retAngles[]={0xc2,12,0};
    const bool attachments = binding.function
        && !memcmp(base+0xa5c50,matrixPrologue,sizeof(matrixPrologue))
        && !memcmp(base+0xa5d00,anglesPrologue,sizeof(anglesPrologue))
        && !memcmp(base+0xa5c94,retMatrix,sizeof(retMatrix))
        && !memcmp(base+0xa5d7b,retAngles,sizeof(retAngles));
    const bool matched = binding.function && binding.entityList && binding.traceFlags && attachments;
    auto playerTable=SigScanner::FindRttiVtable("client.dll",".?AVC_Portal_Player@@");
    const auto activeWeapon=playerTable?SigScanner::GetVirtualFunction(&playerTable,242):0;
    const unsigned char invalidAccessor[]={0x33,0xc0,0xc2,8,0};
    const bool rejectedOldAccessor=activeWeapon&&!std::memcmp(reinterpret_cast<const void*>(activeWeapon),invalidAccessor,sizeof(invalidAccessor));
    const auto weapon=ActiveWeapon::Binding::Resolve(reinterpret_cast<uintptr_t>(client));
    uint32_t table[4096*4]{},object[4]{};uintptr_t tableAddress=reinterpret_cast<uintptr_t>(table);
    ActiveWeapon::Binding fake{&tableAddress};const uint32_t handle=(23<<12)|17;
    table[17*4+1]=reinterpret_cast<uintptr_t>(object);table[17*4+2]=23;
    if(fake.Handle(handle)!=object||fake.Handle(handle+(1<<12))||fake.Handle(0xffffffff))return 6;
    printf("Verified active-weapon RecvProp 0xde8; rejected incompatible inherited slot 242; serial-checked entity handles\n");
    FreeLibrary(client);
    puts(matched ? "Installed Portal 1 trace and attachment ABI guards match" : "Installed client ABI mismatch");
    return matched&&weapon.entries&&rejectedOldAccessor ? 0 : 4;
}
