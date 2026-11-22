#pragma once
#include "handcontact.h"
#include "portalpose.h"
// Bounded visual/target corrections only; no player hull or physics ownership.
namespace InteractionPolish {
inline matrix3x4_t Blend(const matrix3x4_t& a,const matrix3x4_t& b,float t){
    auto x=TwoHandAim::Axis(a,0)*(1-t)+TwoHandAim::Axis(b,0)*t;
    auto y=TwoHandAim::Axis(a,1)*(1-t)+TwoHandAim::Axis(b,1)*t;
    if(!TwoHandAim::Unit(x))return b;y-=x*TwoHandAim::Dot(y,x);if(!TwoHandAim::Unit(y))return b;
    return HandPose::Frame(x,y,CrossProduct(x,y),TwoHandAim::Position(a)*(1-t)+TwoHandAim::Position(b)*t);
}
struct Smooth {
    bool valid=false;double time=0;matrix3x4_t filtered{},previous{};Vector base{},velocity{};
    void Reset(){valid=false;}
    matrix3x4_t Step(const matrix3x4_t& input,Vector reference,double now,float units,float milliseconds){
        bool finite=TwoHandAim::Finite(reference)&&std::isfinite(now)&&std::isfinite(units)&&units>=1&&std::isfinite(milliseconds);
        for(int r=0;r<3;++r)for(int c=0;c<4;++c)finite=finite&&std::isfinite(input[r][c]);
        if(!finite){Reset();return input;}
        auto local=input;for(int r=0;r<3;++r)local[r][3]-=reference[r];
        const double dt=now-time;
        const float travel=valid?sqrtf((TwoHandAim::Position(local)-TwoHandAim::Position(previous)).LengthSqr()):0;
        const float dot=valid?TwoHandAim::Dot(TwoHandAim::Axis(previous,0),TwoHandAim::Axis(local,0)):1;
        if(!valid||milliseconds<=0||!std::isfinite(now)||!std::isfinite(units)||units<1||dt<0||dt>.12
            ||travel>units*.5f||(reference-base).LengthSqr()>units*units*.5f||dot<-.5f){
            filtered=previous=local;time=now;base=reference;velocity={0,0,0};valid=true;
        }else if(dt>=.002){
            const auto measured=(TwoHandAim::Position(local)-TwoHandAim::Position(previous))*(1/(units*float(dt)));
            velocity+=(measured-velocity)*(1-expf(-float(dt)/.06f));
            const float speed=sqrtf(velocity.LengthSqr());
            const float rotation=acosf(std::clamp(dot,-1.f,1.f))/float(dt);
            const float tau=std::clamp(milliseconds,0.f,60.f)*.001f/(1+speed*3+rotation*.1f);
            const float alpha=tau>0?1-expf(-float(dt)/tau):1;
            filtered=Blend(filtered,local,alpha);previous=local;time=now;base=reference;
        }
        auto result=filtered;for(int r=0;r<3;++r)result[r][3]+=reference[r];return result;
    }
};
// Conservative chest/abdomen proxy from the visible head, using yaw only.
inline matrix3x4_t Torso(Vector head,Vector forward,float units){
    forward.z=0;if(!TwoHandAim::Unit(forward))forward={1,0,0};const Vector up{0,0,1};
    return HandPose::Frame(forward,CrossProduct(up,forward),up,head-forward*(units*.10f)-up*(units*.43f));
}
inline matrix3x4_t OutsideTorso(const matrix3x4_t& raw,Vector head,Vector forward,float units,bool palm){
    if(!std::isfinite(units)||units<1||!TwoHandAim::Finite(head))return raw;
    const auto frame=Torso(head,forward,units);const auto center=TwoHandAim::Position(frame);
    const auto point=TwoHandAim::Position(raw)+TwoHandAim::Axis(raw,0)*(palm?0:units*.045f);
    const auto d=point-center;const Vector radii{units*.17f,units*.19f,units*.30f};Vector p;
    for(int i=0;i<3;++i)p[i]=TwoHandAim::Dot(d,TwoHandAim::Axis(frame,i));
    const float n=sqrtf(p.x*p.x/(radii.x*radii.x)+p.y*p.y/(radii.y*radii.y)+p.z*p.z/(radii.z*radii.z));
    if(!std::isfinite(n)||n>=1)return raw;
    if(n<.001f)p={radii.x,0,0};else p=p*(1/n);
    const auto safe=center+TwoHandAim::Axis(frame,0)*p.x+TwoHandAim::Axis(frame,1)*p.y+TwoHandAim::Axis(frame,2)*p.z;
    const auto correction=safe-point;if(correction.LengthSqr()>units*units*.30f*.30f)return raw;
    auto result=raw;for(int r=0;r<3;++r)result[r][3]+=correction[r];return result;
}
inline matrix3x4_t OutsideHand(const matrix3x4_t& raw,const matrix3x4_t& other,float units,bool left){
    auto delta=TwoHandAim::Position(raw)-TwoHandAim::Position(other);const float distance=sqrtf(delta.LengthSqr()),radius=units*.06f;
    if(!std::isfinite(distance)||!std::isfinite(radius)||radius<=0||distance>=radius)return raw;
    if(distance<.0001f)delta=TwoHandAim::Axis(other,1)*(left?-1.f:1.f);else delta=delta*(1/distance);
    auto result=raw;const auto p=TwoHandAim::Position(other)+delta*radius;for(int r=0;r<3;++r)result[r][3]=p[r];return result;
}
inline bool SceneDepth(const char* name,bool localBody){
    return localBody||(name&&(strstr(name,"weapons/v_portalgun")||strstr(name,"weapons/v_hands")));
}
}
