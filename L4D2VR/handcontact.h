#pragma once
#include "twohandaim.h"
namespace HandContact {
struct State {bool valid=false,touching=false;Vector safe{0,0,0},normal{0,0,1};matrix3x4_t frame{};unsigned long long time=0,next=0;};
inline matrix3x4_t Surface(const matrix3x4_t& raw,Vector normal,Vector position,bool orient) {
    auto result=raw;
    if(orient&&TwoHandAim::Unit(normal)) {
        // Both Valve hand frames use -Y as the palm-facing direction.
        auto fingers=TwoHandAim::Axis(raw,0);fingers-=normal*TwoHandAim::Dot(fingers,normal);
        if(!TwoHandAim::Unit(fingers)){fingers=CrossProduct(normal,{0,0,1});if(!TwoHandAim::Unit(fingers))fingers={1,0,0};}
        result=HandPose::Frame(fingers,normal,CrossProduct(fingers,normal),position);
    }
    for(int r=0;r<3;++r)result[r][3]=position[r];return result;
}
inline bool Override(const float* curls,float gain,float threshold) {
    if(!curls)return false;
    // One pointing finger must not close/override the whole palm. A deliberate
    // squeeze of at least two non-thumb fingers wins over the contact pose.
    int closed=0;for(int i=1;i<5;++i)if(std::isfinite(curls[i])&&curls[i]*gain>=threshold)++closed;return closed>=2;
}
}
