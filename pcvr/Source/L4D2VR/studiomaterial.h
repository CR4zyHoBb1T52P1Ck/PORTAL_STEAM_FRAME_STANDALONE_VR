#pragma once
#include <string>
#include <cstring>
#include <cctype>
namespace StudioMaterial {
inline std::string First(const unsigned char* data,size_t size) {
    if(!data||size<232)return {};
    auto integer=[&](size_t at){int n=0;if(at<=size-4)std::memcpy(&n,data+at,4);return n;};
    auto string=[&](size_t at)->std::string {if(at>=size)return {};const auto* end=static_cast<const unsigned char*>(std::memchr(data+at,0,size-at));return end?std::string(reinterpret_cast<const char*>(data+at),end-(data+at)):std::string();};
    const int textures=integer(204),at=integer(208),folders=integer(212),paths=integer(216);
    if(textures<1||at<232||size_t(at)>size-64||folders<1||paths<232||size_t(paths)>size-4)return {};
    const int name=integer(at),folder=integer(paths);
    if(name<1||size_t(name)>=size-size_t(at)||folder<1)return {};
    auto result=string(folder)+string(size_t(at)+name);
    for(auto& c:result)c=c=='\\'?'/':static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}
}
