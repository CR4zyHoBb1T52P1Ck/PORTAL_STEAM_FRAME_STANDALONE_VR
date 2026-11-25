#pragma once
#include "graphicssettings.h"
#include <sstream>
#include <iomanip>
#include <limits>
namespace VisualTrial {
// Only the five image settings changed by the comparison presets. Restoring
// must never overwrite calibration, contact, movement, comfort or lighting.
struct Baseline {
    int width=2016,msaa=2,mode=0,output=2016;float sharpness=1.5f;
    static Baseline Capture(const VrGraphics::Settings& s){return {s.eyeWidth,s.msaa,s.upscaler,s.upscaleWidth,s.upscaleSharpness};}
    bool Valid()const{return width>=1024&&width<=3200&&(msaa==0||msaa==2||msaa==4||msaa==8)&&mode>=0&&mode<=3&&output>=1728&&output<=3200&&std::isfinite(sharpness)&&sharpness>=1&&sharpness<=2;}
    void Apply(VrGraphics::Settings& s)const{if(!Valid())return;s.eyeWidth=width;s.msaa=msaa;s.upscaler=mode;s.upscaleWidth=output;s.upscaleSharpness=sharpness;}
    std::string Text()const{std::ostringstream out;out<<"PortalVRImageBaseline 1\n"<<width<<' '<<msaa<<' '<<mode<<' '<<output<<' '<<std::setprecision(std::numeric_limits<float>::max_digits10)<<sharpness<<'\n';return out.str();}
    static bool Read(const std::string& text,Baseline& out){
        if(text.size()>512)return false;
        Baseline parsed;std::string magic,extra;int version=0;std::istringstream input(text);
        if(!(input>>magic>>version>>parsed.width>>parsed.msaa>>parsed.mode>>parsed.output>>parsed.sharpness)||magic!="PortalVRImageBaseline"||version!=1||!parsed.Valid()||(input>>extra))return false;
        out=parsed;return true;
    }
};
}
