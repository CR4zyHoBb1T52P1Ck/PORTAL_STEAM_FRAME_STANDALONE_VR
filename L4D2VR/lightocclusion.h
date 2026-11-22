#pragma once
#include "gunshape.h"
namespace LightOcclusion {
inline float Segments(Vector p,Vector q,Vector a,Vector b){
 const auto d=q-p,e=b-a,r=p-a;const float dd=d.LengthSqr(),ee=e.LengthSqr(),de=TwoHandAim::Dot(d,e),dr=TwoHandAim::Dot(d,r),er=TwoHandAim::Dot(e,r);
 float s=0,t=0;if(dd<1e-8f&&ee<1e-8f)return r.LengthSqr();
 if(dd<1e-8f)t=std::clamp(er/ee,0.f,1.f);
 else if(ee<1e-8f)s=std::clamp(-dr/dd,0.f,1.f);
 else {const float denom=dd*ee-de*de;s=denom>1e-8f?std::clamp((de*er-dr*ee)/denom,0.f,1.f):0;t=(de*s+er)/ee;
  if(t<0){t=0;s=std::clamp(-dr/dd,0.f,1.f);}else if(t>1){t=1;s=std::clamp((de-dr)/dd,0.f,1.f);}}
 return (p+d*s-a-e*t).LengthSqr();
}
inline bool RearHousing(const matrix3x4_t& gun,Vector eye,Vector light){
 if(!TwoHandAim::Finite(eye)||!TwoHandAim::Finite(light))return false;
 const auto inverse=HandPose::InverseRigid(gun);
 auto local=[&](Vector p){return TwoHandAim::Position(HandPose::Concat(inverse,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},p)));};
 auto a=local(eye),b=local(light),delta=b-a;const float length=sqrtf(delta.LengthSqr());if(length<.2f)return false;
 b-=delta*(.1f/length);const auto& shell=GunShape::Housing[0];const float radius=shell.radius-.20f;
 return Segments(a,b,GunShape::Start(shell),GunShape::Start(shell)+GunShape::Axis(shell))<radius*radius;
}
inline bool Box(Vector a,Vector b,Vector low,Vector high){
 float enter=0,leave=1;for(int c=0;c<3;++c){const float d=b[c]-a[c];
  if(std::abs(d)<1e-6f){if(a[c]<low[c]||a[c]>high[c])return false;continue;}
  float x=(low[c]-a[c])/d,y=(high[c]-a[c])/d;if(x>y)std::swap(x,y);enter=std::max(enter,x);leave=std::min(leave,y);if(enter>leave)return false;
 }return leave>.0001f&&enter<.999f;
}
inline bool Opaque(const matrix3x4_t& gun,const matrix3x4_t& delta,Vector eye,Vector light){
 if(RearHousing(gun,eye,light))return true;
 if(!TwoHandAim::Finite(eye)||!TwoHandAim::Finite(light))return false;
 const auto inverse=HandPose::InverseRigid(HandPose::Concat(gun,delta));
 auto local=[&](Vector p){return TwoHandAim::Position(HandPose::Concat(inverse,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},p)));};
 // A conservative inner patch of the white lower cover, not the barrel/glass
 // volume. This cannot close the intended front opening just to hide a light.
 return Box(local(eye),local(light),{-2.35f,-2.4f,18.8f},{2.7f,-1.8f,24.4f});
}
}
