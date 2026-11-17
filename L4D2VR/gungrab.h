#pragma once
#include "twohandaim.h"
namespace GunGrab {
inline matrix3x4_t Apply(matrix3x4_t frame,const Vector& offsetCM,float scale) {
    const auto p=TwoHandAim::Position(HandPose::Concat(frame,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},offsetCM*(scale/100))));
    for(int r=0;r<3;++r)frame[r][3]=p[r];return frame;
}
inline bool Capture(const matrix3x4_t& gun,const Vector& holding,const Vector& touching,float scale,const Vector& previousCM,Vector& nextCM) {
    if(!TwoHandAim::Finite(holding)||!TwoHandAim::Finite(touching)||!std::isfinite(scale)||scale<1)return false;
    const auto delta=holding-touching;if(delta.LengthSqr()>scale*scale*.65f*.65f)return false;
    Vector local;for(int c=0;c<3;++c)local[c]=TwoHandAim::Dot(TwoHandAim::Axis(gun,c),delta)*100/scale;
    nextCM=previousCM+local;
    return TwoHandAim::Finite(nextCM)&&std::abs(nextCM.x)<=50&&std::abs(nextCM.y)<=50&&std::abs(nextCM.z)<=50;
}
inline matrix3x4_t WorldFit() {
    constexpr float s=1.1080807133325283f;
    return HandPose::Frame({0,-s,0},{-s,0,0},{0,0,-s},{-.3920553864f,1.3147089797f,3.1399856912f});
}
inline matrix3x4_t InPass(const matrix3x4_t& nativeRoot,const matrix3x4_t& passRoot,const matrix3x4_t& tracked) {
    return HandPose::Concat(HandPose::Concat(passRoot,HandPose::InverseRigid(nativeRoot)),tracked);
}
}
