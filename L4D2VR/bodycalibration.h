#pragma once
#include "twohandaim.h"
namespace BodyCalibration {
struct Measurements {float eyeCM=165,spanCM=150,widthCM=40,dropCM=23;};
inline bool Valid(const Measurements& m) {
    return std::isfinite(m.eyeCM)&&std::isfinite(m.spanCM)&&std::isfinite(m.widthCM)&&std::isfinite(m.dropCM)
        &&m.eyeCM>=90&&m.eyeCM<=220&&m.spanCM>=80&&m.spanCM<=240&&m.widthCM>=20&&m.widthCM<=65
        &&m.dropCM>=8&&m.dropCM<=50&&m.spanCM>m.widthCM+45;
}
inline bool Measure(const Vector& head,const Vector& left,const Vector& right,Vector forward,Measurements& result) {
    if(!TwoHandAim::Finite(head)||!TwoHandAim::Finite(left)||!TwoHandAim::Finite(right))return false;
    forward.z=0;if(!TwoHandAim::Unit(forward))return false;
    const auto side=CrossProduct(forward,{0,0,1});const auto span=right-left;
    const float width=std::abs(TwoHandAim::Dot(span,side));
    if(width<.8f||std::abs(span.z)>.15f||std::abs(TwoHandAim::Dot(span,forward))>.3f||
       TwoHandAim::Dot(left-head,side)>-.3f||TwoHandAim::Dot(right-head,side)<.3f||
       std::abs(TwoHandAim::Dot((left+right)*.5f-head,forward))>.3f)return false;
    Measurements measured{head.z*100,width*100,std::clamp(head.z*25,28.f,55.f),(head.z-(left.z+right.z)*.5f)*100};
    // Raised hands are not shoulder measurements. The previous permissive
    // 8cm minimum accepted a shrug/high T-pose and produced oversized reach.
    if(measured.dropCM<std::max(16.f,measured.eyeCM*.09f))return false;
    if(!Valid(measured))return false;result=measured;return true;
}
struct Capture {
    bool active=false,review=false,neutral=false,ready=false;float elapsed=0;
    Vector anchor[3]{},sum[3]{};unsigned samples=0;Measurements measured;
    void Start(){*this={};active=true;}
    void Cancel(){active=false;review=false;elapsed=0;samples=0;ready=false;}
    void ResetHold(){elapsed=0;samples=0;ready=false;}
    bool Step(float dt,const Vector& head,const Vector& left,const Vector& right,const Vector& forward,bool tracked,bool both,bool any) {
        if(!active||review)return false;
        if(!any)neutral=true;
        Measurements candidate;
        if(!neutral||!tracked||!both||!std::isfinite(dt)||dt<=0||dt>.1f||!Measure(head,left,right,forward,candidate)){ResetHold();return false;}
        const Vector now[]={head,left,right};
        bool moved=false;for(int i=0;i<3;++i)if(ready&&(now[i]-anchor[i]).LengthSqr()>.025f*.025f)moved=true;
        if(moved)ResetHold();
        if(!ready){for(int i=0;i<3;++i){anchor[i]=now[i];sum[i]={0,0,0};}ready=true;}
        for(int i=0;i<3;++i)sum[i]+=now[i];++samples;elapsed+=dt;
        if(elapsed<1.f)return false;
        if(!Measure(sum[0]/float(samples),sum[1]/float(samples),sum[2]/float(samples),forward,measured)){ResetHold();return false;}
        review=true;active=false;return true;
    }
};
}
