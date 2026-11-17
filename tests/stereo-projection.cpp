#include "stereoprojection.h"
#include <cassert>
#include <cstdio>
#include <limits>

using namespace StereoProjection;
static void Near(double a,double b) { assert(std::abs(a-b)<.000002); }
struct V { double x,y,z; };
static V Rotate(V p,int axis,double angle) {
    const double c=std::cos(angle),s=std::sin(angle);
    if(axis==0) return {p.x,c*p.y-s*p.z,s*p.y+c*p.z};
    if(axis==1) return {c*p.x+s*p.z,p.y,-s*p.x+c*p.z};
    return {c*p.x-s*p.y,s*p.x+c*p.y,p.z};
}
int main() {
    // Actual Frame calibration, retained in Frame-Optical-Calibration.private.json.
    const Frustum raw[2]={
        {-1.650734305381775f,1.222975254058838f,-1.7200982570648193f,1.178255319595337f},
        {-1.209002137184143f,1.6722297668457031f,-1.7112325429916382f,1.1834520101547241f}};
    Plan plan;
    assert(Build(raw,plan));
    unsigned cases=0;double oldError=0;
    for(int eye=0;eye<2;++eye) {
        const auto& f=raw[eye];const auto& b=plan.eyes[eye];
        Near(b.uMin,.5+.5*f.left/plan.tanHalfX);
        Near(b.uMax,.5+.5*f.right/plan.tanHalfX);
        Near(b.vMin,.5-.5*f.bottom/plan.tanHalfY);
        Near(b.vMax,.5-.5*f.top/plan.tanHalfY);
        for(double pitch:{-.9,0.,.9}) for(double yaw:{-1.4,0.,1.4})
        for(double roll:{-.8,0.,.8}) for(double lean:{-.25,0.,.25})
        for(double depth:{.3,1.,20.}) for(int x=0;x<=8;++x) for(int y=0;y<=8;++y) {
            const V local={depth*(f.left+(f.right-f.left)*x/8.),
                depth*(f.bottom-(f.bottom-f.top)*y/8.),-depth};
            const V eyeOffset={eye==0?-.03300402686:.03300402686,0,0};
            V world={local.x+eyeOffset.x,local.y,local.z};
            world=Rotate(Rotate(Rotate(world,0,pitch),1,yaw),2,roll);
            world.x+=lean;world.y+=lean*.3;world.z-=lean*.2;
            world.x-=lean;world.y-=lean*.3;world.z+=lean*.2;
            V p=Rotate(Rotate(Rotate(world,2,-roll),1,-yaw),0,-pitch);
            p.x-=eyeOffset.x;
            const double u=.5+.5*p.x/(-p.z)/plan.tanHalfX;
            const double v=.5-.5*p.y/(-p.z)/plan.tanHalfY;
            // Independently compare compositor-sampled symmetric rendering with
            // a direct asymmetric optical projection of the same eye-local ray.
            const double nativeU=(p.x/(-p.z)-f.left)/(f.right-f.left);
            const double nativeV=(f.bottom-p.y/(-p.z))/(f.bottom-f.top);
            Near((u-b.uMin)/(b.uMax-b.uMin),nativeU);
            Near((v-b.vMin)/(b.vMax-b.vMin),nativeV);
            oldError=std::max({oldError,std::abs(u-nativeU),std::abs(v-nativeV)});
            ++cases;
        }
    }
    assert(oldError>.15); // Negative control: old full-target submission fails.
    const Frustum symmetric[2]={{-1,1,-1,1},{-1,1,-1,1}};
    Plan simple;assert(Build(symmetric,simple));Near(simple.horizontalFov,90);Near(simple.aspect,1);
    for(const auto& b:simple.eyes) {Near(b.uMin,0);Near(b.vMin,0);Near(b.uMax,1);Near(b.vMax,1);}
    const float invalids[]={std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()};
    for(float invalid:invalids) for(int index=0;index<4;++index) {
        Frustum bad[2]={raw[0],raw[1]};
        if(index==0) bad[1].left=invalid;if(index==1) bad[1].right=invalid;
        if(index==2) bad[1].top=invalid;if(index==3) bad[1].bottom=invalid;
        Plan untouched;untouched.aspect=123;
        assert(!Build(bad,untouched));assert(untouched.aspect==123);
    }
    Frustum collapsed[2]={{1,1,-1,1},raw[1]};assert(!Build(collapsed,simple));
    collapsed[0]={-1,1,0,0};assert(!Build(collapsed,simple));
    printf("PASS: %u asymmetric projection cases under head rotation/lean; old full-bounds error=%f\n",cases,oldError);
    for(int i=0;i<2;++i) {const auto& b=plan.eyes[i];printf("Eye %d bounds %.9f %.9f %.9f %.9f\n",i,b.uMin,b.vMin,b.uMax,b.vMax);}
}
