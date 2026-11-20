#include <Windows.h>
#include <cassert>
#include <fstream>
#include <vector>
#include "nativefingers.h"
#include "nativefit.h"
#include "portalpose.h"
static void Near(float a,float b,float e=.002f){assert(std::abs(a-b)<e);}
static Vector Position(const matrix3x4_t& m){return {m[0][3],m[1][3],m[2][3]};}
int main(int argc,char** argv) {
    assert(argc==3);unsigned cases=0;
    for(int file=1;file<=2;++file) {
        std::ifstream f(argv[file],std::ios::binary);std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)),{});assert(d.size()>248);
        int count,offset;memcpy(&count,d.data()+156,4);memcpy(&offset,d.data()+160,4);assert(count==(file==1?43:45));
        matrix3x4_t bind[45];for(int i=0;i<count;++i){matrix3x4_t inv;memcpy(&inv,d.data()+offset+i*216+96,48);bind[i]=HandPose::InverseRigid(inv);}
        if(file==1)for(bool left:{false,true}) {
            matrix3x4_t open[45],closed[45];for(int i=0;i<count;++i)open[i]=closed[i]=bind[i];
            float relaxed[5]{},index[5]={0,1,0,0,0};
            NativeFingers::TrackedHand(bind,open,relaxed,left,true);NativeFingers::TrackedHand(bind,closed,index,left,true);
            const int wrist=left?8:27,tip=left?20:39;
            assert(!memcmp(&open[wrist],&bind[wrist],48));assert(!memcmp(&closed[wrist],&bind[wrist],48));
            assert((Position(open[tip])-Position(closed[tip])).LengthSqr()>.1f);
            for(int i=0;i<count;++i)if(i<wrist+1||i>wrist+15)assert(!memcmp(&closed[i],&bind[i],48));
        }
        for(float amount:{0.f,.25f,.5f,.75f,1.f})for(int active=0;active<5;++active)for(float yaw:{-120.f,0.f,120.f}) {
            const auto frame=PortalPose::Frame({10,20,30},{25,yaw,37});matrix3x4_t actual[45],original[45];
            for(int i=0;i<count;++i)actual[i]=original[i]=HandPose::Concat(frame,bind[i]);
            float curl[5]{};curl[active]=amount;
            if(file==1)NativeFingers::Hands(bind,actual,curl,curl);else NativeFingers::Chain(bind,actual,curl,0,8);
            for(int i=0;i<count;++i)for(int r=0;r<3;++r)for(int c=0;c<4;++c)assert(std::isfinite(actual[i][r][c]));
            for(int wrist: file==1?std::vector<int>{8,27}:std::vector<int>{8}) {
                assert(!memcmp(&actual[wrist],&original[wrist],48));
                for(int i=wrist+1;i<wrist+16;++i) {
                    const int parent=(i-wrist-1)%3==0?wrist:i-1;
                    Near(sqrtf((Position(actual[i])-Position(actual[parent])).LengthSqr()),sqrtf((Position(bind[i])-Position(bind[parent])).LengthSqr()));
                    if((i-wrist-1)/3!=4-active)for(int r=0;r<3;++r)for(int c=0;c<4;++c)Near(actual[i][r][c],original[i][r][c]);
                }
                if(amount==1&&active!=0) {
                    const int tip=wrist+3*(5-active);
                    const auto inv=HandPose::InverseRigid(actual[wrist]);
                    const auto local=HandPose::Concat(inv,actual[tip]);const auto rest=HandPose::Concat(HandPose::InverseRigid(bind[wrist]),bind[tip]);
                    assert(local[1][3]<rest[1][3]-.2f); // bend into palm, not towards thumb
                }
            }
            if(file==2)for(int i=24;i<45;++i)assert(!memcmp(&actual[i],&original[i],48));
            ++cases;
        }
    }
    assert(NativeFingers::Amount(NAN)==0&&NativeFingers::Amount(-1)==0&&NativeFingers::Amount(2)==1);
    printf("PASS: %u native finger poses; palm direction, isolated fingers, rigid wrists, chain lengths untouched gun animation and live index curl in either gripping hand\n",cases);
}
