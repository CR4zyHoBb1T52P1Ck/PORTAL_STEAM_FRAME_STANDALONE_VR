#include <Windows.h>
#include <cassert>
#include <fstream>
#include <vector>
#include <cstdio>
#include "avatarpalette.h"
#include "weaponhandling.h"
#include "nativefit.h"
#include "portalpose.h"
static void Near(float a,float b,float e=.006f){assert(std::abs(a-b)<e);}
static void Near(Vector a,Vector b,float e=.01f){assert((a-b).LengthSqr()<e*e);}
static void Near(const matrix3x4_t& a,const matrix3x4_t& b){for(int r=0;r<3;++r)for(int c=0;c<4;++c)Near(a[r][c],b[r][c]);}
static std::vector<unsigned char> Read(const char* path){std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
static Vector Transform(const matrix3x4_t& m,const Vector& p){return TwoHandAim::Position(HandPose::Concat(m,PortalPose::Frame(p,{0,0,0})));}
static int Integer(const std::vector<unsigned char>& d,int at){int n;assert(at>=0&&at+4<=int(d.size()));std::memcpy(&n,d.data()+at,4);return n;}
struct Vertex {float weights[3];unsigned char bones[3],count;Vector position,normal;float uv[2];};
static_assert(sizeof(Vertex)==48);
static Vector Skin(const BodyIK::Rig& rig,const matrix3x4_t* pose,const Vertex& vertex) {
    Vector result(0,0,0);
    for(int i=0;i<vertex.count;++i) {
        const int bone=vertex.bones[i];assert(bone<rig.count);
        const auto skin=HandPose::Concat(pose[bone],HandPose::InverseRigid(rig.bind[bone]));
        result+=Transform(skin,vertex.position)*vertex.weights[i];
    }
    return result;
}
int main(int argc,char** argv) {
    assert(argc==3||argc==4);const auto data=Read(argv[1]),vvd=Read(argv[2]);BodyIK::Rig rig;assert(rig.Read(data.data(),data.size()));
    assert(Integer(vvd,4)==4&&Integer(vvd,8)==Integer(data,8)&&Integer(vvd,12)==1&&Integer(vvd,48)==0);
    const int n= Integer(vvd,16),start=Integer(vvd,56);assert(n>20000&&size_t(start)+size_t(n)*48<=vvd.size());
    std::vector<Vertex> vertices(n);std::memcpy(vertices.data(),vvd.data()+start,n*48);
    const auto model=HandPose::Frame({1,0,0},{0,0,1},{0,-1,0},{100,200,0});
    matrix3x4_t base[128],posed[128],moved[128],clone[128];for(int i=0;i<rig.count;++i)base[i]=HandPose::Concat(model,rig.bind[i]);
    unsigned poses=0,skins=0;
    for(float pitch:{-70.f,0.f,65.f})for(float yaw:{-170.f,0.f,95.f})for(float roll:{-165.f,0.f,160.f})for(float crouch:{0.f,-12.f}) {
        matrix3x4_t hands[2];
        for(int h=0;h<2;++h) {
            Vector f,r,u;QAngle::AngleVectors({pitch,yaw,roll},&f,&r,&u);
            const Vector point=TwoHandAim::Position(base[rig.arm[h][2]])+Vector(h?-9.f:9.f,-7,3+crouch);
            hands[h]=NativeFit::Arm(f,r,u,point,h==0,0,0,0);
        }
        const float curlL[5]={.2f,.3f,.4f,.5f,.6f},curlR[5]={.6f,.5f,.4f,.3f,.2f};const float* curls[2]={curlL,curlR};const bool gripping[2]={false,false};
        auto head=TwoHandAim::Position(base[rig.head]);head.z+=crouch;
        const auto headPose=PortalPose::Frame(rig.Eyes(base)+Vector(0,0,crouch),{pitch*.15f,-90+yaw*.1f,roll*.15f});
        std::memcpy(posed,base,rig.count*48);assert(rig.SolveNative(posed,head,hands,{0,-1,0},1.1f,1,true,curls,gripping,&headPose));
        Near(HandPose::Concat(posed[rig.head],rig.eyeLocal),headPose);
        if(argc==4&&pitch==0&&yaw==0&&roll==0&&crouch==0) {
            std::ofstream output(argv[3],std::ios::binary);assert(output);
            output.write(reinterpret_cast<const char*>(&rig.count),sizeof(rig.count));
            output.write(reinterpret_cast<const char*>(posed),rig.count*48);
            output.write(reinterpret_cast<const char*>(hands),2*48);
        }
        for(int h=0;h<2;++h) {
            Near(TwoHandAim::Position(posed[rig.arm[h][2]]),TwoHandAim::Position(hands[h]));
            Near(posed[rig.arm[h][2]],rig.WristTarget(hands[h],h));
            Near(posed[rig.leg[h][2]],base[rig.leg[h][2]]);
        }
        for(int i=0;i<rig.count;++i) {
            for(int a=0;a<3;++a)for(int b=0;b<3;++b)Near(TwoHandAim::Dot(TwoHandAim::Axis(posed[i],a),TwoHandAim::Axis(posed[i],b)),a==b?1.f:0.f,.01f);
        }
        // Move only the left controller. Every weighted native hand vertex must
        // follow rigidly, with no duplicate VM, chopped wrist, or right-hand drift.
        const Vector delta(2,3,-1);for(int r=0;r<3;++r)hands[0][r][3]+=delta[r];
        std::memcpy(moved,base,rig.count*48);assert(rig.SolveNative(moved,head,hands,{0,-1,0},1.1f,1,true,curls,gripping,&headPose));
        unsigned left=0,right=0;
        for(const auto& vertex:vertices) {
            assert(vertex.count>=1&&vertex.count<=3);
            bool leftOnly=true,rightOnly=true;
            for(int j=0;j<vertex.count;++j)if(vertex.weights[j]>.00001f) {
                leftOnly&=rig.below[rig.arm[0][2]][vertex.bones[j]];
                rightOnly&=rig.below[rig.arm[1][2]][vertex.bones[j]];
            }
            const auto before=Skin(rig,posed,vertex),after=Skin(rig,moved,vertex);
            assert(TwoHandAim::Finite(before)&&TwoHandAim::Finite(after));
            if(leftOnly){Near(after-before,delta,.025f);++left;}
            if(rightOnly){Near(after,before,.025f);++right;}
            ++skins;
        }
        assert(left>300&&right>300);
        AvatarPalette::Snapshot snapshot;snapshot.Store(25,data.data(),base[0],moved,rig.count);assert(snapshot.Current(25,data.data())&&!snapshot.Current(26,data.data()));
        const auto link=PortalPose::Frame({-300,50,75},{pitch,yaw,roll});
        snapshot.Copy(clone,HandPose::Concat(link,base[0]),false);
        for(int i=0;i<rig.count;++i)Near(clone[i],HandPose::Concat(link,moved[i]));
        snapshot.Copy(clone,base[0],true);for(int i=0;i<rig.count;++i)Near(clone[i],moved[i]);++poses;
    }
    // Receiver identity and support lifecycle must match a shoulder draw.
    for(int receiving:{0,1})for(int repeat=0;repeat<1000;++repeat) {
        WeaponHandling::State transfer;transfer.hand=1-receiving;
        const bool tracked[2]={true,true},away[2]={false,false};bool grip[2]={true,true};
        assert(transfer.Update(grip,tracked,away,true,true,true)==WeaponHandling::None);
        grip[1-receiving]=false;
        assert(transfer.Update(grip,tracked,away,true,true,true)==WeaponHandling::Transfer&&transfer.hand==receiving&&!transfer.transferSupport);
        WeaponHandling::State drawn;drawn.stowed=true;bool shoulder[2]{};shoulder[receiving]=true;
        assert(drawn.Update(grip,tracked,shoulder,false,true,true)==WeaponHandling::Draw&&drawn.hand==transfer.hand&&!drawn.transferSupport);
    }
    printf("PASS: %u full native Chell poses; %u weighted mesh-vertex checks including independent left-hand translation; intact wrist/finger matrices and ankle attitude; shared portal/direct palettes; 2000 transfer/draw receiver equivalence cases\n",poses,skins);
}
