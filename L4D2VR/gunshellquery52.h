#pragma once
#include "gunshellmesh.h"
namespace GunShell {
// One immutable owned draw/eye/portal scope. No cross-frame result cache.
class Prepared {
 struct PartPose {matrix3x4_t inverse{},relative{};Vector eye{};unsigned index=0;};
 const Mesh* mesh=nullptr;matrix3x4_t rootInverse{};Vector rootEye{};
 std::array<PartPose,21> poses{};unsigned count=0;
public:
 unsigned queries=0,partTransforms=0;
 bool Prepare(const Mesh& source,const matrix3x4_t& gun,const matrix3x4_t* animated,Vector eye){
  mesh=nullptr;count=queries=partTransforms=0;
  if(!source.ready||source.parts.size()>poses.size()||!GunHandContact::Rigid(gun)||!TwoHandAim::Finite(eye))return false;
  rootInverse=HandPose::InverseRigid(gun);rootEye=Transform(rootInverse,eye);
  for(unsigned i=0;i<source.parts.size();++i){const auto& part=source.parts[i];
   const auto relative=animated?animated[part.bone-24]:part.bind;if(!GunHandContact::Rigid(relative))continue;
   auto& pose=poses[count++];pose.index=i;pose.relative=relative;
   pose.inverse=HandPose::InverseRigid(HandPose::Concat(gun,relative));pose.eye=Transform(pose.inverse,eye);++partTransforms;
  }
  mesh=&source;return true;
 }
 bool Occluded(Vector light,bool indicator=false,float aperture=0,unsigned* tested=nullptr){
  ++queries;if(!mesh||!TwoHandAim::Finite(light)||!std::isfinite(aperture))return false;
  const auto rootLight=Transform(rootInverse,light);
  for(unsigned i=0;i<count;++i){const auto& pose=poses[i];const auto& part=mesh->parts[pose.index];
   const auto a=pose.eye,b=Transform(pose.inverse,light);unsigned stack[64],used=1;stack[0]=part.root;
   while(used){const auto& node=mesh->nodes[stack[--used]];if(!Bounds(a,b,node))continue;
    if(node.count){for(unsigned k=0;k<node.count;++k){float t;if(tested)++*tested;if(Hit(a,b,mesh->triangles[node.first+k],t)){
     const auto rootHit=Transform(pose.relative,a+(b-a)*t);if(indicator&&part.bone==24&&TopWindow(rootEye,rootLight,rootHit,aperture))continue;return true;}}}
    else{if(used+2>64)return false;stack[used++]=node.left;stack[used++]=node.right;}
   }
  }return false;
 }
};
}
