#pragma once
#include "sigscanner.h"
namespace SourceLoad {
// IVEngineClient::IsDrawingLoadingImage follows IsInGame/IsConnected at slot
// 28. Retail Portal's accessor is precisely MOV AL,[flag]; RET. Resolve only
// that read-only accessor, without calling an unknown virtual method.
struct Binding {
    const unsigned char* plaque=nullptr;
    static Binding Resolve(void* engine) {
        const auto fn=SigScanner::GetVirtualFunction(engine,28);
        if(!fn||!SigScanner::IsReadable(fn,6))return {};
        const auto* code=reinterpret_cast<const unsigned char*>(fn);
        HMODULE owner=nullptr;
        if(code[0]!=0xa0||code[5]!=0xc3
            ||!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCSTR>(fn),&owner)
            ||owner!=GetModuleHandleA("engine.dll"))return {};
        uintptr_t flag=0;std::memcpy(&flag,code+1,4);
        return SigScanner::IsReadable(flag,1)?Binding{reinterpret_cast<const unsigned char*>(flag)}:Binding{};
    }
    bool Active() const{return plaque&&*plaque!=0;}
};
}
