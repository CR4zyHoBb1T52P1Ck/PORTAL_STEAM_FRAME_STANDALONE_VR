#pragma once
#include "handpose.h"
#include "fingerrange.h"
#include <algorithm>
namespace NativeFingers {
inline float Amount(float value) {return std::isfinite(value)?std::clamp(value,0.f,1.f):0.f;}
inline void Chain(const matrix3x4_t* bind,matrix3x4_t* result,const float* curl,int offset,int wrist,float gain=1,float extension=0) {
    static const int chains[5][3]={{21,22,23},{18,19,20},{15,16,17},{12,13,14},{9,10,11}};
    // Retail Valve wrists use Z towards the thumb and -Y into the palm.
    // The old custom-avatar routine uses Z for palm flexion and splays paw pads.
    const Vector palm(-bind[wrist][0][1],-bind[wrist][1][1],-bind[wrist][2][1]);
    const Vector middle(bind[15+offset][0][3],bind[15+offset][1][3],bind[15+offset][2][3]);
    for(int finger=0;finger<5;++finger) {
        int parent=wrist;
        for(int segment=0;segment<3;++segment) {
            const int bone=chains[finger][segment]+offset;
            const int from=segment<2?bone:parent,to=segment<2?bone+1:bone;
            const Vector direction(bind[to][0][3]-bind[from][0][3],bind[to][1][3]-bind[from][1][3],bind[to][2][3]-bind[from][2][3]);
            Vector toward=palm;
            if(finger==0) {
                toward=middle-Vector(bind[bone][0][3],bind[bone][1][3],bind[bone][2][3]);
                VectorNormalize(toward);toward+=palm*.5f;
            }
            const Vector axis=CrossProduct(direction,toward);
            const auto inverse=HandPose::InverseRigid(bind[parent]);
            const Vector localAxis(inverse[0][0]*axis.x+inverse[0][1]*axis.y+inverse[0][2]*axis.z,
                inverse[1][0]*axis.x+inverse[1][1]*axis.y+inverse[1][2]*axis.z,
                inverse[2][0]*axis.x+inverse[2][1]*axis.y+inverse[2][2]*axis.z);
            const auto local=HandPose::Concat(inverse,bind[bone]);
            
            result[bone]=HandPose::Concat(result[parent],HandPose::RotateJoint(local,localAxis,FingerRange::Angle(finger,segment,FingerRange::Amount(curl[finger],gain),extension)));
            parent=bone;
        }
    }
}
inline void Hands(const matrix3x4_t* bind,matrix3x4_t* result,const float* left,const float* right) {
    Chain(bind,result,left,0,8);Chain(bind,result,right,19,27);
}
inline void Gun(const matrix3x4_t* bind,matrix3x4_t* result,const float* curl,float gain=1,float floor=.5f,float extension=0) {
    static const float rest[5]={.35f,.1f,.5f,.5f,.5f};float grip[5];
    for(int i=0;i<5;++i)grip[i]=i?std::max(FingerRange::Amount(curl[i],gain),floor*(i==1?.25f:1.f)):FingerRange::Amount(curl[i],gain);
    Chain(bind,result,grip,0,8,1,extension);
}
inline void TrackedHand(const matrix3x4_t* bind,matrix3x4_t* result,const float* curl,bool left,bool grip,float gain=1,float floor=.5f,float extension=0) {
    static const float rest[5]={.35f,.1f,.5f,.5f,.5f};float posed[5];
    for(int i=0;i<5;++i)posed[i]=grip&&i!=0?std::max(FingerRange::Amount(curl[i],gain),floor*(i==1?.25f:1.f)):FingerRange::Amount(curl[i],gain);
    Chain(bind,result,posed,left?0:19,left?8:27,1,extension);
}
}
