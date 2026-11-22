#pragma once
#include "gunhandcontact.h"
namespace SelfContact {
struct Capsule {Vector a,b;float radius=1;};
inline float Distance(Vector p,const Capsule& c){const auto d=c.b-c.a;const float n=d.LengthSqr();return sqrtf((p-c.a-d*(n>1e-6f?std::clamp(TwoHandAim::Dot(p-c.a,d)/n,0.f,1.f):0)).LengthSqr())-c.radius;}
struct Scene {
 FingerSurface::Shape torso,abdomen;Capsule upper,forearm,palm;
 float Distance(Vector p)const{return std::min({torso.Distance(p),abdomen.Distance(p),SelfContact::Distance(p,upper),SelfContact::Distance(p,forearm),SelfContact::Distance(p,palm)});}
};
struct Result {matrix3x4_t frame{};bool touching=false;};
inline Result Resolve(const matrix3x4_t& raw,const Scene& scene,float units,bool palmAnchor,bool orient){
 Result out{raw,false};if(!GunHandContact::Rigid(raw)||!std::isfinite(units)||units<1)return out;
 const float gap=units*.006f;const auto point=TwoHandAim::Position(raw)+TwoHandAim::Axis(raw,0)*(palmAnchor?0.f:units*.045f);auto safe=point;Vector normal=TwoHandAim::Axis(raw,1);
 for(int i=0;i<16;++i){const float depth=gap-scene.Distance(safe);if(!std::isfinite(depth)||depth<=.0005f)break;
  const float e=units*.001f;Vector n{scene.Distance(safe+Vector(e,0,0))-scene.Distance(safe-Vector(e,0,0)),scene.Distance(safe+Vector(0,e,0))-scene.Distance(safe-Vector(0,e,0)),scene.Distance(safe+Vector(0,0,e))-scene.Distance(safe-Vector(0,0,e))};
  if(!TwoHandAim::Unit(n))n=normal;normal=n;safe+=n*(depth+.001f);
 }
 // Overlapping torso lobes can point their gradients at each other. Search
 // exits in the tracked palm frame rather than oscillating inside the union.
 if(scene.Distance(safe)<gap-.001f){
  float best=units*.3f;bool found=false;Vector direction=normal;
  for(int ray=0;ray<7;++ray){const auto d=ray==6?normal:TwoHandAim::Axis(raw,ray/2)*(ray%2?-1.f:1.f);
   for(int step=1;step<=12;++step){const float t=units*.3f*step/12;if(t>best)break;
    if(scene.Distance(point+d*t)<gap)continue;
    float low=units*.3f*(step-1)/12,high=t;
    for(int k=0;k<8;++k){const float mid=(low+high)*.5f;if(scene.Distance(point+d*mid)<gap)low=mid;else high=mid;}
    best=high;direction=d;found=true;break;
   }
  }
  if(!found)return out;safe=point+direction*best;normal=direction;
 }
 const auto shift=safe-point;if(shift.LengthSqr()>units*units*.3f*.3f||shift.LengthSqr()<.000001f)return out;
 out.touching=true;out.frame=HandContact::Surface(raw,normal,TwoHandAim::Position(raw),orient);
 const auto position=safe-TwoHandAim::Axis(out.frame,0)*(palmAnchor?0.f:units*.045f);
 for(int r=0;r<3;++r)out.frame[r][3]=position[r];return out;
}
}
