#pragma once
#include "propinteraction.h"
namespace WorldPush {
// Continuous pressure is force (N) integrated over real query time, independent
// of impact gain/cooldown. Player movement is excluded from velocity upstream.
inline Vector Pressure(Vector center,Vector surface,Vector normal,Vector velocity,float radius,
                       float units,float stiffness,float cap,float dt) {
    if(!TwoHandAim::Finite(center)||!TwoHandAim::Finite(surface)||!TwoHandAim::Finite(velocity)
       ||!TwoHandAim::Unit(normal)||!std::isfinite(radius)||!std::isfinite(units)||units<1
       ||!std::isfinite(stiffness)||!std::isfinite(cap)||!std::isfinite(dt)||dt<=0||dt>.12f)return {0,0,0};
    const float depth=std::clamp((radius-TwoHandAim::Dot(center-surface,normal))/units,0.f,.20f);
    if(depth<=0)return {0,0,0};
    const float entering=std::clamp(-TwoHandAim::Dot(velocity,normal)/units,-3.f,3.f);
    const float force=std::clamp(depth*std::clamp(stiffness,0.f,120000.f)+entering*25.f,0.f,std::clamp(cap,0.f,4000.f));
    return -normal*(force*units*dt);
}
struct Sweep {
    bool valid=false;Vector previous{0,0,0};double stamp=0;
    bool Sample(Vector local,double now,float units,Vector& start,Vector& velocity) {
        start=previous;velocity={0,0,0};const double dt=now-stamp;
        const bool usable=valid&&dt>=.008&&dt<=.12&&units>=1&&TwoHandAim::Finite(local)
            &&(local-previous).LengthSqr()<=units*units*.49f;
        if(usable)velocity=(local-previous)*(1/float(dt));
        previous=local;stamp=now;valid=TwoHandAim::Finite(local);
        return usable&&velocity.LengthSqr()<=units*units*144;
    }
};
inline Vector Impulse(Vector velocity,Vector normal,float units,float strength,float maxNS,bool gun) {
    if(!std::isfinite(units)||units<1||!std::isfinite(strength)||!std::isfinite(maxNS)||!TwoHandAim::Finite(velocity)||!TwoHandAim::Unit(normal))return {0,0,0};
    const float entering=-TwoHandAim::Dot(velocity,normal);if(entering<units*.12f)return {0,0,0};
    return PropInteraction::Limit((velocity*.3f-normal*(entering*.7f))*(gun?2.f:.8f)*std::clamp(strength,.1f,900.f),std::clamp(maxNS,2.f,1200.f)*units);
}
struct Cooldown {
    void* entities[8]{};double stamps[8]{};int next=0;
    bool Accept(void* entity,double now,double cooldown=.2) {
        if(!entity)return false;
        for(int i=0;i<8;++i)if(entities[i]==entity&&now-stamps[i]<std::clamp(cooldown,.06,.4))return false;
        entities[next]=entity;stamps[next]=now;next=(next+1)%8;return true;
    }
};
}
