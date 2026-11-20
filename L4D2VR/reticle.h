#pragma once
#include "twohandaim.h"
#include <vector>
namespace Reticle {
inline std::vector<uint32_t> Pixels(int shape,int color,float opacity) {
    const uint32_t colors[]={0xffffff,0x40dfff,0xff9c40,0x70ff80,0xff70ef};
    const auto alpha=uint32_t(std::clamp(opacity,0.f,1.f)*255);
    std::vector<uint32_t> result(128*128);
    for(int y=0;y<128;++y)for(int x=0;x<128;++x) {
        const int dx=std::abs(x-64),dy=std::abs(y-64);const float radius=sqrtf(float(dx*dx+dy*dy));
        bool lit=shape==1?radius<=9:shape==2?((dx<=3&&dy>=7&&dy<=35)||(dy<=3&&dx>=7&&dx<=35)):shape==3?(radius>=25&&radius<=30):false;
        const bool outline=shape==1?radius<=12:shape==2?((dx<=5&&dy>=5&&dy<=37)||(dy<=5&&dx>=5&&dx<=37)):shape==3?(radius>=23&&radius<=32):false;
        if(lit||outline){const auto rgb=lit?colors[std::clamp(color,0,4)]:0x080808;result[y*128+x]=(alpha<<24)|((rgb&255)<<16)|(rgb&0xff00)|((rgb>>16)&255);}
    }return result;
}
inline float Width(int shape,float mm,float distance) {
    if(!std::isfinite(mm)||!std::isfinite(distance))return .05f;
    const float visiblePixels=shape==1?18.f:shape==2?70.f:60.f;
    return std::clamp(std::clamp(mm,2.f,40.f)*.001f*std::clamp(distance,.15f,30.f)*128/visiblePixels,.002f,9.f);
}
inline Vector TrackingOffset(Vector delta,Vector forward,Vector right,Vector up,float units) {
    if(units<1||!TwoHandAim::Finite(delta))return {0,0,0};
    return {TwoHandAim::Dot(delta,right)/units,TwoHandAim::Dot(delta,up)/units,-TwoHandAim::Dot(delta,forward)/units};
}
}
