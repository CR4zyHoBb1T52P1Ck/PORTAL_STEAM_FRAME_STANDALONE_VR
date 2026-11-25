#pragma once
#include "propinteraction.h"
#include <cctype>
#include <ctime>
namespace CreatorCamera {
struct State {
    Vector origin{0,0,0};QAngle angles{0,0,0};bool ready=false;double stamp=0;
    void Reset(Vector p,QAngle a){origin=p;angles=a;ready=true;stamp=0;}
    void Move(float forward,float side,float up,float units,float distance) {
        Vector f,r;QAngle::AngleVectors(angles,&f,&r,nullptr);origin+=(f*forward+r*side+Vector(0,0,up))*(units*distance);
    }
    void Turn(float yaw,float pitch){angles.y+=yaw;angles.x=std::clamp(angles.x+pitch,-85.f,85.f);angles.z=0;}
    void View(int mode,Vector head,QAngle headAngles,float units,float distance,float height,float orbit,double now,Vector& p,QAngle& a) {
        if(!ready)Reset(head,headAngles);
        const double dt=stamp?std::clamp(now-stamp,0.,.1):0;stamp=now;
        if(mode==1){Vector f;QAngle::AngleVectors({0,headAngles.y,0},&f,nullptr,nullptr);p=head-f*(units*distance)+Vector(0,0,height*units);QAngle::VectorAngles(head-p,a);}
        else if(mode==2){angles.y+=orbit*float(dt);Vector f;QAngle::AngleVectors({0,angles.y,0},&f,nullptr,nullptr);p=head-f*(units*distance)+Vector(0,0,height*units);QAngle::VectorAngles(head-p,a);}
        else if(mode==3){p=origin;a=angles;}else {p=head;a=headAngles;}
    }
};
inline bool TakeName(const std::string& name){return name.size()>14&&name.size()<80&&name.rfind("portalvr_take_",0)==0&&std::all_of(name.begin(),name.end(),[](unsigned char c){return std::isalnum(c)||c=='_';});}
}
