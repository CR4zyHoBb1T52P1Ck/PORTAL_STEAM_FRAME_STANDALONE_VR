#pragma once
#include <cstdint>
namespace ModelInstance {
// Valve ModelInstanceHandle_t: zero owns a real cache slot. An extra draw
// without its own engine-managed instance must use MODEL_INSTANCE_INVALID.
// This skips per-instance dynamic-light history and avoids inheriting another
// entity/map's lighting/decal pointers. Native portal/world draws retain their
// original instance handles and normal cached lighting.
inline constexpr uint16_t Uncached=0xffff;
}
