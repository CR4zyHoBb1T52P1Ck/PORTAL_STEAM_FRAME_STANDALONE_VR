#pragma once
#include "gunattachments.h"
#include "gunshellquery52.h"
#include <vector>
namespace GunLights {
// Owned retail world gun has one rigid bone. Attachment locals use that SAME
// submitted bone, not the engine weapon entity's unchanged animation pose.
struct WorldModel {
 matrix3x4_t local[256]{};int count=0;
 bool Read(const unsigned char* hdr,size_t size){
  count=0;if(!hdr||size<248)return false;
  auto integer=[&](size_t at){int v;memcpy(&v,hdr+at,4);return v;};
  if(integer(4)!=44||integer(156)!=1)return false;
  constexpr char name[]="weapons/w_portalgun.mdl";
  for(size_t i=0;i<sizeof(name);++i){const char c=hdr[12+i]=='\\'?'/':char(hdr[12+i]);if(c!=name[i])return false;}
  const int n=integer(240),off=integer(244);if(n<1||n>256||off<248||size_t(off)>size||size_t(n)>(size-size_t(off))/92)return false;
  for(int i=0;i<n;++i){const size_t record=size_t(off)+size_t(i)*92;if(integer(record+8)!=0)return false;
   memcpy(&local[i],hdr+record+12,sizeof(matrix3x4_t));for(int r=0;r<3;++r)for(int c=0;c<4;++c)if(!std::isfinite(local[i][r][c]))return false;}
  count=n;return true;
 }
};
struct Pose {
 const WorldModel* model=nullptr;void* owner=nullptr;void* eye=nullptr;uint64_t frame=0;matrix3x4_t bone{};
 matrix3x4_t gunFrame{};bool gunFrameValid=false;
 bool Current(void* weapon,void* target,uint64_t serial)const{return model&&model->count&&owner==weapon&&eye==target&&frame==serial;}
 bool Resolve(int number,matrix3x4_t& out)const{if(!model||number<1||number>model->count)return false;out=HandPose::Concat(bone,model->local[number-1]);return true;}
};
struct Views {
 struct View {Pose pose;Vector camera{0,0,0};bool cameraValid=false;};std::vector<View> stack;
 void Push(){stack.emplace_back();}
 void Push(Vector camera){stack.push_back({{},camera,true});}
 void Pop(){if(!stack.empty())stack.pop_back();}
 void Clear(){stack.clear();}
 void Store(const Pose& p){if(!stack.empty())stack.back().pose=p;}
 bool Camera(Vector& camera)const{if(stack.empty()||!stack.back().cameraValid)return false;camera=stack.back().camera;return true;}
 Pose Current(void* owner,void* eye,uint64_t frame)const{
  return !stack.empty()&&stack.back().pose.Current(owner,eye,frame)?stack.back().pose:Pose{};
 }
};
// Freeze one attachment palette/controller pose per sprite draw. Invalidated
// unconditionally at scope end, never shared across eyes/portal views/frames.
class DrawScope {
 inline static thread_local DrawScope* active=nullptr;
 DrawScope* previous;bool viewmodelReady=false;
 matrix3x4_t aligned[256];int count=0;
public:
 const bool owned,world;const Pose pose;unsigned queries=0,builds=0,reuses=0;
 matrix3x4_t gunFrame{};bool gunFrameValid=false;Vector camera{0,0,0};bool cameraValid=false;
 void* indicatorMaterial=nullptr;bool indicatorAttempted=false;
 GunShell::Prepared shell;bool shellAttempted=false;
 DrawScope(bool own,bool w,Pose p={}):previous(active),owned(own),world(w),pose(p){active=this;}
 ~DrawScope(){active=previous;}
 static DrawScope* Active(){return active;}
 bool ResolveCached(int number,matrix3x4_t& out){
  ++queries;if(!viewmodelReady||number<1||number>count)return false;out=aligned[number-1];++reuses;return true;
 }
 void StoreViewmodel(const GunAttachments::Model& metadata,const matrix3x4_t& controller,const matrix3x4_t* bones){
  viewmodelReady=false;count=metadata.count;if(count<1||count>256)return;
  for(int i=1;i<=count;++i)if(!metadata.Resolve(i,controller,bones,aligned[i-1]))return;
  viewmodelReady=true;++builds;
  gunFrame=HandPose::Concat(controller,metadata.gunFromController);gunFrameValid=true;
 }
};
}
