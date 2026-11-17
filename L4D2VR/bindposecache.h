#pragma once
#include "handpose.h"
#include <array>
#include <cstdint>
namespace BindPoseCache {
// Only immutable model-space bind matrices are cached. Tracked/animated/world
// matrices must still be recomputed independently for each draw and eye.
struct Entry {
    const unsigned char* header=nullptr;
    int length=0,count=0,offset=0,checksum=0;
    unsigned rebuilds=0;
    std::array<matrix3x4_t,128> matrices;
    const matrix3x4_t* Get(const unsigned char* source,int size,int bones,int at,int identity) {
        if(!source||bones<1||bones>128||at<164||size<at||bones>(size-at)/216)return nullptr;
        if(header!=source||length!=size||count!=bones||offset!=at||checksum!=identity) {
            for(int i=0;i<bones;++i) {
                matrix3x4_t inverse;std::memcpy(&inverse,source+at+i*216+96,sizeof(inverse));
                matrices[i]=HandPose::InverseRigid(inverse);
            }
            header=source;length=size;count=bones;offset=at;checksum=identity;++rebuilds;
        }
        return matrices.data();
    }
};
}
