#pragma once
#include "handpose.h"
#include "fingerrange.h"
#include "twohandaim.h"
namespace FingerSurface {
struct Shape {
    matrix3x4_t frame{};Vector radii{1,1,1};bool gun=false;
    float Distance(Vector world) const {
        const auto delta=world-TwoHandAim::Position(frame);
        const Vector p{TwoHandAim::Dot(delta,TwoHandAim::Axis(frame,0)),TwoHandAim::Dot(delta,TwoHandAim::Axis(frame,1)),TwoHandAim::Dot(delta,TwoHandAim::Axis(frame,2))};
        if(gun){
            // Conservative authored contact proxy for the housing/underside.
            // It has no moving claws, player collision or world force body.
            auto capsule=[&](Vector a,Vector b,float radius){const auto d=b-a;const float t=std::clamp(TwoHandAim::Dot(p-a,d)/d.LengthSqr(),0.f,1.f);return sqrtf((p-a-d*t).LengthSqr())-radius;};
            return std::min(capsule({.15f,2.7f,5.7f},{.15f,2.7f,16.5f},3.25f),capsule({.15f,.1f,2.8f},{.15f,.8f,5.8f},1.15f));
        }
        const Vector r{std::max(.1f,radii.x),std::max(.1f,radii.y),std::max(.1f,radii.z)};
        const float n=sqrtf(p.x*p.x/(r.x*r.x)+p.y*p.y/(r.y*r.y)+p.z*p.z/(r.z*r.z));
        return (n-1)*std::min(r.x,std::min(r.y,r.z));
    }
};
inline float Clamp(matrix3x4_t* bones,int wrist,const int chain[3],const matrix3x4_t local[3],const Vector axes[3],int finger,
                   float wanted,float extension,const Shape& shape,float margin) {
    if(!std::isfinite(wanted)||!std::isfinite(margin))return 0;
    wanted=std::clamp(wanted,0.f,1.f);margin=std::clamp(margin,0.f,1.5f);
    matrix3x4_t candidate[3],best[3];float bestAmount=0,bestScore=1e20f;bool openClear=false;
    auto pose=[&](float amount){int parent=wrist;for(int s=0;s<3;++s){candidate[s]=HandPose::Concat(s?candidate[s-1]:bones[parent],HandPose::RotateJoint(local[s],axes[s],FingerRange::Angle(finger,s,amount,extension)));parent=chain[s];}};
    auto penalty=[&](){float score=0;
        for(int s=0;s<3;++s){const auto start=TwoHandAim::Position(candidate[s]);
            const auto end=s<2?TwoHandAim::Position(candidate[s+1]):start+(start-TwoHandAim::Position(candidate[1]))*.65f;
            for(float t:{.15f,.5f,.85f,1.f}){const float depth=std::max(0.f,margin-shape.Distance(start+(end-start)*t));score+=depth*depth;}}
        return score;};
    // Search from the open pose; stop at first surface rather than letting a
    // fingertip pass through the shell and reappear on the far side.
    float clear=0,blocked=wanted;
    for(int n=0;n<=12;++n){const float amount=wanted*n/12;pose(amount);const auto score=penalty();
        if(n==0)openClear=score<.00000001f;
        if(score<bestScore-.00000001f||std::abs(score-bestScore)<.00000001f){bestScore=score;bestAmount=amount;for(int s=0;s<3;++s)best[s]=candidate[s];}
        if(openClear&&score>.00000001f){blocked=amount;break;}if(score<.00000001f)clear=amount;
    }
    if(openClear&&blocked>clear){for(int i=0;i<7;++i){const float mid=(clear+blocked)*.5f;pose(mid);if(penalty()>.00000001f)blocked=mid;else clear=mid;}pose(clear);bestAmount=clear;for(int s=0;s<3;++s)best[s]=candidate[s];}
    for(int s=0;s<3;++s)bones[chain[s]]=best[s];return bestAmount;
}
}
