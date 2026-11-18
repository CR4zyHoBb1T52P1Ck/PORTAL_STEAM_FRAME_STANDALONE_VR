#pragma once
#include <cstdint>
#include <cstring>
namespace OverlayPixels {
inline bool Copy(const uint32_t* rgba,unsigned width,unsigned height,void* destination,int pitch) {
    if(!rgba||!destination||!width||!height||width>4096||height>4096||pitch<int(width*4))return false;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const uint32_t p=rgba[size_t(y)*width+x];
        const uint32_t bgra=(p&0xff00ff00u)|((p&255u)<<16)|((p>>16)&255u);
        std::memcpy(static_cast<unsigned char*>(destination)+size_t(y)*pitch+x*4,&bgra,4);
    }
    return true;
}
}
