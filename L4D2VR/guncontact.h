#pragma once
#include "twohandaim.h"
namespace GunContact {
inline Vector Correction(Vector tip,Vector safe,Vector normal,float margin,float limit) {
    if(!TwoHandAim::Finite(tip)||!TwoHandAim::Finite(safe)||!TwoHandAim::Unit(normal)||!std::isfinite(margin)||!std::isfinite(limit))return {0,0,0};
    const float depth=TwoHandAim::Dot(safe-tip,normal)+std::clamp(margin,0.f,2.f);
    return normal*std::clamp(depth,0.f,std::max(0.f,limit));
}
}
