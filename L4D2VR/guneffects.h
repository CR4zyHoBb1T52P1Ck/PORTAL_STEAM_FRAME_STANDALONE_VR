#pragma once
#include "sdk/vector.h"

namespace GunEffects {
inline bool Hidden(bool vr,bool owned,bool stowed,bool visible){return vr&&owned&&(stowed||!visible);}
// The native sprite loop branches on Color32 alpha BEFORE submitting a quad.
// Size alone is not its visibility gate. No material pointer is invalidated.
inline void Suppress(void* color,float* size){if(color)static_cast<unsigned char*>(color)[3]=0;if(size)*size=0;}
// GetEffectParameters calls GetAttachment, then FormatViewModelAttachment.
// A tracked attachment is already in the same world space as the rendered gun;
// retain that position across the native flat-screen FOV conversion.
class PositionScope {
    inline static thread_local PositionScope *active = nullptr;
    PositionScope *previous;
    Vector *output;
    Vector tracked;
    bool captured = false;
    bool owned = false;
public:
    PositionScope(Vector& position, bool enabled)
        : previous(active), output(enabled ? &position : nullptr) { active = this; }
    ~PositionScope() { active = previous; }
    PositionScope(const PositionScope&) = delete;
    PositionScope& operator=(const PositionScope&) = delete;

    static void Capture(const Vector& position) {
        if (active && active->output == &position) {
            active->tracked = position;
            active->captured = true;
        }
    }
    static void ObserveOwned(bool local){if(active&&local)active->owned=true;}
    bool Owned() const{return owned;}
    bool Restore(Vector *nativePosition = nullptr) const {
        if (!captured) return false;
        if (nativePosition) *nativePosition = *output;
        *output = tracked;
        return true;
    }
};
}
