#pragma once
// No native state writes. Select only a verified main-eye viewmodel query;
// world/portal clears and desktop/creator rendering keep native behavior.
namespace ViewDepth {
struct Pass {bool primary=false,viewmodel=false;};
inline bool TrackedViewmodel(bool activeEye,bool callerMatches,bool parentIsPrimary,bool stackEmpty){
 return activeEye&&callerMatches&&(stackEmpty||parentIsPrimary);
}
inline bool Preserve(bool enabled,bool activeEye,bool vr,bool inGame,bool viewmodel,bool callerMatches){
 return enabled&&activeEye&&vr&&inGame&&viewmodel&&callerMatches;
}
}
