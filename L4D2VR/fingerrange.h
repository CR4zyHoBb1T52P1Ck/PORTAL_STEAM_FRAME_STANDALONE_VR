#pragma once
#include <algorithm>
#include <cmath>
namespace FingerRange {
inline float Amount(float value,float gain=1) {
    return std::isfinite(value)&&std::isfinite(gain)?std::clamp(value*gain,0.f,1.f):0.f;
}
inline float Angle(int finger,int segment,float amount) {
    // MCP/PIP/DIP close into the palm; the thumb retains its opposing axis.
    static const float fingers[3]={1.40f,1.60f,1.10f},thumb[3]={.65f,.95f,.70f};
    return Amount(amount)*(finger?fingers[segment]:thumb[segment]);
}
}
