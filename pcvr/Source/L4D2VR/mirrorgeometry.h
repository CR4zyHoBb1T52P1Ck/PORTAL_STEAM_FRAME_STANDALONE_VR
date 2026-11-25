#pragma once
#include <algorithm>
#include <cmath>
namespace MirrorGeometry {
struct Rect {int left=0,top=0,right=0,bottom=0;int Width()const{return right-left;}int Height()const{return bottom-top;}};
struct Layout {bool valid=false;Rect source,destination;};
inline bool Select(bool enabled,bool reuse,int camera,bool failed,bool inGame,bool cursor,bool modMenu,bool focused){
    return enabled&&reuse&&camera==0&&!failed&&inGame&&!cursor&&!modMenu&&focused;
}
inline Layout Fit(unsigned sw,unsigned sh,unsigned dw,unsigned dh,float u0,float v0,float u1,float v1,bool fill){
    Layout out;
    if(!sw||!sh||!dw||!dh||std::max({sw,sh,dw,dh})>16384)return out;
    if(!std::isfinite(u0)||!std::isfinite(v0)||!std::isfinite(u1)||!std::isfinite(v1)||u1<=u0||v1<=v0)return out;
    auto& s=out.source;auto& d=out.destination;
    s={int(std::floor(std::clamp(u0,0.f,1.f)*sw)),int(std::floor(std::clamp(v0,0.f,1.f)*sh)),int(std::ceil(std::clamp(u1,0.f,1.f)*sw)),int(std::ceil(std::clamp(v1,0.f,1.f)*sh))};
    if(s.Width()<1||s.Height()<1)return out;
    d={0,0,int(dw),int(dh)};
    const double sourceAspect=double(s.Width())/s.Height(),targetAspect=double(dw)/dh;
    if(fill){
        if(sourceAspect>targetAspect){const int width=std::clamp(int(std::lround(s.Height()*targetAspect)),1,s.Width());s.left+=(s.Width()-width)/2;s.right=s.left+width;}
        else {const int height=std::clamp(int(std::lround(s.Width()/targetAspect)),1,s.Height());s.top+=(s.Height()-height)/2;s.bottom=s.top+height;}
    }else{
        if(sourceAspect>targetAspect){const int height=std::clamp(int(std::lround(dw/sourceAspect)),1,int(dh));d.top=(dh-height)/2;d.bottom=d.top+height;}
        else {const int width=std::clamp(int(std::lround(dh*sourceAspect)),1,int(dw));d.left=(dw-width)/2;d.right=d.left+width;}
    }
    out.valid=true;return out;
}
}
