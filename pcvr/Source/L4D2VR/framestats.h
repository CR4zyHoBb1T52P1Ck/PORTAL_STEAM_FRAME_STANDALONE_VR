#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace FrameStats {
struct ViewGroup {
    unsigned samples=0,over72=0,over90=0;
    double frame=0,eye=0,wait=0,mirror=0,body=0;
    void Add(float interval,float eyeCpu,float idle,float mirrorCpu,float bodyCpu) {
        ++samples;frame+=interval;over72+=interval>1000.f/72;over90+=interval>1000.f/90;
        if(std::isfinite(eyeCpu)&&eyeCpu>=0)eye+=eyeCpu;
        if(std::isfinite(idle)&&idle>=0)wait+=idle;
        if(std::isfinite(mirrorCpu)&&mirrorCpu>=0)mirror+=mirrorCpu;
        if(std::isfinite(bodyCpu)&&bodyCpu>=0)body+=bodyCpu;
    }
};
struct Window {
    std::vector<float> frames;
    double total=0,eye=0,wait=0,body=0,secondary=0,rebases=0,cacheHits=0,mirror=0;unsigned maxViewDepth=0;
    ViewGroup groups[3]; // 0-2, 3-4, 5+ secondary views; not portal-only identities.
    Window(){frames.reserve(256);}
    void Add(float frame,float eyeCpu,float gpuWait,unsigned views=0,unsigned depth=0,unsigned transforms=0,unsigned hits=0,float bodyCpu=0,float mirrorCpu=0) {
        if(!std::isfinite(frame)||frame<=0)return;
        frames.push_back(frame);total+=frame;secondary+=views;maxViewDepth=std::max(maxViewDepth,depth);rebases+=transforms;cacheHits+=hits;if(std::isfinite(bodyCpu)&&bodyCpu>=0)body+=bodyCpu;
        if(std::isfinite(eyeCpu)&&eyeCpu>=0)eye+=eyeCpu;
        if(std::isfinite(gpuWait)&&gpuWait>=0)wait+=gpuWait;
        if(std::isfinite(mirrorCpu)&&mirrorCpu>=0)mirror+=mirrorCpu;
        groups[views<=2?0:views<=4?1:2].Add(frame,eyeCpu,gpuWait,mirrorCpu,bodyCpu);
    }
    float Mean() const {return frames.empty()?0:float(total/frames.size());}
    float P95() const {if(frames.empty())return 0;auto selected=frames;const auto index=size_t((selected.size()-1)*.95);std::nth_element(selected.begin(),selected.begin()+index,selected.end());return selected[index];}
    void Clear(){frames.clear();total=eye=wait=body=secondary=rebases=cacheHits=mirror=0;maxViewDepth=0;for(auto& group:groups)group={};}
};
}
