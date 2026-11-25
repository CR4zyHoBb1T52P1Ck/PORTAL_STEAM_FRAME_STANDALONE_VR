#pragma once
#include "bodyik.h"
namespace SoftSkin {
inline bool Marked(const unsigned char* hdr,size_t length) {
    constexpr char marker[]="player/chell_frame_softskin25.mdl";
    return hdr&&length>=248&&!std::memcmp(hdr+12,marker,sizeof(marker));
}
// Slots 65/66 had no retail skin weights. Reconstruct skin palettes from the
// torso, never modify animation caches or attachments in the engine.
inline void Apply(const BodyIK::Rig& rig,matrix3x4_t* bones,Vector breast={0,0,0},Vector glute={0,0,0}) {
    if(rig.count!=68||!bones)return;
    const int parent[2]={rig.chest,rig.hips};const Vector offsets[2]={breast,glute};
    for(int r=0;r<2;++r){
        const auto skin=HandPose::Concat(bones[parent[r]],HandPose::InverseRigid(rig.bind[parent[r]]));
        bones[65+r]=HandPose::Concat(skin,rig.bind[65+r]);
        // Translation only: no changing ribcage width or scaling the skeleton.
        const auto frame=HandPose::Concat(skin,rig.bodyBindFrame);
        for(int axis=0;axis<3;++axis)for(int c=0;c<3;++c)bones[65+r][axis][3]+=frame[axis][c]*offsets[r][c];
    }
}
}
