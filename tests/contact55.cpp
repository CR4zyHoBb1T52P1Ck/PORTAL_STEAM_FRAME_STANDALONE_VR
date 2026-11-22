#include <Windows.h>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
#include "bodyik.h"
#include "skintrial55.h"
#include "portalpose.h"
#include "vrmenupaint.h"
static void Near55(const matrix3x4_t& a,const matrix3x4_t& b){for(int r=0;r<3;++r)for(int c=0;c<4;++c)assert(std::abs(a[r][c]-b[r][c])<.002f);}
int main(int argc,char** argv){
 _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);assert(argc==2);
 std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> mdl{std::istreambuf_iterator<char>(file),{}};BodyIK::Rig rig;assert(rig.Read(mdl.data(),mdl.size()));unsigned cases=0;
 for(int hand=0;hand<2;++hand)for(float factor:{.5f,.7f,.9f,1.f,1.25f,1.5f})for(float distance:{2.f,5.f,12.f,25.f,40.f}){
  matrix3x4_t bones[BodyIK::Max]{};memcpy(bones,rig.bind,sizeof(bones));const auto* chain=rig.arm[hand];const auto shoulder=TwoHandAim::Position(bones[chain[0]]);const float upper=sqrtf((TwoHandAim::Position(bones[chain[1]])-shoulder).LengthSqr()),lower=sqrtf((TwoHandAim::Position(bones[chain[2]])-TwoHandAim::Position(bones[chain[1]])).LengthSqr())*factor;
  const auto originalLocal=HandPose::Concat(HandPose::InverseRigid(bones[chain[2]]),bones[rig.fingers[hand][1][1]]);const Vector target=shoulder+Vector(hand?-distance:distance,0,0);
  for(int repeat=0;repeat<6;++repeat){assert(rig.ArmSegments(bones,chain,target,{0,-1,-1},upper,lower,true));const auto a=TwoHandAim::Position(bones[chain[1]]),b=TwoHandAim::Position(bones[chain[2]]);assert(std::abs(sqrtf((b-a).LengthSqr())-lower)<.004f);assert((b-target).LengthSqr()<.0001f);assert((TwoHandAim::Position(bones[chain[0]])-shoulder).LengthSqr()<.000001f);}
  Near55(originalLocal,HandPose::Concat(HandPose::InverseRigid(bones[chain[2]]),bones[rig.fingers[hand][1][1]]));
  matrix3x4_t beforeSkin[BodyIK::Max]{};memcpy(beforeSkin,bones,sizeof(bones));rig.ArmSkinLengths(bones,hand);
  for(int i=0;i<rig.count;++i){for(int r=0;r<3;++r)assert(bones[i][r][3]==beforeSkin[i][r][3]);if(rig.below[chain[2]][i])assert(!memcmp(&bones[i],&beforeSkin[i],sizeof(matrix3x4_t)));}
  const auto skin=HandPose::Concat(bones[chain[1]],HandPose::InverseRigid(rig.bind[chain[1]]));const auto originalTip=TwoHandAim::Position(rig.bind[chain[2]]);Vector rendered;for(int r=0;r<3;++r)rendered[r]=skin[r][0]*originalTip.x+skin[r][1]*originalTip.y+skin[r][2]*originalTip.z+skin[r][3];assert((rendered-TwoHandAim::Position(bones[chain[2]])).LengthSqr()<.0001f);++cases;
 }
 {matrix3x4_t a[BodyIK::Max]{},b[BodyIK::Max]{};memcpy(a,rig.bind,sizeof(a));memcpy(b,rig.bind,sizeof(b));matrix3x4_t hands[2]={rig.bind[rig.arm[0][2]],rig.bind[rig.arm[1][2]]};const bool grips[2]={false,true};const float ones[2]={1,1};assert(rig.SolveNative(a,rig.Eyes(a),hands,{1,0,0},1,1,true,nullptr,grips));assert(rig.SolveNative(b,rig.Eyes(b),hands,{1,0,0},1,1,true,nullptr,grips,nullptr,1,.5f,false,1,1,1,false,nullptr,0,false,ones,ones));assert(!memcmp(a,b,sizeof(a)));}
 unsigned bounded=0;for(int hand=0;hand<2;++hand){ForearmRoll54::History state;float previous=0;
  for(int i=0;i<1800;++i){const float raw=std::remainder(float(i)*.07f,6.283185307f),nowAngle=state.ResolveBounded(hand,raw,i+1,1+i/72.);assert(std::abs(nowAngle)<=2.617994f);if(i)assert(std::abs(nowAngle-previous)<=20.f/72+.0001f);assert(state.ResolveBounded(hand,0,i+1,1+i/72.)==nowAngle);previous=nowAngle;++bounded;}
  assert(std::abs(state.ResolveBounded(hand,.2f,5000,99)-.2f)<1e-6f);
 }
 for(int bits=0;bits<16;++bits){const bool marked=bits&1,valid=bits&2,master=bits&4,mode=bits&8;assert(SkinTrial55::Motion(marked,valid?SkinTrial55::Checksum:5,mode?1:0,master)==(marked&&valid&&master&&mode));}
 for(float units:{12.f,43.2f,100.f,1000.f})for(float cap:{0.f,.5f,2.f,99.f}){const auto motion=SkinTrial55::Limit({3,-4,5},units,cap);assert(sqrtf(motion.LengthSqr())<=.10001f);assert(sqrtf(motion.LengthSqr())<=units*std::clamp(cap,0.f,2.f)/1000+.00001f);}
 assert(SkinTrial55::Limit({NAN,0,0},43.2f,2).LengthSqr()==0);
 VrGraphics::Settings settings;assert(settings.boundedForearmRoll&&settings.leftForearmLength==100&&settings.skinTrialMode==0);assert(VrMenu::Options().size()==263&&VrMenu::PageCount==57);for(int p=0;p<VrMenu::PageCount;++p)assert(VrMenu::Rows(settings,VrMenu::Page(p)).size()<=12);
 printf("Contact55 CPU: %u calibrated segment cases with6palm-style repetitions,rigid finger/hand branches,100percent legacy parity;%u bounded roll samples;16skin gates/caps,57bounded pages/263options pass.\n",cases,bounded);
}
