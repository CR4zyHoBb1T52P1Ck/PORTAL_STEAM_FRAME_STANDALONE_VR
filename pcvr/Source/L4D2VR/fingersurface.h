#pragma once
#include "handpose.h"
#include "fingerrange.h"
#include "twohandaim.h"
#include "gunshape.h"
namespace FingerSurface {
struct Shape {
    matrix3x4_t frame{};Vector radii{1,1,1};bool gun=false;
    bool frontValid=false;matrix3x4_t frontFrame{};
    GunShape::Bounds Bounds()const{
        if(!frontValid)return GunShape::HousingBounds;
        const auto relative=HandPose::Concat(HandPose::InverseRigid(frame),frontFrame);GunShape::Bounds bounds{};
        for(int c=0;c<3;++c){bounds.low[c]=1e20f;bounds.high[c]=-1e20f;}
        for(size_t i=0;i<GunShape::Housing.size();++i){const auto& capsule=GunShape::Housing[i];
            for(int end=0;end<2;++end){Vector p=GunShape::Start(capsule)+(end?GunShape::Axis(capsule):Vector(0,0,0));
                if(i>=2)p=TwoHandAim::Position(HandPose::Concat(relative,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},p)));
                for(int c=0;c<3;++c){bounds.low[c]=std::min(bounds.low[c],p[c]-capsule.radius);bounds.high[c]=std::max(bounds.high[c],p[c]+capsule.radius);}}
        }for(int c=0;c<3;++c){bounds.low[c]-=.02f;bounds.high[c]+=.02f;}return bounds;
    }
    float Distance(Vector world) const {
        const auto delta=world-TwoHandAim::Position(frame);
        const Vector p{TwoHandAim::Dot(delta,TwoHandAim::Axis(frame,0)),TwoHandAim::Dot(delta,TwoHandAim::Axis(frame,1)),TwoHandAim::Dot(delta,TwoHandAim::Axis(frame,2))};
        if(gun){
            if(frontValid){const auto d=world-TwoHandAim::Position(frontFrame);
                const Vector q{TwoHandAim::Dot(d,TwoHandAim::Axis(frontFrame,0)),TwoHandAim::Dot(d,TwoHandAim::Axis(frontFrame,1)),TwoHandAim::Dot(d,TwoHandAim::Axis(frontFrame,2))};
                return std::min({GunShape::Distance(p,GunShape::Housing[0]),GunShape::Distance(p,GunShape::Housing[1]),GunShape::Distance(q,GunShape::Housing[2]),GunShape::Distance(q,GunShape::Housing[3])});}
            return GunShape::Distance(p);
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
