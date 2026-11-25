#pragma once
#include "handpose.h"
#include <algorithm>
namespace MenuPose {
inline Vector Position(const matrix3x4_t& p) {return {p[0][3],p[1][3],p[2][3]};}
inline Vector Axis(const matrix3x4_t& p,int c) {return {p[0][c],p[1][c],p[2][c]};}
inline void Translate(matrix3x4_t& p,const Vector& value) {for(int r=0;r<3;++r)p[r][3]=value[r];}
inline matrix3x4_t Front(const matrix3x4_t& head,float depth) {
    Vector forward=-Axis(head,2);forward.y=0;
    if(forward.LengthSqr()<.01f)forward={0,0,-1};VectorNormalize(forward);
    const Vector up(0,1,0),right=CrossProduct(forward,up);
    return HandPose::Frame(right,up,-forward,Position(head)+forward*std::clamp(depth,.4f,4.f)+Vector(0,-.05f,0));
}
struct Hit {bool hit=false;float x=0,y=0,distance=0;Vector point;};
// Two tangent panels with a clear gap. The shortcut puts settings at zero yaw.
inline matrix3x4_t Beside(const matrix3x4_t& head,float depth,float width,int panel,bool focusMod) {
    const float radius=std::clamp(std::max(depth,width*1.1f),.4f,4.f);
    const auto front=Front(head,radius);const Vector right=Axis(front,0),forward=-Axis(front,2);
    const float half=std::atan((width*.5f+.06f)/radius);
    const float angle=focusMod?(panel?0:-2*half):(panel?half:-half);
    const Vector direction=forward*std::cos(angle)+right*std::sin(angle);
    return HandPose::Frame(right*std::cos(angle)-forward*std::sin(angle),{0,1,0},-direction,
        Position(head)+direction*radius+Vector(0,-.05f,0));
}
inline int Nearest(const Hit& nativeHit,const Hit& modHit) {
    if(modHit.hit&&(!nativeHit.hit||modHit.distance<=nativeHit.distance+.001f))return 1;
    return nativeHit.hit?0:-1;
}
struct StickChord {
    bool latched=false;
    bool Update(bool left,bool right) {
        if(!left&&!right)latched=false;
        if(left&&right&&!latched){latched=true;return true;}
        return false;
    }
};
inline Hit Intersect(const matrix3x4_t& panel,float width,float heightRatio,
    const Vector& source,const Vector& direction,int pixelWidth,int pixelHeight) {
    Hit result;const Vector normal=Axis(panel,2),center=Position(panel);
    const float denominator=DotProduct(direction,normal);
    if(denominator>=-.0001f || width<=0 || heightRatio<=0) return result;
    const float distance=DotProduct(center-source,normal)/denominator;
    if(!std::isfinite(distance) || distance<.02f || distance>8) return result;
    result.point=source+direction*distance;const Vector relative=result.point-center;
    result.x=(.5f+DotProduct(relative,Axis(panel,0))/width)*pixelWidth;
    result.y=(.5f-DotProduct(relative,Axis(panel,1))/(width*heightRatio))*pixelHeight;
    result.distance=distance;
    result.hit=std::isfinite(result.x)&&std::isfinite(result.y)&&result.x>=0&&result.x<pixelWidth&&result.y>=0&&result.y<pixelHeight;
    return result;
}
inline matrix3x4_t Drag(const matrix3x4_t& panel,const matrix3x4_t& previousHand,const matrix3x4_t& hand) {
    return HandPose::Concat(HandPose::Concat(hand,HandPose::InverseRigid(previousHand)),panel);
}
inline void Depth(matrix3x4_t& panel,const Vector& head,float delta) {
    Vector radial=Position(panel)-head;float length=std::sqrt(radial.LengthSqr());if(length<.01f)return;
    Translate(panel,head+radial*(std::clamp(length+delta,.4f,4.f)/length));
}
inline int Corner(float x,float y,int width,int height) {
    const bool left=x<55,right=x>width-55,top=y<55,bottom=y>height-55;
    return (left||right)&&(top||bottom) ? (bottom?2:0)+(right?1:0) : -1;
}
inline matrix3x4_t Beam(const Vector& source,const Vector& destination,const Vector& head) {
    Vector x=destination-source;const float length=std::sqrt(x.LengthSqr());
    if(length<.001f)return HandPose::Frame({1,0,0},{0,1,0},{0,0,1},source);
    x*=1/length;const Vector center=(source+destination)*.5f;
    Vector z=head-center;z-=x*DotProduct(z,x);
    if(z.LengthSqr()<.0001f)z={0,1,0};VectorNormalize(z);
    Vector y=CrossProduct(z,x);VectorNormalize(y);z=CrossProduct(x,y);
    return HandPose::Frame(x,y,z,center);
}
}
