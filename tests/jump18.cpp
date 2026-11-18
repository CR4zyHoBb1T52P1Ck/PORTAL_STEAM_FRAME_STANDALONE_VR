#include <Windows.h>
#include <cassert>
#include <fstream>
#include <vector>
#include <cstdio>
#include "avatarpalette.h"
#include "portalpose.h"
#include "menuimage.h"
static std::vector<unsigned char> Read(const char* path){std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
static Vector Point(const matrix3x4_t& m,Vector p){return {m[0][0]*p.x+m[0][1]*p.y+m[0][2]*p.z+m[0][3],m[1][0]*p.x+m[1][1]*p.y+m[1][2]*p.z+m[1][3],m[2][0]*p.x+m[2][1]*p.y+m[2][2]*p.z+m[2][3]};}
static void Near(Vector a,Vector b,float e=.01f){assert((a-b).LengthSqr()<e*e);}
struct Vertex{float weights[3];unsigned char bones[3],count;Vector position,normal;float uv[2];};
int main(int argc,char** argv){
 assert(argc==4);const auto mdl=Read(argv[1]),vvd=Read(argv[2]);BodyIK::Rig rig;assert(rig.Read(mdl.data(),mdl.size()));
 int count,start;memcpy(&count,vvd.data()+16,4);memcpy(&start,vvd.data()+56,4);assert(count>20000&&start+count*48<=int(vvd.size()));
 const auto model=HandPose::Frame({1,0,0},{0,0,1},{0,-1,0},{100,200,0});matrix3x4_t native[128],posed[128],portal[128],rest[128];
 for(int i=0;i<rig.count;++i)rest[i]=HandPose::Concat(model,rig.bind[i]);
 unsigned cases=0,vertices=0,packets=0;float worstKnee=-1000,maxLengthError=0;
 for(float tuck:{-80.f,0.f,20.f,45.f,80.f})for(float offset:{-20.f,0.f,20.f})for(float length:{.8f,1.f,1.2f})for(float crouch:{0.f,-25.f}) {
  memcpy(native,rest,rig.count*48);const Vector jump{12,-7,25};
  for(int i=0;i<rig.count;++i)for(int r=0;r<3;++r)native[i][r][3]+=jump[r];
  // Regression: animated feet can rise above the hips while the HMD-fitted
  // torso stays upright. Also exercise unreachable feet and landing targets.
  for(int h=0;h<2;++h)rig.Shift(native,rig.leg[h][2],{offset,offset*.3f,tuck});
  memcpy(posed,native,rig.count*48);auto eye=rig.Eyes(rest)+jump+Vector(0,0,crouch*.432f);
  rig.FitUpperBody(posed,eye,{0,-1,0},43.2f,150,41,25.5f,9.5f,0,1,crouch,true,true,1,1,true,length,3);
  for(int h=0;h<2;++h){const auto hip=TwoHandAim::Position(posed[rig.leg[h][0]]),knee=TwoHandAim::Position(posed[rig.leg[h][1]]),ankle=TwoHandAim::Position(posed[rig.leg[h][2]]);
   const float upper=sqrtf((TwoHandAim::Position(rig.bind[rig.leg[h][1]])-TwoHandAim::Position(rig.bind[rig.leg[h][0]])).LengthSqr())*length;
   const float lower=sqrtf((TwoHandAim::Position(rig.bind[rig.leg[h][2]])-TwoHandAim::Position(rig.bind[rig.leg[h][1]])).LengthSqr())*length;
   const float error=std::max(std::abs(sqrtf((knee-hip).LengthSqr())-upper),std::abs(sqrtf((ankle-knee).LengthSqr())-lower));maxLengthError=std::max(maxLengthError,error);assert(error<.01f);
   assert(ankle.z<hip.z);if(crouch==0){worstKnee=std::max(worstKnee,knee.z-hip.z);assert(knee.z<hip.z+.01f);}
  }
  for(int i=0;i<count;++i){Vertex v;memcpy(&v,vvd.data()+start+i*48,48);Vector p{0,0,0};for(int j=0;j<v.count;++j){assert(v.bones[j]<rig.count);p+=Point(HandPose::Concat(posed[v.bones[j]],HandPose::InverseRigid(rig.bind[v.bones[j]])),v.position)*v.weights[j];}assert(TwoHandAim::Finite(p));++vertices;}
  AvatarPalette::Snapshot snapshot;snapshot.Store(1,mdl.data(),native[0],posed,rig.count);
  for(float pitch:{-90.f,0.f,85.f}){const auto link=PortalPose::Frame({-200,300,-100},{pitch,170,35});snapshot.Copy(portal,HandPose::Concat(link,native[0]),false);for(int i=0;i<rig.count;++i){const auto expected=HandPose::Concat(link,posed[i]);Near(TwoHandAim::Position(portal[i]),TwoHandAim::Position(expected));}}
  if(tuck==45&&offset==0&&length==1&&crouch==0){std::ofstream f(argv[3],std::ios::binary);f.write(reinterpret_cast<char*>(&rig.count),4);f.write(reinterpret_cast<char*>(posed),rig.count*48);f.write(reinterpret_cast<char*>(&posed[rig.arm[0][2]]),48);f.write(reinterpret_cast<char*>(&posed[rig.arm[1][2]]),48);}
  ++cases;
 }
 // Exercise every file-backed overlay's dimensions and alpha/RLE boundaries.
 for(auto size:{std::pair<int,int>{1,1},{32,32},{256,256},{320,80},{1200,1060},{1280,720}}){const int w=size.first,h=size.second;std::vector<uint32_t> pixels(w*h,0);for(size_t i=0;i<pixels.size();i+=7)pixels[i]=uint32_t(i)*0x01943219u;
  const auto tga=MenuImage::Tga(pixels,w,h);assert(!tga.empty());std::vector<uint32_t> decoded;size_t at=18;
  while(at<tga.size()){unsigned packet=tga[at++],n=(packet&127)+1;auto read=[&](){assert(at+4<=tga.size());uint32_t b=tga[at++],g=tga[at++],r=tga[at++],a=tga[at++];return r|(g<<8)|(b<<16)|(a<<24);};if(packet&128){auto p=read();decoded.insert(decoded.end(),n,p);}else for(unsigned j=0;j<n;++j)decoded.push_back(read());++packets;}assert(decoded==pixels);
 }
 printf("PASS: %u tucked-jump/landing/crouch/scale leg cases; %u weighted native mesh vertices; 270 floor/wall/ceiling portal rebases; max limb-length error=%f units, highest standing knee above hip=%f units; six RGBA overlay sizes / %u lossless RLE packets\n",cases,vertices,maxLengthError,worstKnee,packets);
}
