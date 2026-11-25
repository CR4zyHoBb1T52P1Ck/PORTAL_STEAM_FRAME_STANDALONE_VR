#pragma once
#include "twohandaim.h"
namespace SkinTrial55 {
constexpr int Checksum=-674186433;
inline bool Motion(bool marked,int checksum,int mode,bool master){return marked&&checksum==Checksum&&mode==1&&master;}
inline Vector Limit(Vector v,float units,float millimeters){
 if(!TwoHandAim::Finite(v)||!std::isfinite(units)||units<1||!std::isfinite(millimeters))return {0,0,0};
 const float maximum=std::min(.1f,units*std::clamp(millimeters,0.f,2.f)/1000),n=sqrtf(v.LengthSqr());
 if(!std::isfinite(n))return {0,0,0};return n>maximum&&n>0?v*(maximum/n):v;
}
}
