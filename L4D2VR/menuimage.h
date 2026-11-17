#pragma once
#include <vector>
#include <cstdint>
#include <string>
namespace MenuImage {
// Lossless RLE TGA avoids the raw-overlay IPC pixel payload limit. BGRA,
// top-left origin; Valve documents TGA support for SetOverlayFromFile.
inline std::vector<unsigned char> Tga(const std::vector<uint32_t>& rgba,int width,int height) {
    if(width<1||height<1||width>1920||height>1080||rgba.size()!=size_t(width)*height)return {};
    std::vector<unsigned char> out(18,0);out[2]=10;out[12]=width&255;out[13]=width>>8;
    out[14]=height&255;out[15]=height>>8;out[16]=32;out[17]=0x28;
    auto pixel=[&](uint32_t p){out.push_back((p>>16)&255);out.push_back((p>>8)&255);out.push_back(p&255);out.push_back((p>>24)&255);};
    size_t at=0;
    while(at<rgba.size()) {
        size_t run=1;while(run<128&&at+run<rgba.size()&&rgba[at+run]==rgba[at])++run;
        if(run>1){out.push_back(0x80|unsigned(run-1));pixel(rgba[at]);at+=run;continue;}
        const auto start=at++;
        while(at-start<128&&at<rgba.size()) {if(at+1<rgba.size()&&rgba[at]==rgba[at+1])break;++at;}
        out.push_back(unsigned(at-start-1));for(size_t i=start;i<at;++i)pixel(rgba[i]);
    }
    return out;
}
inline std::string NativePath(std::string path) {
    if(path.size()>3&&(path[0]=='z'||path[0]=='Z')&&path[1]==':'&&(path[2]=='\\'||path[2]=='/')) {
        path.erase(0,2);for(auto& c:path)if(c=='\\')c='/';
    }
    return path;
}
}
