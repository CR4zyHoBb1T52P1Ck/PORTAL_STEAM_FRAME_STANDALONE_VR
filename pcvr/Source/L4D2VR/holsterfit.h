#pragma once
#include "weaponhandling.h"
namespace HolsterFit {
inline Vector Center(Vector head,Vector forward,Vector right,int physical,float width,float back,float down,Vector offsetCM,float unit) {
    return WeaponHandling::Shoulder(head,forward,right,physical,width*unit,back*unit,down*unit)
        +(forward*offsetCM.x+right*offsetCM.y+Vector(0,0,offsetCM.z))*unit;
}
inline bool Capture(Vector point,Vector base,Vector forward,Vector right,float unit,Vector& offsetCM) {
    if(!TwoHandAim::Finite(point)||!TwoHandAim::Finite(base)||!std::isfinite(unit)||unit<=0)return false;
    const auto delta=(point-base)/unit;offsetCM={TwoHandAim::Dot(delta,forward),TwoHandAim::Dot(delta,right),delta.z};
    return TwoHandAim::Finite(offsetCM)&&std::abs(offsetCM.x)<=60&&std::abs(offsetCM.y)<=60&&std::abs(offsetCM.z)<=60;
}
}
