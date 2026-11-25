#pragma once
#include <algorithm>
#include <cmath>

namespace StereoProjection {
struct Frustum { float left, right, top, bottom; };
struct Bounds { float uMin, vMin, uMax, vMax; };
struct Plan {
    float tanHalfX=0, tanHalfY=0, horizontalFov=0, aspect=0;
    Bounds eyes[2]{};
};

// Source renders a symmetric camera. OpenVR samples each eye's asymmetric
// frustum from that camera. Raw OpenVR "bottom" is the +Y edge; its "top"
// is the -Y edge. See Valve's GetProjectionRaw documentation/SteamVR.cs.
inline bool Build(const Frustum (&raw)[2], Plan& out) {
    Plan result;
    for (const auto& eye:raw) {
        for (float value:{eye.left,eye.right,eye.top,eye.bottom})
            if (!std::isfinite(value)) return false;
        if (eye.left>=eye.right || eye.top>=eye.bottom) return false;
        result.tanHalfX=std::max({result.tanHalfX,-eye.left,eye.right});
        result.tanHalfY=std::max({result.tanHalfY,-eye.top,eye.bottom});
    }
    if (result.tanHalfX<=0 || result.tanHalfY<=0) return false;
    result.horizontalFov=2*std::atan(result.tanHalfX)*180.0f/3.14159265358979323846f;
    result.aspect=result.tanHalfX/result.tanHalfY;
    for (int index=0;index<2;++index) {
        const auto& eye=raw[index];auto& bounds=result.eyes[index];
        bounds.uMin=std::clamp(.5f+.5f*eye.left/result.tanHalfX,0.f,1.f);
        bounds.uMax=std::clamp(.5f+.5f*eye.right/result.tanHalfX,0.f,1.f);
        bounds.vMin=std::clamp(.5f-.5f*eye.bottom/result.tanHalfY,0.f,1.f);
        bounds.vMax=std::clamp(.5f-.5f*eye.top/result.tanHalfY,0.f,1.f);
        if (bounds.uMin>=bounds.uMax || bounds.vMin>=bounds.vMax) return false;
    }
    out=result;
    return true;
}
}
