#include <Windows.h>
#include <cassert>
#include <cstdio>
#include "propholdinput.h"
#include "nativephysics.h"
#include "modeldrawstate.h"
#include "sourceload.h"
int main(int argc,char** argv) {
    assert(argc==3);
    auto engine=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(engine);
    auto table=SigScanner::FindRttiVtable("engine.dll",".?AVCEngineClient@@");assert(table);
    const auto loading=SourceLoad::Binding::Resolve(&table);assert(loading.plaque);
    unsigned char plaque=0;SourceLoad::Binding fakeLoading{&plaque};assert(!fakeLoading.Active());plaque=1;assert(fakeLoading.Active());
    assert(!SourceLoad::Binding::Resolve(nullptr).plaque);
    uintptr_t invalidTable[29]{};invalidTable[28]=SigScanner::GetVirtualFunction(&table,26);
    uintptr_t invalidEngine=reinterpret_cast<uintptr_t>(invalidTable);
    assert(!SourceLoad::Binding::Resolve(&invalidEngine).plaque);
    for(int slot:{26,27,28}) {
        const auto target=SigScanner::GetVirtualFunction(&table,slot);assert(target);
        printf("Engine slot %d RVA %x bytes ",slot,unsigned(target-reinterpret_cast<uintptr_t>(engine)));
        for(int j=0;j<32;++j)printf("%02x ",reinterpret_cast<const unsigned char*>(target)[j]);puts("");
    }
    FreeLibrary(engine);
    auto server=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(server);
    const auto binding=NativePhysics::Binding::Resolve(reinterpret_cast<uintptr_t>(server));
    assert(binding.shutdownPickup&&binding.pickupTable&&binding.getAttached);
    alignas(4) unsigned char parent[0x23cc]{};int player=0,other=0;
    *reinterpret_cast<uintptr_t*>(parent)=binding.pickupTable;
    *reinterpret_cast<void**>(parent+0x23c8)=&player;
    assert(binding.PickupOwner(parent+0x338,&player)==parent);
    assert(!binding.PickupOwner(parent+0x338,&other)&&!binding.PickupOwner(nullptr,&player));
    *reinterpret_cast<uintptr_t*>(parent)=0;assert(!binding.PickupOwner(parent+0x338,&player));
    FreeLibrary(server);
    unsigned cases=0;
    for(int owner=0;owner<2;++owner)for(int frames:{1,2,30,600}) {
        PropHold::Input input;bool grip[2]{},tracked[2]{true,true},eligible[2]{true,true};
        input.Step(true,grip,tracked,eligible);grip[owner]=true;input.Step(true,grip,tracked,eligible);
        assert(input.hand==owner&&input.use&&!input.release);input.Attach();
        for(int i=0;i<frames;++i){input.Step(true,grip,tracked,eligible);assert(input.use&&input.attached&&!input.release);}
        // The other hand cannot steal or drop the held object.
        grip[1-owner]=true;input.Step(true,grip,tracked,eligible);assert(input.hand==owner&&input.use);
        grip[owner]=false;input.Step(true,grip,tracked,eligible);
        assert(input.release&&input.throwRelease&&!input.use);input.Detached();
        input.Step(true,grip,tracked,eligible);assert(input.hand<0); // no automatic transfer
        grip[1-owner]=false;input.Step(true,grip,tracked,eligible);
        grip[1-owner]=true;input.Step(true,grip,tracked,eligible);assert(input.hand==1-owner);
        ++cases;
    }
    for(int owner=0;owner<2;++owner)for(int cancel=0;cancel<3;++cancel) {
        PropHold::Input input;bool grip[2]{},tracked[2]{true,true},eligible[2]{true,true};
        grip[owner]=true;input.Step(true,grip,tracked,eligible);input.Attach();
        if(cancel==0)input.Suspend(); // pause/menu
        if(cancel==1)input.Step(false,grip,tracked,eligible); // focus/mode loss
        if(cancel==2){tracked[owner]=false;input.Step(true,grip,tracked,eligible);}
        tracked[owner]=true;grip[owner]=false;
        for(int i=0;i<10;++i){input.Step(true,grip,tracked,eligible);assert(input.release&&!input.throwRelease&&!input.use);}
        input.Detached();++cases;
    }
    // Failed selection remains a normal use button; no later release impulse.
    {PropHold::Input input;bool grip[2]{true,false},valid[2]{true,true};input.Step(true,grip,valid,valid);
     grip[0]=false;input.Step(true,grip,valid,valid);assert(input.hand<0&&!input.release&&!input.throwRelease);++cases;}
    // Both ModelRenderInfo and DrawModelState must reference the same temporary
    // portal-space model matrix. The engine's original state remains untouched.
    matrix3x4_t original=PortalPose::Frame({1,2,3},{0,20,0});int header=0,renderable=0;
    ModelDrawState::State source{&header,nullptr,&renderable,&original,12,0,77,2},copy;
    for(int i=0;i<600;++i) {
        const auto mapped=HandPose::Concat(PortalPose::Frame({float(i),-20,80},{0,float(i),0}),original);
        assert(ModelDrawState::Copy(&source,copy,&header,&renderable,mapped));
        assert(copy.model==&mapped&&source.model==&original&&copy.flags==77&&copy.lod==2&&copy.decals==12);
        assert(!ModelDrawState::Copy(&source,copy,&player,&renderable,mapped));
    }
    printf("Interaction-20: %u grip ownership/cancellation cases, guarded retail pickup shutdown ABI and 600 paired portal draw-state matrices passed. CPU only.\n",cases);
}
