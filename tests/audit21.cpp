#include <Windows.h>
#include <cassert>
#include <cstdio>
#include <cstddef>
#include "sdk/sdk.h"
#include "modelinstance.h"
#include "sigscanner.h"
#include "propholdinput.h"
static_assert(ModelInstance::Uncached==0xffff&&ModelInstance::Uncached!=0);
static_assert(sizeof(ModelRenderInfo_t)==68&&offsetof(ModelRenderInfo_t,instance)==0x40);
int main(int argc,char** argv) {
    assert(argc==2);auto engine=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(engine);
    const auto* base=reinterpret_cast<const unsigned char*>(engine);
    const unsigned char skip[]={0x66,0x8b,0x45,0x08,0xba,0xff,0xff,0,0,0x66,0x3b,0xc2,0x74,0xcd};
    assert(!memcmp(base+0x132fb9,skip,sizeof(skip)));
    assert(base[0x1332b2]==0xf3&&base[0x1332b3]==0xa5);
    // The branch on FFFF returns the current light state before dereferencing
    // the per-instance history that crashed. No engine function is executed.
    ModelRenderInfo_t info;assert(info.instance==ModelInstance::Uncached);FreeLibrary(engine);
    int cases=0;
    for(int owner=0;owner<2;++owner)for(int cancel=0;cancel<4;++cancel) {
        PropHold::Input input;bool grip[2]{},tracked[2]{true,true},eligible[2]{true,true};
        grip[owner]=true;input.Step(true,grip,tracked,eligible);input.Attach();
        grip[owner]=false;input.Step(true,grip,tracked,eligible);assert(input.throwRelease);
        if(cancel==0){tracked[owner]=false;input.Step(true,grip,tracked,eligible);tracked[owner]=true;}
        if(cancel==1)input.CancelThrow(); // portal crossing while release queued
        if(cancel==2)input.Suspend();
        if(cancel==3)input.Step(false,grip,tracked,eligible);
        for(int i=0;i<20;++i){input.Step(true,grip,tracked,eligible);assert(input.attached&&input.release&&!input.use&&!input.throwRelease);}
        input.Detached();grip[owner]=true;input.Step(true,grip,tracked,eligible);input.Attach();
        assert(input.hand==owner&&input.use);grip[owner]=false;input.Step(true,grip,tracked,eligible);assert(input.throwRelease);
        ++cases;
    }
    for(int owner=0;owner<2;++owner) {
        PropHold::Input input;bool grip[2]{},tracked[2]{true,true},eligible[2]{true,true};
        grip[owner]=true;input.Step(true,grip,tracked,eligible);input.Attach();input.CancelThrow();
        input.Step(true,grip,tracked,eligible);assert(input.hand==owner&&input.use&&!input.release);
        grip[owner]=false;input.Step(true,grip,tracked,eligible);assert(input.throwRelease);++cases;
    }
    uint32_t rng=0x2142;PropHold::Input input;
    for(int i=0;i<100000;++i) {
        rng=rng*1664525u+1013904223u;
        const bool grip[2]={bool(rng&1),bool(rng&2)},tracked[2]={bool(rng&4),bool(rng&8)},eligible[2]={bool(rng&16),bool(rng&32)};
        const int oldOwner=input.hand;const bool wasAttached=input.attached;
        input.Step(bool(rng&64),grip,tracked,eligible);
        if(oldOwner>=0&&wasAttached)assert(input.hand==oldOwner);
        assert(!input.throwRelease||(input.release&&input.attached&&!input.cancelled));
        assert(!input.use||(input.hand>=0&&tracked[input.hand]&&grip[input.hand]&&!input.release));
        if(rng&128)input.Attach();if(rng&256)input.CancelThrow();if(input.release&&(rng&512))input.Detached();
    }
    printf("Audit-21: retail invalid-instance early return/fault instruction and 68-byte render ABI verified; %d queued-release cancellation/recovery cases and100000 input transitions passed. CPU only.\n",cases);
}
