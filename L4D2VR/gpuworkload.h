#pragma once
#include <d3d9.h>
#include <cstdint>
#include <cmath>
// Sparse, bounded timestamp queries. Never flush, spin, or wait for the GPU.
namespace GpuWorkload {
constexpr int Markers=10,Slots=3;
struct Metadata {uint64_t serial=0;unsigned views=0,width=0,height=0,outputWidth=0,outputHeight=0;int upscaler=0,mirror=0,phase=0,msaa=0;bool upscaleReady=false;};
struct Result {Metadata meta;double left=0,right=0,mirror=0,copy=0,upscale=0,resolve=0;};
inline bool Convert(const uint64_t (&ticks)[Markers],uint64_t frequency,bool disjoint,Result& out){
    if(disjoint||!frequency)return false;
    for(int i=1;i<Markers;++i)if(ticks[i]<ticks[i-1])return false;
    const auto ms=[&](int a,int b){return double(ticks[b]-ticks[a])*1000./frequency;};
    out.left=ms(0,1);out.right=ms(1,2);out.mirror=ms(2,3);out.copy=ms(4,5);out.upscale=ms(6,7);out.resolve=ms(8,9);
    return std::isfinite(out.left+out.right+out.mirror+out.copy+out.upscale+out.resolve);
}
class Ring {
    struct Slot {
        IDirect3DQuery9* query[Markers+2]{};Metadata meta;bool pending=false;unsigned issued=0;
        ~Slot(){for(auto* q:query)if(q)q->Release();}
    } slots[Slots];
    int active=-1;unsigned next=0;uint64_t serial=0;bool failed=false;
    void Abort(){if(active>=0){slots[active].query[Markers]->Issue(D3DISSUE_END);active=-1;}}
public:
    unsigned dropped=0,rejected=0;
    Ring()=default;Ring(const Ring&)=delete;Ring& operator=(const Ring&)=delete;
    bool Failed()const{return failed;}
    bool Active()const{return active>=0;}
    template<class Factory> void Begin(bool enabled,Metadata meta,Factory create,unsigned cadence=30){
        Abort();++serial;
        if(!enabled||failed||!cadence||serial%cadence)return;
        int index=-1;
        for(int n=0;n<Slots;++n){const int i=(next+n)%Slots;if(!slots[i].pending){index=i;break;}}
        if(index<0){++dropped;return;}
        auto& slot=slots[index];
        for(int n=0;n<Markers+2;++n)if(!slot.query[n]){
            const auto type=n<Markers?D3DQUERYTYPE_TIMESTAMP:n==Markers?D3DQUERYTYPE_TIMESTAMPDISJOINT:D3DQUERYTYPE_TIMESTAMPFREQ;
            if(FAILED(create(type,&slot.query[n]))||!slot.query[n]){failed=true;return;}
        }
        slot.meta=meta;slot.meta.serial=serial;slot.issued=0;active=index;next=(index+1)%Slots;
        if(FAILED(slot.query[Markers]->Issue(D3DISSUE_BEGIN))){failed=true;active=-1;return;}
        Mark(0);
    }
    void Mark(unsigned marker){
        if(active<0||marker>=Markers)return;
        auto& slot=slots[active];
        if(slot.issued!=(1u<<marker)-1){++rejected;Abort();return;}
        if(FAILED(slot.query[marker]->Issue(D3DISSUE_END))){failed=true;Abort();return;}
        slot.issued|=1u<<marker;
        if(marker==Markers-1){
            const auto frequency=slot.query[Markers+1]->Issue(D3DISSUE_END);
            const auto disjoint=slot.query[Markers]->Issue(D3DISSUE_END);
            slot.pending=SUCCEEDED(frequency)&&SUCCEEDED(disjoint);if(!slot.pending)failed=true;active=-1;
        }
    }
    void Update(Metadata meta){if(active>=0){meta.serial=slots[active].meta.serial;slots[active].meta=meta;}}
    template<class Consume> void Poll(Consume consume){
        for(auto& slot:slots){
            if(!slot.pending)continue;
            BOOL disjoint=TRUE;uint64_t frequency=0,ticks[Markers]{};
            auto read=[](IDirect3DQuery9* q,void* data,DWORD size){return q->GetData(data,size,0);};
            HRESULT hr=read(slot.query[Markers],&disjoint,sizeof(disjoint));
            if(hr==S_FALSE)continue;
            if(FAILED(hr)){slot.pending=false;failed=true;continue;}
            hr=read(slot.query[Markers+1],&frequency,sizeof(frequency));
            if(hr==S_FALSE)continue;
            if(FAILED(hr)){slot.pending=false;failed=true;continue;}
            bool ready=true;
            for(int i=0;i<Markers;++i){hr=read(slot.query[i],&ticks[i],sizeof(ticks[i]));if(hr!=S_OK){ready=false;if(FAILED(hr)){slot.pending=false;failed=true;}break;}}
            if(!ready)continue;
            slot.pending=false;Result result;result.meta=slot.meta;
            if(Convert(ticks,frequency,disjoint!=FALSE,result))consume(result);else ++rejected;
        }
    }
};
}
