#pragma once
#include <cstring>
#include <algorithm>
#include <cmath>
namespace RenderPolicy {
// Retail SetupMain3DView redirects HDR to a desktop-sized FullFrameFB.
inline bool UseEyeTarget(bool legacyEffects,const char* nativeTarget) {
    return !nativeTarget || (!legacyEffects&&std::strncmp(nativeTarget,"_rt_FullFrameFB",15)==0);
}
template<class View> inline void PrepareEye(View& view,bool legacyEffects) {
    view.m_bDoBloomAndToneMapping=legacyEffects;
}
inline unsigned Visible(unsigned dimension,float low,float high) {
    return unsigned(std::lround(dimension*std::clamp(high-low,0.f,1.f)));
}
}
