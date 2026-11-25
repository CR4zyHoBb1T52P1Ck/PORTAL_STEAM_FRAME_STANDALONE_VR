#pragma once
#include "twohandaim.h"
namespace WeaponHandling {
inline Vector Shoulder(const Vector& head,const Vector& forward,const Vector& right,int physical,float side,float back,float down) {
    return head+right*((physical?1.f:-1.f)*side)-forward*back-Vector(0,0,down);
}
inline matrix3x4_t Rebase(const matrix3x4_t& newController,const matrix3x4_t& oldGun) {
    return HandPose::Concat(HandPose::InverseRigid(newController),oldGun);
}
enum Event {None,Stow,Draw,Transfer};
struct State {
    int hand=1;
    bool stowed=false,previousGrip[2]{},blocked[2]{},transferSupport=false,fireBlocked=false;
    void Suspend(){blocked[0]=blocked[1]=true;transferSupport=false;fireBlocked=true;}
    Event Update(const bool grip[2],const bool tracked[2],const bool shoulder[2],bool support,bool holsterEnabled,bool transferEnabled) {
        bool rising[2]{},falling[2]{};
        for(int h=0;h<2;++h) {
            if(!tracked[h])blocked[h]=true;
            if(tracked[h]&&!grip[h])blocked[h]=false;
            rising[h]=tracked[h]&&!blocked[h]&&grip[h]&&!previousGrip[h];
            falling[h]=tracked[h]&&!grip[h]&&previousGrip[h];
        }
        Event event=None;
        if(holsterEnabled&&stowed) {
            for(int h=0;h<2;++h)if(rising[h]&&shoulder[h]){hand=h;stowed=false;event=Draw;break;}
        } else if(holsterEnabled&&!support&&rising[hand]&&shoulder[hand]){stowed=true;transferSupport=false;event=Stow;}
        else if(!stowed&&transferEnabled&&support&&falling[hand]&&grip[1-hand]&&tracked[1-hand]&&!blocked[1-hand]) {
            hand=1-hand;transferSupport=false;event=Transfer;
        }
        if(event!=None)fireBlocked=true;
        for(int h=0;h<2;++h)previousGrip[h]=grip[h];
        return event;
    }
    void Triggers(bool left,bool right){if(!left&&!right)fireBlocked=false;}
};
}
