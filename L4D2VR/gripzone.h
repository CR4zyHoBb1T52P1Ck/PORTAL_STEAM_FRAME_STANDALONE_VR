#pragma once
#include "twohandaim.h"
namespace GripZone {
struct Shape {float length=19,radius=7;Vector center={0,-3.7f,12};};
struct Hit {bool valid=false;float distance=0;Vector point;};
inline Hit Closest(const matrix3x4_t& gun,const Vector& hand,const Shape& shape) {
    Hit hit;for(int r=0;r<3;++r)for(int c=0;c<4;++c)if(!std::isfinite(gun[r][c]))return hit;
    if(!TwoHandAim::Finite(hand)||!TwoHandAim::Finite(shape.center)||!std::isfinite(shape.length)||!std::isfinite(shape.radius)||shape.length<0||shape.radius<=0)return hit;
    const auto inverse=HandPose::InverseRigid(gun);
    const auto local=TwoHandAim::Position(HandPose::Concat(inverse,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},hand)));
    hit.point=shape.center;hit.point.z=std::clamp(local.z,shape.center.z-shape.length*.5f,shape.center.z+shape.length*.5f);
    hit.distance=sqrtf((local-hit.point).LengthSqr());hit.valid=std::isfinite(hit.distance);return hit;
}
inline matrix3x4_t Point(const Vector& p){return HandPose::Frame({1,0,0},{0,1,0},{0,0,1},p);}
}
