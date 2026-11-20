#pragma once
#include "handpose.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace OptionalGunGrip {
// Optional hold-near acquisition is gated by the caller's neutral-input state.
// Pulling away, losing tracking, or opening a menu always releases support.
struct State {
    bool active = false;
    bool wasPressed = false;
    bool Update(bool enabled, bool tracked, bool fresh, bool pressed,
                float distance, float radius,bool holdNear=false) {
        const bool rising = pressed && !wasPressed;
        wasPressed = pressed;
        if (!enabled || !tracked || !fresh || !pressed || !std::isfinite(distance)
            || !std::isfinite(radius) || radius <= 0 || distance > radius * 2.5f)
            active = false;
        else if (!active && (rising||holdNear) && distance <= radius)
            active = true;
        return active;
    }
};

inline bool Squeeze(bool button, bool skeletonValid, const float *curl, bool previous) {
    if (button) return true;
    if (!skeletonValid) return false;
    float amount = 0;
    for (int i = 2; i < 5; ++i) {
        if (!std::isfinite(curl[i])) return false;
        amount += curl[i] / 3.0f;
    }
    return amount >= (previous ? 0.45f : 0.75f);
}

inline bool Finite(const matrix3x4_t& frame) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            if (!std::isfinite(frame[i][j])) return false;
    return true;
}

// Source v48 attachment records are 92 bytes. Accept only our named socket on
// the weapon root, and check every range before reading this optional metadata.
inline bool ReadSocket(const unsigned char *hdr, size_t size, matrix3x4_t& local,bool nativeModel=false) {
    if (!hdr || size < 248) return false;
    auto integer = [&](size_t offset) { int v; std::memcpy(&v, hdr + offset, 4); return v; };
    if ((integer(4)!=48&&!(nativeModel&&integer(4)==46)) || integer(156) != 45) return false;
    const int count = integer(240), offset = integer(244);
    if (count < 1 || count > 256 || offset < 248 || size_t(offset) > size
        || size_t(count) > (size - size_t(offset)) / 92) return false;
    static const char wanted[] = "lefthand_grip";
    for (int i = 0; i < count; ++i) {
        const size_t record = size_t(offset) + size_t(i) * 92;
        const int nameOffset = integer(record);
        if (nameOffset <= 0 || size_t(nameOffset) > size - record) continue;
        const size_t name = record + size_t(nameOffset);
        if (sizeof(wanted) > size - name || std::memcmp(hdr + name, wanted, sizeof(wanted))) continue;
        if (integer(record + 8) != 24) return false;
        std::memcpy(&local, hdr + record + 12, sizeof(local));
        if (!Finite(local)) return false;
        for (int axis = 0; axis < 3; ++axis) {
            float norm = 0;
            for (int r = 0; r < 3; ++r) norm += local[r][axis] * local[r][axis];
            if (std::fabs(norm - 1.0f) > 0.01f || std::fabs(local[axis][3]) > 64) return false;
        }
        // This compiled socket predates the enlarged rear shell. Its palm
        // floated 1.47 units below the surface. Seat that legacy socket against
        // the underside; leave newly authored/custom sockets unchanged.
        if (std::fabs(local[0][3]-2.8f)<.001f
            && std::fabs(local[1][3]+4.4f)<.001f
            && std::fabs(local[2][3]-15.f)<.001f) {
            local[0][3] -= .3f;
            local[1][3] += 1.2f;
            local[2][3] += .3f;
        }
        return true;
    }
    if(nativeModel&&sizeof("weapons/v_portalgun.mdl")<=size-12
        &&!std::memcmp(hdr+12,"weapons/v_portalgun.mdl",sizeof("weapons/v_portalgun.mdl"))) {
        local=HandPose::Frame({1,0,0},{0,1,0},{0,0,1},{0,-2.8f,12});return true;
    }
    return false;
}
}
