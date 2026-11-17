#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace FrameStats {
struct Window {
    std::vector<float> frames;
    double total=0,eye=0,wait=0;
    void Add(float frame,float eyeCpu,float gpuWait) {
        if(!std::isfinite(frame)||frame<=0)return;
        frames.push_back(frame);total+=frame;
        if(std::isfinite(eyeCpu)&&eyeCpu>=0)eye+=eyeCpu;
        if(std::isfinite(gpuWait)&&gpuWait>=0)wait+=gpuWait;
    }
    float Mean() const {return frames.empty()?0:float(total/frames.size());}
    float P95() const {if(frames.empty())return 0;auto sorted=frames;std::sort(sorted.begin(),sorted.end());return sorted[size_t((sorted.size()-1)*.95)];}
    void Clear(){frames.clear();total=eye=wait=0;}
};
}
