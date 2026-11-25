#pragma once
#include "reticle.h"
namespace GunIndicator {
// Native initializer0x1024b950: Body_light effects0..5 only. All other
// indices belong to claw, inside or beam effects and keep their native colours.
inline bool Top(int effect){return effect>=0&&effect<=5;}
// The completed rear shell comes from the world mesh. Its Body_light anchor
// transformed by GunGrab::WorldFit differs from the old viewmodel attachment.
inline Vector HousingAnchor(){return {.16198497f,6.3010722f,12.2262473f};}
inline bool Tint(int effect,int lastShot,void* color){
 if(!Top(effect)||lastShot<0||lastShot>1||!color)return false;
 const auto rgb=Reticle::TintRGB(lastShot?2:1);auto* bytes=static_cast<unsigned char*>(color);bytes[0]=static_cast<unsigned char>((rgb>>16)&255);bytes[1]=static_cast<unsigned char>((rgb>>8)&255);bytes[2]=static_cast<unsigned char>(rgb&255);return true;
}
}
