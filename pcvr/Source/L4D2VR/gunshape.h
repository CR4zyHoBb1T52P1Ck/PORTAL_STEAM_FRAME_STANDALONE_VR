#pragma once
#include "twohandaim.h"
#include <array>
// Authored visual contact proxy only; not an engine collision object.
namespace GunShape {
struct Capsule {float a[3],b[3],radius;};
inline constexpr std::array<Capsule,4> Housing={{
    {{.15f,2.35f,5.7f},{.15f,2.35f,14.f},4.25f},
    {{.15f,.1f,2.8f},{.15f,.8f,5.8f},1.15f},
    // Front barrel and muzzle: native gun surface reaches z~30 with animated
    // claw tips; the old rear-only proxy stopped at18.25. Approximate the tube,
    // not the empty space between opened claws. Never used as a player hull.
    {{.15f,2.35f,17.f},{.15f,2.35f,25.5f},3.5f},
    // Front_Cover lower white shell reaches y~-2.69 in the owned bind mesh.
    {{.29f,.7f,19.2f},{.29f,.7f,24.f},3.4f}
}};
struct Bounds {float low[3],high[3];};
template<size_t N> constexpr Bounds BoundsFor(const std::array<Capsule,N>& shapes){
    static_assert(N>0,"nonempty proxy required");Bounds result{};
    for(int axis=0;axis<3;++axis){
        result.low[axis]=std::min(shapes[0].a[axis],shapes[0].b[axis])-shapes[0].radius;
        result.high[axis]=std::max(shapes[0].a[axis],shapes[0].b[axis])+shapes[0].radius;
        for(size_t i=1;i<N;++i){result.low[axis]=std::min(result.low[axis],std::min(shapes[i].a[axis],shapes[i].b[axis])-shapes[i].radius);
            result.high[axis]=std::max(result.high[axis],std::max(shapes[i].a[axis],shapes[i].b[axis])+shapes[i].radius);}
    }return result;
}
inline constexpr Bounds HousingBounds=BoundsFor(Housing);
inline Vector Start(const Capsule& c){return {c.a[0],c.a[1],c.a[2]};}
inline Vector Axis(const Capsule& c){return Vector(c.b[0],c.b[1],c.b[2])-Start(c);}
inline Vector Radial(Vector point,const Capsule& c){const auto a=Start(c),d=Axis(c);
    const float length=d.LengthSqr();const float t=length>0?std::clamp(TwoHandAim::Dot(point-a,d)/length,0.f,1.f):0;
    return point-a-d*t;
}
inline float Distance(Vector point,const Capsule& c){return sqrtf(Radial(point,c).LengthSqr())-c.radius;}
inline float Distance(Vector point){float result=Distance(point,Housing.front());
    for(size_t i=1;i<Housing.size();++i)result=std::min(result,Distance(point,Housing[i]));return result;}
struct Nearest {float distance=1e20f;Vector radial{1,0,0},axis{0,0,1};};
inline Nearest Closest(Vector point){Nearest result;
    for(const auto& c:Housing){const auto n=Radial(point,c);const float distance=sqrtf(n.LengthSqr())-c.radius;
        if(distance<result.distance){result.distance=distance;result.radial=n;result.axis=Axis(c);}}
    return result;
}
}
