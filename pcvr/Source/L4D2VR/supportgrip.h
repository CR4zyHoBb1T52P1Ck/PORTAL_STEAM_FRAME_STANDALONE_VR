#pragma once
#include "gunhandcontact.h"
namespace SupportGrip {
inline matrix3x4_t Surface(const matrix3x4_t& raw,const FingerSurface::Shape& gun,float units,bool palm,float gapMM){
 GunHandContact::State contact;auto result=contact.Resolve(raw,gun,units,palm,gapMM);
 auto point=TwoHandAim::Position(result)+TwoHandAim::Axis(result,0)*(palm?0:units*.045f);
 const auto normal=GunHandContact::Normal(gun,point,TwoHandAim::Axis(result,1));
 point-=normal*(gun.Distance(point)-units*gapMM/1000);point=GunHandContact::Project(point,gun,units*gapMM/1000,normal);
 result=HandContact::Surface(result,normal,TwoHandAim::Position(result),true);
 const auto p=point-TwoHandAim::Axis(result,0)*(palm?0:units*.045f);for(int r=0;r<3;++r)result[r][3]=p[r];return result;
}
// A visual grip is captured AFTER contact correction. It never changes raw
// tracking or a physics object's owner, and never recaptures while held.
struct Latch {
 bool valid=false;matrix3x4_t local{};
 void Reset(){valid=false;}
 bool Capture(const matrix3x4_t& gun,const matrix3x4_t& hand){
  if(!GunHandContact::Rigid(gun)||!GunHandContact::Rigid(hand)){Reset();return false;}
  local=HandPose::Concat(HandPose::InverseRigid(gun),hand);valid=true;return true;
 }
 matrix3x4_t Resolve(const matrix3x4_t& gun)const{return HandPose::Concat(gun,local);}
};
}
