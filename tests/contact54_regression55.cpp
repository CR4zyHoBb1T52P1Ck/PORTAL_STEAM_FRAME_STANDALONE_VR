#include <Windows.h>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
#include "bodyik.h"
#include "portalpose.h"
#include "avatarpalette.h"
#include "vrmenupaint.h"
static void Near54(const matrix3x4_t& a,const matrix3x4_t& b,float tolerance=.0008f){for(int r=0;r<3;++r)for(int c=0;c<4;++c)assert(std::abs(a[r][c]-b[r][c])<tolerance);}
static bool Roll54(const BodyIK::Rig& rig,matrix3x4_t* bones,int hand,ForearmRoll54::History& state,uint64_t frame,float gain,float elbow,float helper){return ForearmRoll54::Apply(bones[rig.arm[hand][1]],bones[rig.wristTwist[hand]],bones[rig.arm[hand][2]],rig.forearmWristFromElbow[hand],rig.forearmHelperFromElbow[hand],rig.forearmStation[hand],state,hand,frame,gain,elbow,helper);}
int main(int argc,char** argv){
 _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);assert(argc==2);
 std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> mdl{std::istreambuf_iterator<char>(file),{}};BodyIK::Rig rig;assert(rig.Read(mdl.data(),mdl.size()));assert(rig.count==68);
 unsigned cases=0,seams=0;
 const auto portal=PortalPose::Frame({400,-900,80},{33,121,-24});
 for(int hand=0;hand<2;++hand)for(int degrees=-150;degrees<=150;degrees+=15)for(float gain:{0.f,.5f,1.f}){
  matrix3x4_t bones[BodyIK::Max]{},before[BodyIK::Max]{},mapped[BodyIK::Max]{};memcpy(bones,rig.bind,sizeof(bones));const int elbow=rig.arm[hand][1],helper=rig.wristTwist[hand],wrist=rig.arm[hand][2];
  const auto origin=TwoHandAim::Position(bones[elbow]),point=TwoHandAim::Position(bones[wrist]);auto axis=point-origin;assert(TwoHandAim::Unit(axis));const float angle=degrees*.01745329252f;
  const auto rotation=HandPose::RotateAroundAxis(axis,angle);bones[wrist]=HandPose::Concat(rotation,bones[wrist]);for(int r=0;r<3;++r)bones[wrist][r][3]=point[r];
  // Contact may redirect the elbow AFTER wrist solving; reconstruct from final arm.
  if(degrees%30==0){FingerSurface::Shape obstacle;obstacle.frame=PortalPose::Frame((origin+point)*.5f,{0,0,0});obstacle.radii={2,3,2};rig.AvoidGunArm(bones,hand,obstacle,43.2f);}
  memcpy(before,bones,sizeof(bones));for(int i=0;i<rig.count;++i)mapped[i]=HandPose::Concat(portal,bones[i]);
  ForearmRoll54::History first,second;const float a=hand?.15f:.35f,b=hand?.65f:.9f;assert(Roll54(rig,bones,hand,first,1,gain,a,b));assert(Roll54(rig,mapped,hand,second,1,gain,a,b));
  for(int i=0;i<rig.count;++i){if(i!=elbow&&i!=helper)assert(!memcmp(&bones[i],&before[i],sizeof(matrix3x4_t)));Near54(mapped[i],HandPose::Concat(portal,bones[i]));}
  for(int r=0;r<3;++r)assert(bones[elbow][r][3]==before[elbow][r][3]);assert(!memcmp(&bones[wrist],&before[wrist],sizeof(matrix3x4_t)));
  assert(GunHandContact::Rigid(bones[elbow])&&GunHandContact::Rigid(bones[helper]));
  auto segment=TwoHandAim::Position(before[wrist])-TwoHandAim::Position(before[elbow]);const auto expected=TwoHandAim::Position(before[elbow])+segment*rig.forearmStation[hand];assert((expected-TwoHandAim::Position(bones[helper])).LengthSqr()<.000001f);
  {matrix3x4_t shaped[BodyIK::Max]{},oldShape[BodyIK::Max]{};memcpy(shaped,bones,sizeof(shaped));memcpy(oldShape,before,sizeof(oldShape));rig.Volume(shaped,1.05f,1.1f,1.12f,1.1f,1,1.3f);rig.Volume(oldShape,1.05f,1.1f,1.12f,1.1f,1,1.3f);for(int i=0;i<rig.count;++i)if(i!=elbow&&i!=helper)assert(!memcmp(&shaped[i],&oldShape[i],sizeof(matrix3x4_t)));}
  // The final palette remains shared by direct/portal copies.
  AvatarPalette::Snapshot palette;palette.Store(1,mdl.data(),rig.bind[0],bones,rig.count);matrix3x4_t view[BodyIK::Max]{};palette.Copy(view,HandPose::Concat(portal,rig.bind[0]),false);for(int i=0;i<rig.count;++i)Near54(view[i],HandPose::Concat(portal,bones[i]));++cases;
 }
 for(int hand=0;hand<2;++hand){ForearmRoll54::History history;matrix3x4_t last{};bool had=false;
  for(int degrees=150;degrees<=570;++degrees){matrix3x4_t bones[BodyIK::Max]{};memcpy(bones,rig.bind,sizeof(bones));const int e=rig.arm[hand][1],w=rig.arm[hand][2],h=rig.wristTwist[hand];auto axis=TwoHandAim::Position(bones[w])-TwoHandAim::Position(bones[e]);assert(TwoHandAim::Unit(axis));const auto point=TwoHandAim::Position(bones[w]);bones[w]=HandPose::Concat(HandPose::RotateAroundAxis(axis,degrees*.01745329252f),bones[w]);for(int r=0;r<3;++r)bones[w][r][3]=point[r];assert(Roll54(rig,bones,hand,history,degrees-149,.5f,.25f,.75f));
   if(had){float change=0;for(int r=0;r<3;++r)for(int c=0;c<3;++c)change+=powf(bones[h][r][c]-last[r][c],2);assert(change<.001f);++seams;}last=bones[h];had=true;
  }
 }
 ForearmRoll54::History history;const float p=3.12f,n=-3.12f;assert(history.Resolve(0,p,1)==p);assert(history.Resolve(0,n,2)>p);assert(history.Resolve(0,0,2)>p);assert(history.Resolve(0,n,8)==n);history.Reset();assert(!history.valid[0]&&!history.valid[1]);
 {matrix3x4_t bones[BodyIK::Max]{},before[BodyIK::Max]{};memcpy(bones,rig.bind,sizeof(bones));bones[rig.arm[0][1]][0][0]=NAN;memcpy(before,bones,sizeof(bones));assert(!Roll54(rig,bones,0,history,1,.5f,.3f,.8f));assert(!memcmp(bones,before,sizeof(bones)));}
 // Existing callers retain exact legacy/default behavior; zero IK is unchanged.
 {matrix3x4_t a[BodyIK::Max]{},b[BodyIK::Max]{};memcpy(a,rig.bind,sizeof(a));memcpy(b,rig.bind,sizeof(b));matrix3x4_t hands[2]={rig.bind[rig.arm[0][2]],rig.bind[rig.arm[1][2]]};const bool gripping[2]={false,false};
  assert(rig.SolveNative(a,rig.Eyes(a),hands,{1,0,0},1,1,true,nullptr,gripping));assert(rig.SolveNative(b,rig.Eyes(b),hands,{1,0,0},1,1,true,nullptr,gripping,nullptr,1,.5f,false,1,1,1,false,nullptr,0,false));assert(!memcmp(a,b,sizeof(a)));
  memcpy(a,rig.bind,sizeof(a));assert(rig.SolveNative(a,rig.Eyes(a),hands,{1,0,0},1,0,true,nullptr,gripping,nullptr,1,.5f,false,1,1,1,false,nullptr,0,true));assert(!memcmp(a,rig.bind,sizeof(a)));
 }
 VrGraphics::Settings settings;assert(settings.naturalForearms);for(int page=0;page<VrMenu::PageCount;++page)assert(VrMenu::Rows(settings,VrMenu::Page(page)).size()<=12);assert(VrMenu::PageCount==57&&VrMenu::Options().size()==263);assert(VrMenu::Parent(VrMenu::ForearmRoll)==VrMenu::BodyTuning);
 printf("Contact54 CPU: %u real-rig roll/contact/portal cases,%u continuous seam transitions,all unaffected joints/wrists/fingers bit-exact,invalid guards,legacy and zeroIK parity,55bounded pages/256options pass.\n",cases,seams);
}
