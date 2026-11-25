#pragma once
#include "handpose.h"
#include <algorithm>
namespace TwoHandAim {
inline Vector Position(const matrix3x4_t& m){return {m[0][3],m[1][3],m[2][3]};}
inline Vector Axis(const matrix3x4_t& m,int c){return {m[0][c],m[1][c],m[2][c]};}
inline float Dot(const Vector& a,const Vector& b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline bool Finite(const Vector& p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline bool Unit(Vector& p){const float length=sqrtf(p.LengthSqr());if(!std::isfinite(length)||length<1e-5f)return false;p*=1/length;return true;}
// Align the actual grip-to-socket vector, rather than assuming the support
// socket is on the barrel. This avoids a pitch jump on the first squeeze.
inline matrix3x4_t Solve(const matrix3x4_t& raw,const Vector& pivot,const Vector& offhand,
    const matrix3x4_t& socket,bool active,float influence,float minimumSpan) {
    if(!active||!Finite(pivot)||!Finite(offhand)||!std::isfinite(influence)||!std::isfinite(minimumSpan))return raw;
    influence=std::clamp(influence,0.f,1.f);if(influence==0)return raw;
    auto current=Position(HandPose::Concat(raw,socket))-pivot,desired=offhand-pivot;
    if(desired.LengthSqr()<minimumSpan*minimumSpan||!Unit(current)||!Unit(desired))return raw;
    const float cosine=std::clamp(Dot(current,desired),-1.f,1.f);
    Vector axis=CrossProduct(current,desired);
    if(!Unit(axis)) {
        if(cosine>0)return raw;
        axis=Axis(raw,1)-current*Dot(Axis(raw,1),current);
        if(!Unit(axis)){axis=Axis(raw,0)-current*Dot(Axis(raw,0),current);if(!Unit(axis))return raw;}
    }
    const auto rotation=HandPose::RotateAroundAxis(axis,acosf(cosine)*influence);
    auto relative=raw;for(int r=0;r<3;++r)relative[r][3]-=pivot[r];
    auto result=HandPose::Concat(rotation,relative);
    for(int r=0;r<3;++r)result[r][3]+=pivot[r];
    return result;
}
inline matrix3x4_t GripWrist(const matrix3x4_t& bindWrist,const matrix3x4_t& bindGun,const matrix3x4_t& gunTarget) {
    return HandPose::Reanchor(bindWrist,bindGun,gunTarget);
}
}
