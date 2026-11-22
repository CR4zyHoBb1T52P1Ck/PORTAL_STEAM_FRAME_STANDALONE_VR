#pragma once
#include "fingersurface.h"
#include "handcontact.h"
#include <limits>
// Visual rig contact only. Never changes tracked controller input, gun aim,
// native rigid bodies, locomotion or the player's portal collision hull.
namespace GunHandContact {
inline bool Rigid(const matrix3x4_t& m){
    for(int r=0;r<3;++r)for(int c=0;c<4;++c)if(!std::isfinite(m[r][c]))return false;
    for(int a=0;a<3;++a)for(int b=a;b<3;++b){const float d=TwoHandAim::Dot(TwoHandAim::Axis(m,a),TwoHandAim::Axis(m,b));if(std::abs(d-(a==b?1.f:0.f))>.002f)return false;}
    return true;
}
inline bool Same(const matrix3x4_t& a,const matrix3x4_t& b){for(int r=0;r<3;++r)for(int c=0;c<4;++c)if(a[r][c]!=b[r][c])return false;return true;}
inline bool SegmentBox(Vector a,Vector b,float radius,const GunShape::Bounds& bounds=GunShape::HousingBounds){
    // The union of all authored gun capsules, expanded by the palm radius.
    if(!TwoHandAim::Finite(a)||!TwoHandAim::Finite(b)||!std::isfinite(radius)||radius<0)return true;
    float magnitude=radius+1;for(int i=0;i<3;++i)magnitude=std::max(magnitude,std::max(std::abs(bounds.low[i]),std::abs(bounds.high[i]))+radius+1);
    // Different float addition order at a tangent can otherwise cull a point
    // on the expanded capsule. False positives are safe; false negatives aren't.
    const float expanded=radius+magnitude*8*std::numeric_limits<float>::epsilon();
    const Vector low{bounds.low[0]-expanded,bounds.low[1]-expanded,bounds.low[2]-expanded},high{bounds.high[0]+expanded,bounds.high[1]+expanded,bounds.high[2]+expanded};
    float enter=0,leave=1;
    for(int axis=0;axis<3;++axis){const float delta=b[axis]-a[axis];
        if(!std::isfinite(delta))return true; // uncertain bounds never cull contact
        if(std::abs(delta)<.000001f){if(a[axis]<low[axis]||a[axis]>high[axis])return false;continue;}
        float lo=(low[axis]-a[axis])/delta,hi=(high[axis]-a[axis])/delta;if(lo>hi)std::swap(lo,hi);
        enter=std::max(enter,lo);leave=std::min(leave,hi);if(enter>leave)return false;
    }return true;
}
inline Vector Normal(const FingerSurface::Shape& shape,Vector point,Vector fallback={1,0,0}) {
    if(shape.gun&&!shape.frontValid){
        const auto delta=point-TwoHandAim::Position(shape.frame);
        const Vector p{TwoHandAim::Dot(delta,TwoHandAim::Axis(shape.frame,0)),TwoHandAim::Dot(delta,TwoHandAim::Axis(shape.frame,1)),TwoHandAim::Dot(delta,TwoHandAim::Axis(shape.frame,2))};
        const auto closest=GunShape::Closest(p);auto normal=closest.radial,axis=closest.axis;
        if(normal.LengthSqr()<.000001f){
            normal={TwoHandAim::Dot(fallback,TwoHandAim::Axis(shape.frame,0)),TwoHandAim::Dot(fallback,TwoHandAim::Axis(shape.frame,1)),TwoHandAim::Dot(fallback,TwoHandAim::Axis(shape.frame,2))};
            TwoHandAim::Unit(axis);normal-=axis*TwoHandAim::Dot(normal,axis);if(!TwoHandAim::Unit(normal))normal={1,0,0};
        }else TwoHandAim::Unit(normal);
        return TwoHandAim::Axis(shape.frame,0)*normal.x+TwoHandAim::Axis(shape.frame,1)*normal.y+TwoHandAim::Axis(shape.frame,2)*normal.z;
    }
    constexpr float e=.025f;
    Vector n{shape.Distance(point+Vector(e,0,0))-shape.Distance(point-Vector(e,0,0)),
        shape.Distance(point+Vector(0,e,0))-shape.Distance(point-Vector(0,e,0)),
        shape.Distance(point+Vector(0,0,e))-shape.Distance(point-Vector(0,0,e))};
    if(!TwoHandAim::Unit(n)){n=fallback;if(!TwoHandAim::Unit(n))n=TwoHandAim::Axis(shape.frame,0);}
    return n;
}
inline Vector Project(Vector point,const FingerSurface::Shape& shape,float radius,Vector fallback) {
    for(int i=0;i<12;++i){const float depth=radius-shape.Distance(point);if(!std::isfinite(depth)||depth<.0001f)break;
        point+=Normal(shape,point,fallback)*(depth+.002f);}
    return point;
}
struct State {
    bool valid=false,touching=false;Vector localSafe{0,0,0};
    bool cached=false,lastPalm=false,lastGun=false;float lastUnits=0,lastGap=0;Vector lastRadii{};
    matrix3x4_t lastRaw{},lastShape{},lastResult{},lastFront{};bool lastFrontValid=false;
    uint32_t solves=0,reused=0,culled=0;
    void Reset(){valid=touching=cached=false;}
    matrix3x4_t Resolve(const matrix3x4_t& raw,const FingerSurface::Shape& shape,float units,bool palmAnchor,float gapMM=28) {
        // Reject invalid correction inputs before inverse, squared distances or
        // float-to-int step counts. Leave upstream tracking data unchanged.
        if(!std::isfinite(units)||units<1||units>10000||!Rigid(raw)||!Rigid(shape.frame)
            ||(shape.frontValid&&!Rigid(shape.frontFrame))||!TwoHandAim::Finite(shape.radii)||!std::isfinite(gapMM)){Reset();return raw;}
        gapMM=std::clamp(gapMM,0.f,40.f);
        if(cached&&lastUnits==units&&lastGap==gapMM&&lastPalm==palmAnchor&&lastGun==shape.gun
            &&lastRadii.x==shape.radii.x&&lastRadii.y==shape.radii.y&&lastRadii.z==shape.radii.z
            &&lastFrontValid==shape.frontValid&&(!shape.frontValid||Same(shape.frontFrame,lastFront))&&Same(raw,lastRaw)&&Same(shape.frame,lastShape)){++reused;return lastResult;}
        ++solves;
        auto remember=[&](const matrix3x4_t& result){lastRaw=raw;lastShape=shape.frame;lastUnits=units;lastGap=gapMM;lastPalm=palmAnchor;
            lastGun=shape.gun;lastRadii=shape.radii;lastFrontValid=shape.frontValid;lastFront=shape.frontFrame;lastResult=result;cached=true;return result;};
        const auto inverse=HandPose::InverseRigid(shape.frame);
        const auto palm=TwoHandAim::Position(raw)+TwoHandAim::Axis(raw,0)*(palmAnchor?0.f:units*.045f);
        const auto local=HandPose::Concat(inverse,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},palm));
        const auto desired=TwoHandAim::Position(local);const auto normal=TwoHandAim::Axis(raw,1);
        if(!TwoHandAim::Finite(palm)||!TwoHandAim::Finite(desired)){Reset();return raw;}
        auto safe=palm;const float radius=units*gapMM/1000;
        const float travel=(desired-localSafe).LengthSqr();
        const bool sweep=valid&&std::isfinite(travel)&&travel<units*units;
        if(shape.gun&&!SegmentBox(sweep?localSafe:desired,desired,radius,shape.Bounds())){
            localSafe=desired;valid=true;touching=false;++culled;return remember(raw);
        }
        if(sweep){
            // Sweep in gun-local space so moving either controller into the
            // other has the same protection, with a bounded per-frame budget.
            const auto start=TwoHandAim::Position(HandPose::Concat(shape.frame,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},localSafe)));
            const auto delta=palm-start;const int steps=std::clamp(int(sqrtf(delta.LengthSqr())/std::max(.1f,radius*.4f))+1,1,64);
            safe=start;
            for(int i=1;i<=steps;++i){const auto next=start+delta*(float(i)/steps);
                if(shape.Distance(next)<radius){safe=Project(next,shape,radius,normal);break;}safe=next;}
        }
        safe=Project(safe,shape,radius,normal);
        // Release the constraint as soon as the real palm leaves the surface;
        // do not keep a stuck hand magnetized after separating from the gun.
        if(shape.Distance(palm)>radius+.04f&&(palm-safe).LengthSqr()<units*units*.0001f)safe=palm;
        touching=(safe-palm).LengthSqr()>.00001f;
        const auto localResult=HandPose::Concat(inverse,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},safe));
        localSafe=TwoHandAim::Position(localResult);valid=true;
        auto result=HandContact::Surface(raw,Normal(shape,safe,normal),TwoHandAim::Position(raw),touching);
        const auto position=safe-TwoHandAim::Axis(result,0)*(palmAnchor?0.f:units*.045f);
        for(int r=0;r<3;++r)result[r][3]=position[r];return remember(result);
    }
};
inline float SegmentPenalty(Vector a,Vector b,const FingerSurface::Shape& shape,float radius) {
    float worst=0;for(int i=1;i<8;++i)worst=std::max(worst,radius-shape.Distance(a+(b-a)*(float(i)/8)));return std::max(worst,0.f);
}
}
