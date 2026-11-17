#pragma once
#include "bodyik.h"
namespace AvatarPalette {
struct Snapshot {
    bool valid=false;uint64_t serial=0;const void* model=nullptr;int count=0;
    matrix3x4_t nativeRoot{},posed[BodyIK::Max]{};
    bool Current(uint64_t frame,const void* header) const{return valid&&serial==frame&&model==header;}
    void Store(uint64_t frame,const void* header,const matrix3x4_t& root,const matrix3x4_t* bones,int n) {
        valid=false;if(!bones||n<1||n>BodyIK::Max)return;
        for(int i=0;i<n;++i)for(int r=0;r<3;++r)for(int c=0;c<4;++c)if(!std::isfinite(bones[i][r][c]))return;
        serial=frame;model=header;count=n;nativeRoot=root;std::memcpy(posed,bones,n*sizeof(matrix3x4_t));valid=true;
    }
    void Copy(matrix3x4_t* out,const matrix3x4_t& passNativeRoot,bool direct) const {
        if(!out||!valid)return;
        if(direct){std::memcpy(out,posed,count*sizeof(matrix3x4_t));return;}
        const auto transform=HandPose::Concat(passNativeRoot,HandPose::InverseRigid(nativeRoot));
        for(int i=0;i<count;++i)out[i]=HandPose::Concat(transform,posed[i]);
    }
};
}
