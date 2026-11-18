#include <cassert>
#include <cstdio>
#include <vector>
#include "overlaypixels.h"
#include "menuraster.h"
#include "vrexternal.h"
struct Queue {bool locked=false;unsigned commands=0;int BeginTextureSubmission(){assert(!locked);locked=true;return 0;}int EndTextureSubmission(){assert(locked);locked=false;return 0;}int LockVulkanQueue(){return BeginTextureSubmission();}int UnlockVulkanQueue(){return EndTextureSubmission();}};
int main(){
 unsigned copied=0;const unsigned w=37,h=23;const int pitch=w*4+17;
 std::vector<uint32_t> source(w*h);std::vector<unsigned char> output(pitch*h+2,0xcd);
 for(unsigned frame=0;frame<5000;++frame){
  for(unsigned i=0;i<source.size();++i)source[i]=uint32_t(i*0x01943219u+frame);
  assert(OverlayPixels::Copy(source.data(),w,h,output.data()+1,pitch));
  assert(output.front()==0xcd&&output.back()==0xcd);
  for(unsigned y=0;y<h;++y){for(unsigned x=0;x<w;++x){auto* p=output.data()+1+y*pitch+x*4;auto s=source[y*w+x];assert(p[0]==((s>>16)&255)&&p[1]==((s>>8)&255)&&p[2]==(s&255)&&p[3]==(s>>24));++copied;}for(unsigned i=w*4;i<unsigned(pitch);++i)assert(output[1+y*pitch+i]==0xcd);}
 }
 const auto previous=output;
 assert(!OverlayPixels::Copy(source.data(),w,h,output.data(),int(w*4-1)));
 assert(!OverlayPixels::Copy(nullptr,w,h,output.data(),pitch));assert(!OverlayPixels::Copy(source.data(),0,h,output.data(),pitch));assert(!OverlayPixels::Copy(source.data(),4097,1,output.data(),pitch));assert(output==previous);
 Queue queue;MenuRaster::Swap swap;
 for(unsigned frame=1;frame<10000;++frame){
  const auto now=uint64_t(frame)*1000;assert(!queue.locked); // upload before acquiring native queue
  if(!swap.Begin(now,int(frame%34)))continue;
  {VrExternal::Guard<Queue,true> guard(&queue);assert(guard&&queue.locked);++queue.commands;
   if(frame%7==0)swap.Failed(now);else assert(swap.Loaded());}
  assert(!queue.locked);
 }
 printf("PASS: %u RGBA/BGRA pixels over 5000 updates; alpha/unaligned padded-row/canary bounds; invalid inputs leave memory intact; %u queued menu commits/retries release the existing submission guard\n",copied,queue.commands);
}
