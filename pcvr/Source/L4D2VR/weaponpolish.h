#pragma once
#include "twohandaim.h"
namespace WeaponPolish {
// Shortest rigid blend back to the authored grip. A handoff calibration is
// transitional; retaining it forever rotates/translates the gun off its grip.
inline matrix3x4_t Settle(const matrix3x4_t& calibration,float fraction) {
    if(!std::isfinite(fraction))fraction=1;
    fraction=std::clamp(fraction,0.f,1.f);
    if(fraction<=0)return calibration;
    const auto identity=HandPose::Frame({1,0,0},{0,1,0},{0,0,1},{0,0,0});
    if(fraction>=1)return identity;
    // Extract a quaternion using the largest diagonal for stability at 180deg.
    float q[4]{};const float trace=calibration[0][0]+calibration[1][1]+calibration[2][2];
    if(trace>0) {
        const float s=sqrtf(trace+1)*2;q[3]=s*.25f;
        q[0]=(calibration[2][1]-calibration[1][2])/s;q[1]=(calibration[0][2]-calibration[2][0])/s;q[2]=(calibration[1][0]-calibration[0][1])/s;
    } else {
        int i=0;if(calibration[1][1]>calibration[i][i])i=1;if(calibration[2][2]>calibration[i][i])i=2;
        const int j=(i+1)%3,k=(i+2)%3;const float s=sqrtf(std::max(1+calibration[i][i]-calibration[j][j]-calibration[k][k],1e-8f))*2;
        q[i]=s*.25f;q[j]=(calibration[i][j]+calibration[j][i])/s;q[k]=(calibration[i][k]+calibration[k][i])/s;q[3]=(calibration[k][j]-calibration[j][k])/s;
    }
    if(q[3]<0)for(auto& value:q)value=-value;
    const float t=fraction*fraction*(3-2*fraction),angle=acosf(std::clamp(q[3],0.f,1.f));
    Vector axis(q[0],q[1],q[2]);auto result=TwoHandAim::Unit(axis)?HandPose::RotateAroundAxis(axis,2*angle*(1-t)):identity;
    for(int r=0;r<3;++r)result[r][3]=calibration[r][3]*(1-t);
    return result;
}
struct Proximity {
    bool inside=false;unsigned long long lastPulse=0;
    bool Update(bool valid,float distance,float radius,unsigned long long now) {
        const bool previous=inside;
        inside=valid&&std::isfinite(distance)&&std::isfinite(radius)&&radius>0&&distance<=radius*(inside?1.2f:1.f);
        if(inside&&!previous&&(lastPulse==0||now-lastPulse>=500)){lastPulse=now;return true;}
        return false;
    }
};
}
