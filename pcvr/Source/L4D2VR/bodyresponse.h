#pragma once
#include "bodysecondary.h"
#include "propinteraction.h"
namespace BodyResponse {
struct Oscillator {
    float offset=0,velocity=0;
    void Step(float dt,float acceleration,float hz,float damping) {
        const float omega=6.2831853f*std::clamp(hz,2.f,10.f);
        const int steps=std::max(1,int(std::ceil(dt/.003f)));const float tick=dt/steps;
        for(int i=0;i<steps;++i){velocity+=(-acceleration-offset*omega*omega-velocity*(2*std::clamp(damping,.2f,2.f)*omega))*tick;offset+=velocity*tick;}
        offset=std::clamp(offset,-.10f,.10f);velocity=std::clamp(velocity,-2.f,2.f);
    }
};
struct Output {float chest=0,hips=0;Vector breast{0,0,0},glute{0,0,0};unsigned touches=0;};
inline Vector Local(Vector delta,Vector forward,Vector right,float units) {
    return {TwoHandAim::Dot(delta,forward)/units,TwoHandAim::Dot(delta,right)/units,delta.z/units};
}
inline bool Crosses(Vector a,Vector b,Vector center,Vector radii) {
    // Swept ellipsoid, not endpoint-only: fast hands cannot skip the volume.
    for(int i=0;i<3;++i){a[i]=(a[i]-center[i])/radii[i];b[i]=(b[i]-center[i])/radii[i];}
    const auto d=b-a;const float n=d.LengthSqr();
    const float t=n>.000001f?std::clamp(-TwoHandAim::Dot(a,d)/n,0.f,1.f):0;
    return a.LengthSqr()>1&&(a+d*t).LengthSqr()<=1;
}
struct State {
    double stamp=0,contactAfter[2][2]{};
    Vector previousHead{0,0,0},headVelocity{0,0,0},previousHand[2]={{0,0,0},{0,0,0}},handVelocity[2]={{0,0,0},{0,0,0}};
    bool handValid[2]{};Oscillator chest,hips,breastSide,breastFront,gluteSide,gluteFront;
    void Reset(){*this={};}
    Output Step(Vector head,const Vector hands[2],const bool tracked[2],Vector chestCenter,Vector hipCenter,
                Vector forward,double now,float units,float hz,float damping,float strength,float jumpGain,
                float swingGain,float touchGain,float maxResponse,float chestScale=1,float hipScale=1) {
        Output out;const double elapsed=now-stamp;
        if(!TwoHandAim::Finite(head)||!TwoHandAim::Finite(chestCenter)||!TwoHandAim::Finite(hipCenter)||!std::isfinite(now)||units<1||!std::isfinite(units)) {Reset();return out;}
        forward.z=0;if(!TwoHandAim::Unit(forward))forward={1,0,0};const auto right=CrossProduct(forward,{0,0,1});
        Vector local[2];for(int h=0;h<2;++h)local[h]=Local(hands[h]-head,forward,right,units);
        if(!stamp||elapsed<=0||elapsed>.12||(head-previousHead).LengthSqr()>units*units*.25f) {
            Reset();stamp=now;previousHead=head;for(int h=0;h<2;++h){previousHand[h]=local[h];handValid[h]=tracked[h]&&TwoHandAim::Finite(local[h]);}return out;
        }
        for(float value:{hz,damping,strength,jumpGain,swingGain,touchGain,maxResponse,chestScale,hipScale})if(!std::isfinite(value)){Reset();return out;}
        const float dt=float(elapsed);auto velocity=(head-previousHead)*(1/(units*dt));
        auto acceleration=(velocity-headVelocity)*(1/dt);acceleration=PropInteraction::Limit(acceleration,30);
        float drive=acceleration.z*std::clamp(jumpGain,0.f,3.f)+TwoHandAim::Dot(acceleration,forward)*.25f;
        float swing=0;
        const Vector centers[2]={Local(chestCenter-head,forward,right,units)+Vector(.10f,0,.01f),
                                 Local(hipCenter-head,forward,right,units)+Vector(-.10f,0,-.045f)};
        const Vector radii[2]={{.065f*std::clamp(chestScale,.75f,2.f),.15f*std::clamp(chestScale,.75f,2.f),.085f},
                              {.065f*std::clamp(hipScale,.75f,2.f),.17f*std::clamp(hipScale,.75f,2.f),.09f}};
        for(int h=0;h<2;++h) {
            const bool valid=tracked[h]&&TwoHandAim::Finite(local[h]);
            if(valid&&handValid[h]&&(local[h]-previousHand[h]).LengthSqr()<.35f*.35f) {
                const auto handSpeed=PropInteraction::Limit((local[h]-previousHand[h])*(1/dt),6);
                swing+=std::min(12.f,sqrtf(((handSpeed-handVelocity[h])*(1/dt)).LengthSqr()))*.5f;
                for(int region=0;region<2;++region)if(now>=contactAfter[h][region]&&Crosses(previousHand[h],local[h],centers[region],radii[region])) {
                    const float kick=std::min(6.f,sqrtf(handSpeed.LengthSqr()))*.10f*std::clamp(touchGain,0.f,3.f);
                    (region?hips:chest).velocity-=kick;contactAfter[h][region]=now+.18;out.touches|=1u<<(h+region*2);
                }
                handVelocity[h]=handSpeed;
            }else handVelocity[h]={0,0,0};
            handValid[h]=valid;previousHand[h]=local[h];
        }
        drive+=swing*std::clamp(swingGain,0.f,2.f);
        chest.Step(dt,drive,hz,damping);hips.Step(dt,drive*.8f,hz*.85f,damping);
        const float gain=18*std::clamp(strength,0.f,4.f),limit=std::clamp(maxResponse,0.f,.4f);
        out.chest=std::clamp(chest.offset*gain,-limit,limit);out.hips=std::clamp(hips.offset*gain,-limit,limit);
        const float motionGain=std::clamp(jumpGain,0.f,3.f);
        breastSide.Step(dt,TwoHandAim::Dot(acceleration,right)*motionGain,hz,damping);
        breastFront.Step(dt,TwoHandAim::Dot(acceleration,forward)*motionGain,hz,damping);
        gluteSide.Step(dt,TwoHandAim::Dot(acceleration,right)*motionGain*.8f,hz*.85f,damping);
        gluteFront.Step(dt,TwoHandAim::Dot(acceleration,forward)*motionGain*.8f,hz*.85f,damping);
        // Legacy response percentage now bounds positional movement: 20% =
        // 2 cm maximum, rather than a 20% expansion of the entire torso.
        out.breast=PropInteraction::Limit({breastFront.offset*gain*.06f,breastSide.offset*gain*.06f,out.chest*.1f},limit*.1f);
        out.glute=PropInteraction::Limit({gluteFront.offset*gain*.06f,gluteSide.offset*gain*.06f,out.hips*.1f},limit*.1f);
        stamp=now;previousHead=head;headVelocity=velocity;return out;
    }
};
}
