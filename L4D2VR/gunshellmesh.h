#pragma once
#include "gunhandcontact.h"
#include <fstream>
#include <filesystem>
#include <vector>
#include <array>
namespace GunShell {
struct Header{char magic[8];uint32_t version;int32_t checksum;uint32_t parts,nodes,triangles;};
struct Part{uint32_t bone,root;matrix3x4_t bind;};
struct Node{float low[3],high[3];uint32_t left,right,first,count;};
struct Triangle{float p[3][3];};
static_assert(sizeof(Header)==28&&sizeof(Part)==56&&sizeof(Node)==40&&sizeof(Triangle)==36,"private wire layout");
inline Vector Vec(const float* p){return {p[0],p[1],p[2]};}
inline Vector Transform(const matrix3x4_t& m,Vector p){return {m[0][0]*p.x+m[0][1]*p.y+m[0][2]*p.z+m[0][3],m[1][0]*p.x+m[1][1]*p.y+m[1][2]*p.z+m[1][3],m[2][0]*p.x+m[2][1]*p.y+m[2][2]*p.z+m[2][3]};}
inline bool Bounds(Vector a,Vector b,const Node& n){float enter=0,leave=1;for(int c=0;c<3;++c){const float d=b[c]-a[c];
 if(std::abs(d)<1e-7f){if(a[c]<n.low[c]||a[c]>n.high[c])return false;continue;}
 float x=(n.low[c]-a[c])/d,y=(n.high[c]-a[c])/d;if(x>y)std::swap(x,y);enter=std::max(enter,x);leave=std::min(leave,y);if(enter>leave)return false;}return true;}
inline bool Hit(Vector a,Vector b,const Triangle& tri,float& t){
 const auto edge1=Vec(tri.p[1])-Vec(tri.p[0]),edge2=Vec(tri.p[2])-Vec(tri.p[0]),direction=b-a,p=CrossProduct(direction,edge2);const float det=TwoHandAim::Dot(edge1,p);
 if(std::abs(det)<1e-7f)return false;const float inverse=1/det;const auto s=a-Vec(tri.p[0]);const float u=TwoHandAim::Dot(s,p)*inverse;if(u<-.00001f||u>1.00001f)return false;
 const auto q=CrossProduct(s,edge1);const float v=TwoHandAim::Dot(direction,q)*inverse;if(v<-.00001f||u+v>1.00001f)return false;t=TwoHandAim::Dot(edge2,q)*inverse;
 const float length=sqrtf(direction.LengthSqr());return length>.02f&&t>.005f/length&&t<1-.02f/length;
}
inline bool TopWindow(Vector eye,Vector light,Vector hit,float radius){
 return radius>0&&eye.y>std::max(6.5f,light.y+.15f)&&hit.y>=light.y-.1f&&hit.y<=light.y+1.1f&&
        (hit.x-light.x)*(hit.x-light.x)+(hit.z-light.z)*(hit.z-light.z)<=radius*radius;
}
class Mesh {
public:
 bool ready=false;int checksum=0;std::vector<Part> parts;std::vector<Node> nodes;std::vector<Triangle> triangles;
 bool Read(const unsigned char* data,size_t length){
  ready=false;parts.clear();nodes.clear();triangles.clear();if(!data||length<sizeof(Header)||length>4*1024*1024)return false;Header h;memcpy(&h,data,sizeof(h));
  if(memcmp(h.magic,"PVRSH51",8)||h.version!=1||!h.parts||h.parts>21||!h.nodes||h.nodes>32768||!h.triangles||h.triangles>20000)return false;
  const size_t expected=sizeof(h)+size_t(h.parts)*sizeof(Part)+size_t(h.nodes)*sizeof(Node)+size_t(h.triangles)*sizeof(Triangle);if(length!=expected)return false;
  size_t at=sizeof(h);parts.resize(h.parts);memcpy(parts.data(),data+at,parts.size()*sizeof(Part));at+=parts.size()*sizeof(Part);
  nodes.resize(h.nodes);memcpy(nodes.data(),data+at,nodes.size()*sizeof(Node));at+=nodes.size()*sizeof(Node);triangles.resize(h.triangles);memcpy(triangles.data(),data+at,triangles.size()*sizeof(Triangle));
  std::array<bool,21> bones{};for(const auto& p:parts){if(p.bone<24||p.bone>=45||bones[p.bone-24]||p.root>=nodes.size()||!GunHandContact::Rigid(p.bind))return false;bones[p.bone-24]=true;}
  for(const auto& t:triangles)for(const auto& p:t.p)for(float f:p)if(!std::isfinite(f)||std::abs(f)>1024)return false;
  for(size_t i=0;i<nodes.size();++i){const auto& n=nodes[i];for(int c=0;c<3;++c)if(!std::isfinite(n.low[c])||!std::isfinite(n.high[c])||n.low[c]>n.high[c]||std::abs(n.low[c])>1024||std::abs(n.high[c])>1024)return false;
   if(n.count){if(n.count>6||n.first>=triangles.size()||n.count>triangles.size()-n.first)return false;
    for(unsigned k=0;k<n.count;++k)for(const auto& p:triangles[n.first+k].p)for(int c=0;c<3;++c)if(p[c]<n.low[c]-.001f||p[c]>n.high[c]+.001f)return false;
   }else{if(n.left<=i||n.right<=i||n.left>=nodes.size()||n.right>=nodes.size()||n.left==n.right)return false;
    for(unsigned child:{n.left,n.right})for(int c=0;c<3;++c)if(nodes[child].low[c]<n.low[c]-.001f||nodes[child].high[c]>n.high[c]+.001f)return false;}
  }
  std::vector<bool> seen(nodes.size(),false),seenTri(triangles.size(),false);std::vector<unsigned> stack;
  for(const auto& p:parts){stack={p.root};while(!stack.empty()){const auto i=stack.back();stack.pop_back();if(seen[i])return false;seen[i]=true;const auto& n=nodes[i];
   if(n.count){for(unsigned k=0;k<n.count;++k){if(seenTri[n.first+k])return false;seenTri[n.first+k]=true;}}else{stack.push_back(n.left);stack.push_back(n.right);}}}
  for(bool value:seen)if(!value)return false;for(bool value:seenTri)if(!value)return false;checksum=h.checksum;ready=true;return true;
 }
 bool Load(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f){ready=false;return false;}const auto size=f.tellg();if(size<=0||size>4*1024*1024){ready=false;return false;}std::vector<unsigned char> bytes(static_cast<size_t>(size));f.seekg(0);if(!f.read(reinterpret_cast<char*>(bytes.data()),bytes.size())){ready=false;return false;}return Read(bytes.data(),bytes.size());}
 bool Occluded(const matrix3x4_t& gun,const matrix3x4_t* animated,Vector eye,Vector light,bool indicator=false,float aperture=0,unsigned* tested=nullptr)const{
  if(!ready||!GunHandContact::Rigid(gun)||!TwoHandAim::Finite(eye)||!TwoHandAim::Finite(light)||!std::isfinite(aperture))return false;
  const auto rootInverse=HandPose::InverseRigid(gun);const auto rootEye=Transform(rootInverse,eye),rootLight=Transform(rootInverse,light);
  for(const auto& part:parts){const auto relative=animated?animated[part.bone-24]:part.bind;if(!GunHandContact::Rigid(relative))continue;
   const auto inverse=HandPose::InverseRigid(HandPose::Concat(gun,relative));const auto a=Transform(inverse,eye),b=Transform(inverse,light);unsigned stack[64],used=1;stack[0]=part.root;
   while(used){const auto& node=nodes[stack[--used]];if(!Bounds(a,b,node))continue;
    if(node.count){for(unsigned k=0;k<node.count;++k){float t;if(tested)++*tested;if(Hit(a,b,triangles[node.first+k],t)){
     const auto rootHit=Transform(relative,a+(b-a)*t);if(indicator&&part.bone==24&&TopWindow(rootEye,rootLight,rootHit,aperture))continue;return true;}}}
    else{if(used+2>64)return false;stack[used++]=node.left;stack[used++]=node.right;}
   }
  }return false;
 }
};
}
