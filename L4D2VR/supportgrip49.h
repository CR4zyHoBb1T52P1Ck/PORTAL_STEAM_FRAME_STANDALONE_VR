#pragma once
#include "supportgrip.h"
#include "optionalgungrip.h"
namespace SupportGrip49 {
inline bool Update(OptionalGunGrip::State& state,bool enabled,bool tracked,bool fresh,bool pressed,
                   float captureDistance,float captureRadius,float pullDistance,float releaseRadius,bool holdNear){
 // Making acquisition precise must not also shrink pull-away tolerance.
 return state.active?state.Update(enabled,tracked,fresh,pressed,pullDistance,releaseRadius/2.5f,holdNear):
                     state.Update(enabled,tracked,fresh,pressed,captureDistance,captureRadius,holdNear);
}
}
