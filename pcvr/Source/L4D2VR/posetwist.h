#pragma once
#include "twohandaim.h"
namespace PoseTwist {
inline float Angle(const matrix3x4_t& rotation,Vector axis) {
    if(!TwoHandAim::Unit(axis))return 0;
    float q[4]{};const float trace=rotation[0][0]+rotation[1][1]+rotation[2][2];
    if(trace>0) {
        const float s=sqrtf(trace+1)*2;q[3]=s*.25f;
        q[0]=(rotation[2][1]-rotation[1][2])/s;q[1]=(rotation[0][2]-rotation[2][0])/s;q[2]=(rotation[1][0]-rotation[0][1])/s;
    } else {
        int i=0;if(rotation[1][1]>rotation[i][i])i=1;if(rotation[2][2]>rotation[i][i])i=2;
        const int j=(i+1)%3,k=(i+2)%3;const float s=sqrtf(std::max(1+rotation[i][i]-rotation[j][j]-rotation[k][k],1e-8f))*2;
        q[i]=s*.25f;q[j]=(rotation[i][j]+rotation[j][i])/s;q[k]=(rotation[i][k]+rotation[k][i])/s;q[3]=(rotation[k][j]-rotation[j][k])/s;
    }
    if(q[3]<0)for(auto& n:q)n=-n;
    const float along=q[0]*axis.x+q[1]*axis.y+q[2]*axis.z;
    return std::abs(along)+std::abs(q[3])<1e-6f?0:2*atan2f(along,q[3]);
}
inline void Distribute(matrix3x4_t& twist,const matrix3x4_t& wrist,const matrix3x4_t& desired,Vector axis,float fraction) {
    const auto delta=HandPose::Concat(desired,HandPose::InverseRigid(wrist));
    auto result=HandPose::Concat(HandPose::RotateAroundAxis(axis,Angle(delta,axis)*std::clamp(fraction,0.f,1.f)),twist);
    for(int r=0;r<3;++r)result[r][3]=twist[r][3];
    twist=result;
}
}
