#pragma once
#include "gunhandcontact.h"
namespace GunAnimation {
struct State {
 bool metadata=false,valid=false;uint64_t serial=0;matrix3x4_t bindFront{},front{},delta{},parts[21]{};int version=0,checksum=0,failure=0;
 bool Read(const unsigned char* hdr,size_t size,const matrix3x4_t* reference){
  metadata=valid=false;failure=1;if(!hdr||!reference||size<248)return false;
  auto integer=[&](size_t at){int v;memcpy(&v,hdr+at,4);return v;};
  version=integer(4);checksum=integer(8);failure=2;if((version!=46&&version!=48)||integer(156)!=45)return false;const int off=integer(160);
  failure=3;if(off<248||size_t(off)>size||45>(size-size_t(off))/216)return false;
  const size_t at=size_t(off)+25*216;const int name=integer(at);constexpr char wanted[]="ValveBiped.Front_Cover";
  failure=4;if(integer(at+4)!=24||name<=0||size_t(name)>size-at||sizeof(wanted)>size-at-size_t(name)||memcmp(hdr+at+name,wanted,sizeof(wanted)))return false;
  bindFront=HandPose::Concat(HandPose::InverseRigid(reference[24]),reference[25]);front=bindFront;
  delta=HandPose::Frame({1,0,0},{0,1,0},{0,0,1},{0,0,0});metadata=GunHandContact::Rigid(bindFront);failure=metadata?0:5;return metadata;
 }
 bool Update(const matrix3x4_t* bones,uint64_t frame){
  valid=false;serial=frame;failure=6;if(!metadata||!bones)return false;
  for(int i=24;i<45;++i)if(!GunHandContact::Rigid(bones[i])){failure=100+i;return false;}
  const auto inverseRoot=HandPose::InverseRigid(bones[24]);for(int i=24;i<45;++i)parts[i-24]=HandPose::Concat(inverseRoot,bones[i]);
  const auto next=HandPose::Concat(HandPose::InverseRigid(bones[24]),bones[25]);
  const auto change=HandPose::Concat(next,HandPose::InverseRigid(bindFront));
  failure=7;if(!GunHandContact::Rigid(change)||TwoHandAim::Position(change).LengthSqr()>144)return false;
  front=next;delta=change;valid=true;failure=0;return true;
 }
 void Shape(FingerSurface::Shape& shape)const {if(valid){shape.frontValid=true;shape.frontFrame=HandPose::Concat(shape.frame,delta);}}
};
struct Grip {
 bool bound=false,pending=false;matrix3x4_t local{};
 void Reset(){bound=pending=false;}
 void Capture(const matrix3x4_t& part,const matrix3x4_t& hand){pending=false;bound=GunHandContact::Rigid(part)&&GunHandContact::Rigid(hand);if(bound)local=HandPose::Concat(HandPose::InverseRigid(part),hand);}
 matrix3x4_t Resolve(const matrix3x4_t& part)const{return HandPose::Concat(part,local);}
};
inline bool FrontClosest(const FingerSurface::Shape& shape,Vector point){
 auto local=[&](const matrix3x4_t& frame){const auto d=point-TwoHandAim::Position(frame);return Vector(TwoHandAim::Dot(d,TwoHandAim::Axis(frame,0)),TwoHandAim::Dot(d,TwoHandAim::Axis(frame,1)),TwoHandAim::Dot(d,TwoHandAim::Axis(frame,2)));};
 const auto rear=local(shape.frame),front=local(shape.frontValid?shape.frontFrame:shape.frame);
 const float a=std::min(GunShape::Distance(rear,GunShape::Housing[0]),GunShape::Distance(rear,GunShape::Housing[1]));
 const float b=std::min(GunShape::Distance(front,GunShape::Housing[2]),GunShape::Distance(front,GunShape::Housing[3]));return b<a;
}
}
