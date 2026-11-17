#pragma once
#include "handpose.h"
#include <cstdint>
#include <cstddef>
namespace ModelDrawState {
// Valve's VEngineModel016 DrawModelState_t (same interface as Portal).
// Static one-bone draws may use this matrix rather than custom bone zero.
struct State {
    void* header;void* hardware;void* renderable;const matrix3x4_t* model;
    uint16_t decals,padding;int flags,lod;
};
static_assert(sizeof(void*)!=4||(sizeof(State)==28&&offsetof(State,model)==12));
inline bool Copy(const void* source,State& result,const void* header,const void* renderable,const matrix3x4_t& model) {
    if(!source)return false;std::memcpy(&result,source,sizeof(result));
    if(result.header!=header||result.renderable!=renderable)return false;
    result.model=&model;return true;
}
}
