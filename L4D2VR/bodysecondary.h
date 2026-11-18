#pragma once
#include "twohandaim.h"
namespace BodySecondary {
// Cosmetic damped volume response only: never translate bones, hands or camera.
struct Spring {
    Vector previous{0,0,0},velocity{0,0,0},offset{0,0,0},rate{0,0,0};double stamp=0;
    void Reset(){stamp=0;offset=rate=velocity={0,0,0};}
    float Step(Vector position,double now,float units,float hz,float damping,float strength) {
        const double dt=now-stamp;
        if(!TwoHandAim::Finite(position)||!std::isfinite(now)||!std::isfinite(units)||!std::isfinite(hz)||!std::isfinite(damping)||!std::isfinite(strength)||units<1){Reset();return 0;}
        if(!stamp||dt<=0||dt>.1||(position-previous).LengthSqr()>units*units*.25f){previous=position;stamp=now;velocity=offset=rate={0,0,0};return 0;}
        auto next=(position-previous)*(1/float(dt));auto acceleration=(next-velocity)*(1/float(dt));
        const float magnitude=sqrtf(acceleration.LengthSqr());if(magnitude>units*25)acceleration=acceleration*(units*25/magnitude);
        const float omega=6.2831853f*std::clamp(hz,2.f,10.f),d=std::clamp(damping,.2f,2.f);
        int steps=std::max(1,int(std::ceil(dt/.003)));float tick=float(dt)/steps;
        for(int i=0;i<steps;++i){rate+=(acceleration*(-1.f/units)-offset*(omega*omega)-rate*(2*d*omega))*tick;offset+=rate*tick;}
        previous=position;velocity=next;stamp=now;
        return std::clamp((offset.x+offset.y+offset.z)*std::clamp(strength,0.f,1.f),-.025f,.025f);
    }
    static void Volume(matrix3x4_t& bone,float amount) {
        const float scale=1+std::clamp(amount,-.025f,.025f);
        for(int r=0;r<3;++r)for(int c=0;c<3;++c)bone[r][c]*=scale;
    }
};
}
