#pragma once
#include "portalpose.h"
#include "twohandaim.h"
#include <array>
namespace PropInteraction {
inline bool Valid(const matrix3x4_t& m) {for(int r=0;r<3;++r)for(int c=0;c<4;++c)if(!std::isfinite(m[r][c]))return false;return true;}
inline Vector Limit(Vector v,float speed) {const float n=sqrtf(v.LengthSqr());return n>speed&&n>0?v*(speed/n):v;}
inline Vector CenterVelocity(Vector linear,Vector angularDegrees,Vector hand,Vector center) {
    return linear+CrossProduct(angularDegrees*.01745329252f,center-hand);
}
// Fixed storage: no allocations, game-clock assumptions or worker threads.
struct Motion {
    struct Sample {double time=0;matrix3x4_t pose{};};
    std::array<Sample,16> samples{};int count=0;
    void Reset(){count=0;}
    void Push(double now,const matrix3x4_t& pose,float units) {
        if(!Valid(pose)||!std::isfinite(now)||units<1){Reset();return;}
        if(count) {
            const auto& last=samples[count-1];const double dt=now-last.time;
            if(dt==0)return;
            if(dt<0||dt>.15||(PortalPose::Position(pose)-PortalPose::Position(last.pose)).LengthSqr()>powf(units*float(dt)*20+units*.15f,2))Reset();
        }
        if(count==int(samples.size())){for(int i=1;i<count;++i)samples[i-1]=samples[i];--count;}
        samples[count++]={now,pose};
    }
    bool Velocity(double now,Vector& linear,Vector& angular) const {
        linear=angular={0,0,0};if(count<3||now<samples[count-1].time||now-samples[count-1].time>.12)return false;
        int first=count-1;while(first>0&&samples[count-1].time-samples[first-1].time<=.10)--first;
        if(count-first<3||samples[count-1].time-samples[first].time<.025)return false;
        double mean=0,denom=0;Vector pmean{0,0,0};
        for(int i=first;i<count;++i){mean+=samples[i].time-samples[first].time;pmean+=PortalPose::Position(samples[i].pose);}
        mean/=count-first;pmean=pmean*(1.f/(count-first));
        for(int i=first;i<count;++i){const double t=samples[i].time-samples[first].time-mean;denom+=t*t;linear+=(PortalPose::Position(samples[i].pose)-pmean)*float(t);}
        if(denom<1e-8)return false;linear=linear*float(1/denom);
        // Matrix log gives the shortest rotation, including roll; Euler wrap
        // cannot generate a spurious 360-degree spin. World-space axis here.
        const auto rotation=HandPose::Concat(samples[count-1].pose,HandPose::InverseRigid(samples[first].pose));
        const float cosine=std::clamp((rotation[0][0]+rotation[1][1]+rotation[2][2]-1)*.5f,-1.f,1.f);
        const float angle=acosf(cosine),dt=float(samples[count-1].time-samples[first].time);
        Vector axis{rotation[2][1]-rotation[1][2],rotation[0][2]-rotation[2][0],rotation[1][0]-rotation[0][1]};
        if(angle<3.10f&&TwoHandAim::Unit(axis))angular=axis*(angle*57.2957795f/dt);
        return TwoHandAim::Finite(linear)&&TwoHandAim::Finite(angular);
    }
};
struct Carry {
    void* controller=nullptr;void* entity=nullptr;int mode=-1,hand=-1;matrix3x4_t relative{};Motion motion;
    void Reset(){controller=entity=nullptr;mode=hand=-1;motion.Reset();}
    matrix3x4_t Target(void* c,void* e,int m,int h,double now,const matrix3x4_t& handPose,const matrix3x4_t& object,float units) {
        if(c!=controller||e!=entity||m!=mode||h!=hand){Reset();controller=c;entity=e;mode=m;hand=h;relative=HandPose::Concat(HandPose::InverseRigid(handPose),object);}
        motion.Push(now,handPose,units);return HandPose::Concat(handPose,relative);
    }
};
}
