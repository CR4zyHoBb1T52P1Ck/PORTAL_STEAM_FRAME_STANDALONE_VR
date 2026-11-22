#pragma once
#include "posetwist.h"
#include "gunhandcontact.h"
#include <cstdint>
namespace ForearmRoll54 {
struct History {
 bool valid[2]{};uint64_t frame[2]{};float angle[2]{};
 bool bounded[2]{};double stamp[2]{};
 void Reset(){*this={};}
 void Drop(int hand){if(hand>=0&&hand<2)valid[hand]=false;}
 float Resolve(int hand,float raw,uint64_t serial){
  if(hand<0||hand>1||!std::isfinite(raw))return 0;
  if(bounded[hand])valid[hand]=false;bounded[hand]=false;
  if(valid[hand]&&serial==frame[hand])return angle[hand];
  float next=raw;
  if(valid[hand]&&serial>frame[hand]&&serial-frame[hand]<=2){
   constexpr float tau=6.28318530718f;next=angle[hand]+std::remainder(raw-angle[hand],tau);
   if(std::abs(next)>64*tau)next=raw; // bound numerical history, not app lifetime
  }
  valid[hand]=true;frame[hand]=serial;return angle[hand]=next;
 }
 float ResolveBounded(int hand,float raw,uint64_t serial,double now){
  if(hand<0||hand>1||!std::isfinite(raw)||!std::isfinite(now))return 0;
  if(!bounded[hand])valid[hand]=false;bounded[hand]=true;
  if(valid[hand]&&serial==frame[hand])return angle[hand];
  const float target=std::clamp(std::remainder(raw,6.28318530718f),-2.617993878f,2.617993878f);const double dt=now-stamp[hand];
  const bool continuous=valid[hand]&&serial>frame[hand]&&serial-frame[hand]<=2&&dt>0&&dt<=.15;
  angle[hand]=continuous?angle[hand]+std::clamp(target-angle[hand],-float(dt)*20,float(dt)*20):target;
  frame[hand]=serial;stamp[hand]=now;valid[hand]=true;return angle[hand];
 }
};
inline bool Apply(matrix3x4_t& elbow,matrix3x4_t& helper,const matrix3x4_t& wrist,
 const matrix3x4_t& wristFromElbow,const matrix3x4_t& helperFromElbow,float station,
 History& history,int hand,uint64_t serial,float distribution,float elbowShare,float helperShare,double stamp=-1){
 if(hand<0||hand>1||!GunHandContact::Rigid(elbow)||!GunHandContact::Rigid(helper)||!GunHandContact::Rigid(wrist)||
    !GunHandContact::Rigid(wristFromElbow)||!GunHandContact::Rigid(helperFromElbow)||
    !std::isfinite(station)||!std::isfinite(distribution)||!std::isfinite(elbowShare)||!std::isfinite(helperShare)) {history.Drop(hand);return false;}
 const auto a=TwoHandAim::Position(elbow),b=TwoHandAim::Position(wrist);auto axis=b-a;
 if(!TwoHandAim::Unit(axis)){history.Drop(hand);return false;}
 const auto neutralWrist=HandPose::Concat(elbow,wristFromElbow);
 const auto delta=HandPose::Concat(wrist,HandPose::InverseRigid(neutralWrist));
 const float raw=PoseTwist::Angle(delta,axis);if(!std::isfinite(raw)){history.Drop(hand);return false;}
 const float angle=stamp>=0?history.ResolveBounded(hand,raw,serial,stamp):history.Resolve(hand,raw,serial),gain=std::clamp(distribution,0.f,1.f);
 auto middle=HandPose::Concat(elbow,helperFromElbow);
 middle=HandPose::Concat(HandPose::RotateAroundAxis(axis,angle*std::clamp(helperShare*gain,0.f,1.f)),middle);
 const auto point=a+(b-a)*std::clamp(station,0.f,1.f);for(int r=0;r<3;++r)middle[r][3]=point[r];
 auto lower=HandPose::Concat(HandPose::RotateAroundAxis(axis,angle*std::clamp(elbowShare*gain,0.f,1.f)),elbow);
 for(int r=0;r<3;++r)lower[r][3]=a[r];
 // World palettes are independent: never reanchor the wrist/finger branch here.
 elbow=lower;helper=middle;return true;
}
}
