#pragma once
namespace TriggerColors {
inline int Physical(bool primary,bool swapped){return primary!=swapped?1:0;}
inline bool LegacyPrimary(int physical,bool leftHanded){return (physical==1)!=leftHanded;}
}
