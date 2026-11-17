#pragma once
#include "handpose.h"
#include "graphicssettings.h"
namespace NativeFit {
inline float FrameDelta(float milliseconds) {return std::isfinite(milliseconds)?std::clamp(milliseconds,0.f,50.f):0.f;}
inline matrix3x4_t Rotate(matrix3x4_t frame,float pitch,float yaw,float roll) {
    constexpr float rad=3.14159265358979323846f/180;
    const auto rotation=HandPose::Concat(HandPose::RotateAroundAxis({0,0,1},yaw*rad),
        HandPose::Concat(HandPose::RotateAroundAxis({0,1,0},pitch*rad),HandPose::RotateAroundAxis({1,0,0},roll*rad)));
    return HandPose::Concat(frame,rotation);
}
inline Vector Offset(const Vector& position,const Vector& forward,const Vector& right,const Vector& up,
    float x,float y,float z,float vrScale) {return position+(forward*x+right*y+up*z)*(vrScale/100);}
inline matrix3x4_t Arm(const Vector& forward,const Vector& right,const Vector& up,const Vector& position,
    bool left,float pitch,float yaw,float roll) {
    // Native Valve wrists have the opposite transverse basis from the custom
    // avatar. A 180-degree forward-axis correction puts the thumbs upright.
    return Rotate(HandPose::Frame(forward,left ? -right : right,left ? up : -up,position),pitch,yaw,roll);
}
inline matrix3x4_t Gun(matrix3x4_t frame,float pitch,float yaw,float roll) {
    constexpr float rad=3.14159265358979323846f/180;
    const auto rotation=HandPose::Concat(HandPose::RotateAroundAxis({0,1,0},yaw*rad),
        HandPose::Concat(HandPose::RotateAroundAxis({1,0,0},-pitch*rad),HandPose::RotateAroundAxis({0,0,1},roll*rad)));
    return HandPose::Concat(frame,rotation);
}
inline float Axis(float value,float deadzone) {
    if(!std::isfinite(value))return 0;
    deadzone=std::clamp(deadzone,0.f,.8f);
    return std::abs(value)<=deadzone ? 0.f : std::copysign((std::min(std::abs(value),1.f)-deadzone)/(1-deadzone),value);
}
inline void BothHandsVisibility(matrix3x4_t* bones,int arms,bool left,bool right) {
    if(!bones)return;
    auto collapse=[&](int begin,int end,int wrist) {
        const auto point=HandPose::Frame({0,0,0},{0,0,0},{0,0,0},{bones[wrist][0][3],bones[wrist][1][3],bones[wrist][2][3]});
        for(int i=begin;i<end;++i)bones[i]=point;
    };
    collapse(0,5,8);
    if(!left)collapse(5,24,8);else if(arms!=0)collapse(5,8,8);
    if(!right)collapse(24,43,27);else if(arms!=0)collapse(24,27,27);
}
inline bool HandModelVisible(bool leftModel,const VrGraphics::Settings& s) {
    return s.arms!=2&&(leftModel?s.leftArm:s.rightArm);
}
inline void Visibility(matrix3x4_t* bones,int count,bool gun,bool leftModel,const VrGraphics::Settings& s) {
    if(!bones || (gun ? count!=45 : count!=43))return;
    auto collapse=[&](int begin,int end,int wrist) {
        const Vector p(bones[wrist][0][3],bones[wrist][1][3],bones[wrist][2][3]);
        const auto point=HandPose::Frame({0,0,0},{0,0,0},{0,0,0},p);
        for(int i=begin;i<end;++i)bones[i]=point;
    };
    if(gun) {
        if(s.arms==2 || !s.rightArm)collapse(0,24,8);
        else if(s.arms==1)collapse(0,8,8);
        if(!s.gun)collapse(24,45,24);
    } else {
        if(s.arms==2 || !s.leftArm || !leftModel)collapse(5,24,8);
        else if(s.arms==1)collapse(5,8,8);
        if(s.arms==2 || !s.rightArm || leftModel)collapse(24,43,27);
        else if(s.arms==1)collapse(24,27,27);
        collapse(0,5,leftModel?8:27);
    }
}
}
