#pragma once
#include "firstpersonbody.h"
#include "twohandaim.h"
#include "fingerrange.h"
#include "fingersurface.h"
#include "gunhandcontact.h"
#include "posetwist.h"
#include "forearmroll54.h"
#include <cstring>
#include <string>
namespace BodyIK {
constexpr int Max=FirstPersonBody::MaxBones;
inline matrix3x4_t Rotation(Vector from,Vector to) {
    const auto identity=HandPose::Frame({1,0,0},{0,1,0},{0,0,1},{0,0,0});
    if(!TwoHandAim::Unit(from)||!TwoHandAim::Unit(to))return identity;
    const float dot=std::clamp(TwoHandAim::Dot(from,to),-1.f,1.f);
    auto axis=CrossProduct(from,to);
    if(!TwoHandAim::Unit(axis)) {
        if(dot>0)return identity;
        axis=CrossProduct(from,std::abs(from.z)<.8f?Vector(0,0,1):Vector(0,1,0));
        if(!TwoHandAim::Unit(axis))return identity;
    }
    return HandPose::RotateAroundAxis(axis,acosf(dot));
}
struct Joint {Vector elbow,end;bool valid=false;};
inline Joint TwoBone(const Vector& root,const Vector& target,float upper,float lower,Vector pole) {
    Joint out;
    if(!TwoHandAim::Finite(root)||!TwoHandAim::Finite(target)||!TwoHandAim::Finite(pole)||!std::isfinite(upper)||!std::isfinite(lower)||upper<.01f||lower<.01f)return out;
    auto direction=target-root;const float requested=sqrtf(direction.LengthSqr());
    if(!TwoHandAim::Unit(direction))direction={1,0,0};
    const float distance=std::clamp(requested,std::abs(upper-lower)+.001f,upper+lower-.001f);
    pole-=direction*TwoHandAim::Dot(pole,direction);
    if(!TwoHandAim::Unit(pole)) {
        pole=CrossProduct(direction,std::abs(direction.z)<.8f?Vector(0,0,1):Vector(0,1,0));
        if(!TwoHandAim::Unit(pole))return out;
    }
    const float along=(upper*upper-lower*lower+distance*distance)/(2*distance);
    const float height=sqrtf(std::max(upper*upper-along*along,0.f));
    out.elbow=root+direction*along+pole*height;out.end=root+direction*distance;
    out.valid=TwoHandAim::Finite(out.elbow)&&TwoHandAim::Finite(out.end);return out;
}
struct Rig {
    int count=0,parents[Max]{},hips=-1,spine=-1,chest=-1,neck=-1,head=-1;
    int arm[2][3]={{-1,-1,-1},{-1,-1,-1}},leg[2][3]={{-1,-1,-1},{-1,-1,-1}};
    int eyeBone=-1;matrix3x4_t eyeLocal{};
    matrix3x4_t bind[Max]{},wristFromSemantic[2]{};
    Vector palm[2]{},gripLocal[2]{};matrix3x4_t bodyBindFrame{};int wristTwist[2]={-1,-1};
    matrix3x4_t forearmWristFromElbow[2]{},forearmHelperFromElbow[2]{};float forearmStation[2]={.5f,.5f};
    int fingers[2][5][3]{};
    matrix3x4_t fingerLocal[2][5][3]{};Vector fingerAxis[2][5][3]{};
    bool below[Max][Max]{};
    bool Read(const unsigned char* hdr,size_t size) {
        *this={};if(!hdr||size<248)return false;
        auto integer=[&](size_t at){int value;std::memcpy(&value,hdr+at,4);return value;};
        count=integer(156);const int offset=integer(160);
        if(count<1||count>Max||offset<164||size_t(offset)>size||size_t(count)>(size-size_t(offset))/216)return false;
        const char* names[Max]{};
        for(int i=0;i<count;++i) {
            const size_t at=size_t(offset)+i*216;const int name=integer(at);parents[i]=integer(at+4);
            if(parents[i]>=i||parents[i]<-1||name<=0||size_t(name)>=size-at)return false;
            names[i]=reinterpret_cast<const char*>(hdr+at+name);
            if(!std::memchr(names[i],0,size-at-name))return false;
            matrix3x4_t inverse;std::memcpy(&inverse,hdr+at+96,sizeof(inverse));bind[i]=HandPose::InverseRigid(inverse);
        }
        auto find=[&](const char* wanted){for(int i=0;i<count;++i)if(!std::strcmp(names[i],wanted))return i;return -1;};
        hips=find("hips");spine=find("spine_base");chest=find("chest");neck=find("neck");head=find("head");
        for(int h=0;h<2;++h) {
            arm[h][0]=find(h?"bicep_R":"bicep_L");arm[h][1]=find(h?"elbow_R":"elbow_L");arm[h][2]=find(h?"wrist_R":"wrist_L");
            wristTwist[h]=find(h?"wrist_twist_R":"wrist_twist_L");
            leg[h][0]=find(h?"thigh_R":"thigh_L");leg[h][1]=find(h?"knee_R":"knee_L");leg[h][2]=find(h?"ankle_R":"ankle_L");
            for(int j=0;j<3;++j)if(arm[h][j]<0||leg[h][j]<0)return false;
            if(wristTwist[h]>=0){
                const auto inverse=HandPose::InverseRigid(bind[arm[h][1]]);
                forearmWristFromElbow[h]=HandPose::Concat(inverse,bind[arm[h][2]]);
                forearmHelperFromElbow[h]=HandPose::Concat(inverse,bind[wristTwist[h]]);
                const auto axis=TwoHandAim::Position(bind[arm[h][2]])-TwoHandAim::Position(bind[arm[h][1]]);
                forearmStation[h]=axis.LengthSqr()>1e-6f?std::clamp(TwoHandAim::Dot(TwoHandAim::Position(bind[wristTwist[h]])-TwoHandAim::Position(bind[arm[h][1]]),axis)/axis.LengthSqr(),0.f,1.f):.5f;
            }
        }
        if(hips<0||spine<0||chest<0||neck<0||head<0)return false;
        for(int h=0;h<2;++h) {
            const char* digit[]={"thumb","index","mid","ring","pinky"};
            const char* segment[]={"base","mid","end"};
            for(int f=0;f<5;++f)for(int s=0;s<3;++s) {
                const auto name=std::string(digit[f])+"_"+segment[s]+(h?"_R":"_L");
                fingers[h][f][s]=find(name.c_str());if(fingers[h][f][s]<0)return false;
            }
            const auto wrist=TwoHandAim::Position(bind[arm[h][2]]);
            auto forward=TwoHandAim::Position(bind[fingers[h][2][0]])-wrist;
            auto thumb=TwoHandAim::Position(bind[fingers[h][0][0]])-wrist;
            if(!TwoHandAim::Unit(forward))return false;
            thumb-=forward*TwoHandAim::Dot(thumb,forward);if(!TwoHandAim::Unit(thumb))return false;
            auto transverse=CrossProduct(thumb,forward);if(!TwoHandAim::Unit(transverse))return false;
            const auto semantic=HandPose::Frame(forward,transverse,thumb,wrist);
            wristFromSemantic[h]=HandPose::Concat(HandPose::InverseRigid(semantic),bind[arm[h][2]]);
            palm[h]=transverse*(h?1.f:-1.f);
            const auto knuckles=(TwoHandAim::Position(bind[fingers[h][1][0]])+TwoHandAim::Position(bind[fingers[h][2][0]])+TwoHandAim::Position(bind[fingers[h][3][0]]))*(1.f/3);
            const auto grip=wrist+(knuckles-wrist)*.55f;
            const auto local=HandPose::Concat(HandPose::InverseRigid(bind[arm[h][2]]),HandPose::Frame({1,0,0},{0,1,0},{0,0,1},grip));
            gripLocal[h]=TwoHandAim::Position(local);
            for(int f=0;f<5;++f) {
                int parent=arm[h][2];
                for(int s=0;s<3;++s) {
                    const int bone=fingers[h][f][s],from=s<2?bone:parent,to=s<2?fingers[h][f][s+1]:bone;
                    const auto direction=TwoHandAim::Position(bind[to])-TwoHandAim::Position(bind[from]);
                    Vector toward=palm[h];
                    if(f==0){toward=TwoHandAim::Position(bind[fingers[h][2][0]])-TwoHandAim::Position(bind[bone]);TwoHandAim::Unit(toward);toward+=palm[h]*.5f;}
                    const auto axis=CrossProduct(direction,toward);
                    fingerAxis[h][f][s]={TwoHandAim::Dot(TwoHandAim::Axis(bind[parent],0),axis),TwoHandAim::Dot(TwoHandAim::Axis(bind[parent],1),axis),TwoHandAim::Dot(TwoHandAim::Axis(bind[parent],2),axis)};
                    fingerLocal[h][f][s]=HandPose::Concat(HandPose::InverseRigid(bind[parent]),bind[bone]);
                    parent=bone;
                }
            }
        }
        const int attachments=integer(240),start=integer(244);
        if(attachments>0&&attachments<=256&&start>=248&&size_t(start)<=size&&size_t(attachments)<=(size-size_t(start))/92) {
            for(int i=0;i<attachments;++i) {
                const size_t at=size_t(start)+i*92;const int name=integer(at),bone=integer(at+8);
                if(name>0&&size_t(name)<size-at&&size-at-name>=5&&!std::memcmp(hdr+at+name,"eyes",5)&&bone>=0&&bone<count) {
                    eyeBone=bone;std::memcpy(&eyeLocal,hdr+at+12,sizeof(eyeLocal));break;
                }
            }
        }
        auto vertical=Eyes(bind)-(TwoHandAim::Position(bind[leg[0][2]])+TwoHandAim::Position(bind[leg[1][2]]))*.5f;
        auto side=TwoHandAim::Position(bind[arm[1][0]])-TwoHandAim::Position(bind[arm[0][0]]);
        if(!TwoHandAim::Unit(vertical)||!TwoHandAim::Unit(side))return false;
        auto front=CrossProduct(vertical,side);if(!TwoHandAim::Unit(front))return false;side=CrossProduct(front,vertical);
        bodyBindFrame=HandPose::Frame(front,-side,vertical,TwoHandAim::Position(bind[hips]));
        for(int root=0;root<count;++root)for(int i=0;i<count;++i)below[root][i]=FirstPersonBody::IsDescendant(parents,count,i,root);
        return true;
    }
    Vector Eyes(const matrix3x4_t* bones) const {
        return eyeBone>=0?TwoHandAim::Position(HandPose::Concat(bones[eyeBone],eyeLocal)):TwoHandAim::Position(bones[head])+Vector(0,0,4);
    }
    void Shift(matrix3x4_t* bones,int root,const Vector& delta) const {
        for(int i=0;i<count;++i)if(below[root][i])for(int r=0;r<3;++r)bones[i][r][3]+=delta[r];
    }
    void RotateBranch(matrix3x4_t* bones,int root,const Vector& target,int end) const {
        const auto pivot=TwoHandAim::Position(bones[root]);
        auto rotation=Rotation(TwoHandAim::Position(bones[end])-pivot,target-pivot);
        const auto p=TwoHandAim::Position(HandPose::Concat(rotation,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},pivot)));
        for(int r=0;r<3;++r)rotation[r][3]=pivot[r]-p[r];
        for(int i=0;i<count;++i)if(below[root][i])bones[i]=HandPose::Concat(rotation,bones[i]);
    }
    bool Limb(matrix3x4_t* bones,const int chain[3],const Vector& target,const Vector& pole,float scale,bool autoReach=false) const {
        const int root=chain[0],mid=chain[1],end=chain[2];
        if(root<0||mid<0||end<0||!TwoHandAim::Finite(target)||!std::isfinite(scale)||scale<.5f||scale>3)return false;
        const auto pivot=TwoHandAim::Position(bones[root]);
        const auto oldEnd=TwoHandAim::Position(bones[end]);
        if(autoReach) {
            const float length=sqrtf((TwoHandAim::Position(bones[mid])-pivot).LengthSqr())+sqrtf((oldEnd-TwoHandAim::Position(bones[mid])).LengthSqr());
            if(length>.01f)scale=std::clamp(std::max(scale,sqrtf((target-pivot).LengthSqr())/length*1.0001f),.5f,3.f);
        }
        // Change arm reach without spreading the fingers or deforming the hand.
        // Hand and finger translations move as one rigid branch.
        for(int i=0;i<count;++i)if(below[root][i])for(int r=0;r<3;++r)
            bones[i][r][3]=below[end][i]?bones[i][r][3]+(oldEnd[r]-pivot[r])*(scale-1):pivot[r]+(bones[i][r][3]-pivot[r])*scale;
        const float upper=sqrtf((TwoHandAim::Position(bones[mid])-pivot).LengthSqr());
        const float lower=sqrtf((TwoHandAim::Position(bones[end])-TwoHandAim::Position(bones[mid])).LengthSqr());
        const auto joint=TwoBone(pivot,target,upper,lower,pole);if(!joint.valid)return false;
        RotateBranch(bones,root,joint.elbow,mid);RotateBranch(bones,mid,joint.end,end);return true;
    }
    bool ArmSegments(matrix3x4_t* bones,const int chain[3],Vector target,Vector pole,float upper,float lower,bool autoReach) const {
        const int root=chain[0],mid=chain[1],end=chain[2];
        if(!bones||root<0||mid<0||end<0||!TwoHandAim::Finite(target)||!std::isfinite(upper)||!std::isfinite(lower)||upper<.01f||lower<.01f)return false;
        const auto shoulder=TwoHandAim::Position(bones[root]),elbow=TwoHandAim::Position(bones[mid]),wrist=TwoHandAim::Position(bones[end]);
        const float oldUpper=sqrtf((elbow-shoulder).LengthSqr()),oldLower=sqrtf((wrist-elbow).LengthSqr());
        if(oldUpper<.01f||oldLower<.01f)return false;
        const float distance=sqrtf((target-shoulder).LengthSqr());if(!std::isfinite(distance))return false;
        if(autoReach){const float d=std::max(distance,.003f);upper=std::clamp(upper,std::abs(d-lower)+.001f,d+lower-.001f);}
        const auto newElbow=shoulder+(elbow-shoulder)*(upper/oldUpper),newWrist=newElbow+(wrist-elbow)*(lower/oldLower);
        // Stretch only segment positions; the complete hand remains a rigid branch.
        for(int i=0;i<count;++i)if(below[root][i]){
            const auto p=TwoHandAim::Position(bones[i]);const auto moved=below[end][i]?p+(newWrist-wrist):below[mid][i]?newElbow+(p-elbow)*(lower/oldLower):shoulder+(p-shoulder)*(upper/oldUpper);
            for(int r=0;r<3;++r)bones[i][r][3]=moved[r];
        }
        const auto joint=TwoBone(shoulder,target,upper,lower,pole);if(!joint.valid)return false;
        RotateBranch(bones,root,joint.elbow,mid);RotateBranch(bones,mid,joint.end,end);return true;
    }
    void ArmSkinLengths(matrix3x4_t* bones,int h)const {
        const int root=arm[h][0],mid=arm[h][1],end=arm[h][2];
        Vector axes[2]={TwoHandAim::Position(bones[mid])-TwoHandAim::Position(bones[root]),TwoHandAim::Position(bones[end])-TwoHandAim::Position(bones[mid])};
        const float original[2]={sqrtf((TwoHandAim::Position(bind[mid])-TwoHandAim::Position(bind[root])).LengthSqr()),sqrtf((TwoHandAim::Position(bind[end])-TwoHandAim::Position(bind[mid])).LengthSqr())};
        float scales[2];for(int s=0;s<2;++s){scales[s]=sqrtf(axes[s].LengthSqr())/original[s];if(!std::isfinite(scales[s])||!TwoHandAim::Unit(axes[s]))return;}
        for(int i=0;i<count;++i)if(below[root][i]&&!below[end][i]){const int s=below[mid][i]?1:0;for(int c=0;c<3;++c){const auto v=TwoHandAim::Axis(bones[i],c),next=v+axes[s]*(TwoHandAim::Dot(v,axes[s])*(scales[s]-1));for(int r=0;r<3;++r)bones[i][r][c]=next[r];}}
    }
    matrix3x4_t WristTarget(const matrix3x4_t& valveHand,int physical) const {
        const auto forward=TwoHandAim::Axis(valveHand,0),thumb=TwoHandAim::Axis(valveHand,2)*(physical? -1.f:1.f);
        const auto semantic=HandPose::Frame(forward,CrossProduct(thumb,forward),thumb,TwoHandAim::Position(valveHand));
        return HandPose::Concat(semantic,wristFromSemantic[physical]);
    }
    void ReanchorBranch(matrix3x4_t* bones,int root,const matrix3x4_t& target) const {
        const auto delta=HandPose::Concat(target,HandPose::InverseRigid(bones[root]));
        for(int i=0;i<count;++i)if(below[root][i])bones[i]=HandPose::Concat(delta,bones[i]);
    }
    void Curl(matrix3x4_t* bones,int h,const float* values,bool gripping,float gain=1,float gripFloor=.5f,float extension=0) const {
        if(!values)return;
        static const float grip[5]={.35f,.1f,.5f,.5f,.5f};
        for(int f=0;f<5;++f) {
            const float amount=FingerRange::Amount(values[f],gain);
            const float curl=gripping&&f!=0?std::max(amount,gripFloor*(f==1?.25f:1.f)):amount;
            int parent=arm[h][2];
            for(int s=0;s<3;++s) {
                const int bone=fingers[h][f][s];
                bones[bone]=HandPose::Concat(bones[parent],HandPose::RotateJoint(fingerLocal[h][f][s],fingerAxis[h][f][s],FingerRange::Angle(f,s,curl,extension)));
                parent=bone;
            }
        }
    }
    void SurfaceFingers(matrix3x4_t* bones,int h,const float* values,float gain,float floor,float extension,const FingerSurface::Shape& shape,float margin) const {
        if(!values)return;
        for(int f=0;f<5;++f){const float wanted=f?std::max(FingerRange::Amount(values[f],gain),floor*(f==1?.25f:1.f)):FingerRange::Amount(values[f],gain);
            FingerSurface::Clamp(bones,arm[h][2],fingers[h][f],fingerLocal[h][f],fingerAxis[h][f],f,wanted,extension,shape,margin);}
    }
    void AvoidGunArm(matrix3x4_t* bones,int h,const FingerSurface::Shape& shape,float units,bool torso=false) const {
        const int root=arm[h][0],mid=arm[h][1],end=arm[h][2];
        if(root<0||mid<0||end<0||!std::isfinite(units)||units<1)return;
        const auto shoulder=TwoHandAim::Position(bones[root]),elbow=TwoHandAim::Position(bones[mid]),wrist=TwoHandAim::Position(bones[end]);
        const auto hand=bones[end];const float radius=units*.023f;
        auto penalty=[&](Vector joint){
            // The upper-arm root is naturally inside its shoulder. Do not
            // penalize that attachment when clearing the torso itself.
            const auto start=torso?shoulder+(joint-shoulder)*.35f:shoulder;
            return std::max(GunHandContact::SegmentPenalty(start,joint,shape,radius),GunHandContact::SegmentPenalty(joint,wrist,shape,radius));};
        float best=penalty(elbow);if(best<.001f)return;
        auto axis=wrist-shoulder;if(!TwoHandAim::Unit(axis))return;
        const float upper=sqrtf((elbow-shoulder).LengthSqr()),lower=sqrtf((wrist-elbow).LengthSqr());
        Vector selected=elbow;
        // Choose the smallest elbow-plane change that clears the gun. Bone
        // lengths, shoulder and wrist targets remain fixed; visual bones only.
        for(int step=1;step<=12&&best>.001f;++step)for(float sign:{-1.f,1.f}){
            const auto rotation=HandPose::RotateAroundAxis(axis,sign*step*.261799388f);
            const auto pole=TwoHandAim::Position(HandPose::Concat(rotation,HandPose::Frame({1,0,0},{0,1,0},{0,0,1},elbow-shoulder)));
            const auto candidate=TwoBone(shoulder,wrist,upper,lower,pole);if(!candidate.valid)continue;
            const float score=penalty(candidate.elbow);if(score<best-.001f){best=score;selected=candidate.elbow;}
        }
        if((selected-elbow).LengthSqr()<.000001f)return;
        RotateBranch(bones,root,selected,mid);RotateBranch(bones,mid,wrist,end);ReanchorBranch(bones,end,hand);
    }
    void FitLegs(matrix3x4_t* bones,const matrix3x4_t feet[2],Vector forward,float physicalCrouch,float unit,bool stable) const {
        for(int h=0;h<2;++h) {
            const auto hip=TwoHandAim::Position(bones[leg[h][0]]);
            const auto knee=TwoHandAim::Position(bones[leg[h][1]]);
            const float upper=sqrtf((knee-hip).LengthSqr());
            const float lower=sqrtf((TwoHandAim::Position(bones[leg[h][2]])-knee).LengthSqr());
            auto target=TwoHandAim::Position(feet[h]);
            if(stable) {
                // Source's jump animation tucks its ankles up toward the pelvis.
                // Those animated ankles are not tracked feet. Keep them below
                // the fitted hips; real physical crouching may bend further.
                const float crouch=std::clamp(-physicalCrouch*unit,0.f,upper*.75f);
                const float standing=std::max((upper+lower)*.82f,lower+unit*.5f);
                const float drop=std::max(standing-crouch,(upper+lower)*.15f);
                target.z=std::min(target.z,hip.z-drop);
            }
            Limb(bones,leg[h],target,forward,1);
            auto ankle=feet[h];
            if(stable) {
                // TwoBone clamps unreachable targets. Restoring the OLD ankle
                // position afterwards tears the shin/foot skin apart. Restore
                // only its attitude and keep the connected solved endpoint.
                const auto endpoint=TwoHandAim::Position(bones[leg[h][2]]);
                for(int r=0;r<3;++r)ankle[r][3]=endpoint[r];
            }
            ReanchorBranch(bones,leg[h][2],ankle);
        }
    }
    float FitUpperBody(matrix3x4_t* bones,const Vector& headTarget,Vector forward,float unitsPerM,
        float spanCM,float widthCM,float dropCM,float backCM,float yaw,float reach,float crouchCM=0,bool upright=true,bool palmAnchor=false,float handScale=1,float palmDepth=1,bool rigid=false,float torsoScale=1,float clearanceCM=0) const {
        if(!bones||!TwoHandAim::Finite(headTarget)||!std::isfinite(unitsPerM)||unitsPerM<1)return reach;
        forward.z=0;if(!TwoHandAim::Unit(forward))forward={1,0,0};
        const float turn=yaw*.01745329252f;
        forward={forward.x*cosf(turn)-forward.y*sinf(turn),forward.x*sinf(turn)+forward.y*cosf(turn),0};
        const auto right=CrossProduct(forward,{0,0,1});const float unit=unitsPerM/100;
        const matrix3x4_t feet[2]={bones[leg[0][2]],bones[leg[1][2]]};
        const auto center=headTarget-forward*((backCM+clearanceCM)*unit)-Vector(0,0,dropCM*unit);
        if(rigid) {
            // One rigid bind-space torso, including the hips. No independent
            // pelvis/chest translations and no animated eye-height feedback.
            // A saved uniform proportion is constant for standing/crouch/jump.
            torsoScale=std::clamp(torsoScale,.8f,1.2f);
            const auto shoulder=(TwoHandAim::Position(bind[arm[0][0]])+TwoHandAim::Position(bind[arm[1][0]]))*.5f;
            const auto neutral=HandPose::Concat(HandPose::Frame(forward,-right,{0,0,1},center),
                HandPose::InverseRigid(HandPose::Frame(TwoHandAim::Axis(bodyBindFrame,0),TwoHandAim::Axis(bodyBindFrame,1),TwoHandAim::Axis(bodyBindFrame,2),shoulder)));
            for(int i=0;i<count;++i) {
                auto source=bind[i];const auto delta=(TwoHandAim::Position(source)-shoulder)*(torsoScale-1);
                for(int r=0;r<3;++r)source[r][3]+=delta[r];
                bones[i]=HandPose::Concat(neutral,source);
            }
        } else if(upright) {
            const auto neutral=HandPose::Concat(HandPose::Frame(forward,-right,{0,0,1},TwoHandAim::Position(bones[hips])),HandPose::InverseRigid(bodyBindFrame));
            for(int i=0;i<count;++i)if(below[spine][i])bones[i]=HandPose::Concat(neutral,bind[i]);
        }
        if(!rigid){const float crouch=std::clamp(crouchCM*unit,-30.f,0.f);Shift(bones,hips,{0,0,crouch*.65f});}
        FitLegs(bones,feet,forward,crouchCM,unit,rigid);
        const auto original=(TwoHandAim::Position(bones[arm[0][0]])+TwoHandAim::Position(bones[arm[1][0]]))*.5f;
        // Move the upper torso and shoulders together; hands are subsequently
        // solved to their exact controllers. Native feet/hips remain inferred.
        if(!rigid)Shift(bones,chest,center-original);
        float length=0;
        for(int h=0;h<2;++h) {
            const auto target=center+right*((h?1.f:-1.f)*widthCM*unit*.5f);
            Shift(bones,arm[h][0],target-TwoHandAim::Position(bones[arm[h][0]]));
            length+=sqrtf((TwoHandAim::Position(bones[arm[h][1]])-target).LengthSqr())+
                sqrtf((TwoHandAim::Position(bones[arm[h][2]])-TwoHandAim::Position(bones[arm[h][1]])).LengthSqr());
        }
        const float palmLength=palmAnchor?(sqrtf(gripLocal[0].LengthSqr())+sqrtf(gripLocal[1].LengthSqr()))*.5f*handScale*palmDepth:0;
        const float desired=std::max((spanCM-widthCM)*unit*.5f-palmLength,unit*18);
        return length>.01f?std::clamp(reach*desired/(length*.5f),.5f,3.f):reach;
    }
    static void Radial(matrix3x4_t& bone,Vector axis,float amount) {
        if(!TwoHandAim::Unit(axis)||!std::isfinite(amount))return;
        amount=std::clamp(amount,.75f,4.f);
        for(int c=0;c<3;++c) {
            const auto v=TwoHandAim::Axis(bone,c);const auto scaled=v*amount+axis*(TwoHandAim::Dot(v,axis)*(1-amount));
            for(int r=0;r<3;++r)bone[r][c]=scaled[r];
        }
    }
    void Volume(matrix3x4_t* bones,float upper,float forearm,float wrist,float hand,float torso,float cuff=1) const {
        if(!bones)return;
        // Apply skin-only affine volume AFTER all rigid IK and finger math.
        // Joint targets stay fixed. A common hand transform keeps the complete
        // fist intact rather than scaling each phalange around its own origin.
        for(int h=0;h<2;++h) {
            const int a=arm[h][0],b=arm[h][1],w=arm[h][2];
            const auto upperAxis=TwoHandAim::Position(bones[b])-TwoHandAim::Position(bones[a]);
            auto lowerAxis=TwoHandAim::Position(bones[w])-TwoHandAim::Position(bones[b]);
            for(int i=0;i<count;++i)if(below[a][i]&&!below[b][i])Radial(bones[i],upperAxis,upper);
            for(int i=0;i<count;++i)if(below[b][i]&&!below[w][i])Radial(bones[i],lowerAxis,forearm);
            if(wristTwist[h]>=0)Radial(bones[wristTwist[h]],lowerAxis,cuff);
            const auto center=TwoHandAim::Position(bones[w]);TwoHandAim::Unit(lowerAxis);
            for(int i=0;i<count;++i)if(below[w][i]) {
                const auto delta=TwoHandAim::Position(bones[i])-center;
                const auto p=center+(delta*wrist+lowerAxis*(TwoHandAim::Dot(delta,lowerAxis)*(1-wrist)))*hand;
                Radial(bones[i],lowerAxis,wrist);
                for(int r=0;r<3;++r){for(int c=0;c<3;++c)bones[i][r][c]*=hand;bones[i][r][3]=p[r];}
            }
        }
        auto vertical=TwoHandAim::Position(bones[neck])-TwoHandAim::Position(bones[hips]);
        for(int i=0;i<count;++i)if(below[spine][i]&&!below[neck][i]&&!below[arm[0][0]][i]&&!below[arm[1][0]][i])Radial(bones[i],vertical,torso);
    }
    void TorsoVolume(matrix3x4_t* bones,float chestAmount,float hipAmount) const {
        if(!bones)return;
        const auto vertical=TwoHandAim::Position(bones[neck])-TwoHandAim::Position(bones[hips]);
        // Skin affine transforms only. Do not move neck, shoulder, wrist,
        // pelvis or leg joints, including the palette reused in portal views.
        Radial(bones[hips],vertical,std::clamp(hipAmount,.75f,2.3f));
        for(int i=0;i<count;++i)if(below[spine][i]&&!below[neck][i]
            &&!below[arm[0][0]][i]&&!below[arm[1][0]][i]) {
            // Clavicles have no torso skin; leave their arm basis untouched.
            if(i==chest||parents[i]==spine)Radial(bones[i],vertical,std::clamp(chestAmount,.75f,2.3f));
        }
    }
    bool SolveNative(matrix3x4_t* bones,const Vector& headTarget,const matrix3x4_t hands[2],Vector forward,float reach,float influence,bool autoReach,const float* curls[2],const bool gripping[2],const matrix3x4_t* headPose=nullptr,float fingerGain=1,float twistFraction=.5f,bool palmAnchor=false,float handScale=1,float wristScale=1,float palmDepth=1,bool preserveTorso=false,const float* gripFloors=nullptr,float thumbExtension=0,bool deferTwist=false,const float* upperScale=nullptr,const float* forearmScale=nullptr) const {
        if(!bones||count<1||!TwoHandAim::Finite(headTarget)||!std::isfinite(influence))return false;
        influence=std::clamp(influence,0.f,1.f);if(influence==0)return true;
        forward.z=0;if(!TwoHandAim::Unit(forward))forward={1,0,0};const auto right=CrossProduct(forward,{0,0,1});
        const matrix3x4_t feet[2]={bones[leg[0][2]],bones[leg[1][2]]};
        const float dz=preserveTorso?0:std::clamp(headTarget.z-TwoHandAim::Position(bones[head]).z,-24.f,4.f)*influence;
        Shift(bones,hips,{0,0,dz*.7f});Shift(bones,spine,{0,0,dz});
        for(int h=0;h<2;++h) {
            if(!preserveTorso){Limb(bones,leg[h],TwoHandAim::Position(feet[h]),forward,1);
            ReanchorBranch(bones,leg[h][2],feet[h]);} // rigid fit already solved legs
            auto desired=WristTarget(hands[h],h);
            const auto gripTarget=TwoHandAim::Position(desired);
            const auto current=TwoHandAim::Position(bones[arm[h][2]]);
            const float upperFactor=upperScale?std::clamp(upperScale[h],.5f,1.5f):1,lowerFactor=forearmScale?std::clamp(forearmScale[h],.5f,1.5f):1;
            if(!std::isfinite(upperFactor)||!std::isfinite(lowerFactor))return false;
            const bool lengths=upperFactor!=1||lowerFactor!=1;
            const float upperLength=sqrtf((TwoHandAim::Position(bones[arm[h][1]])-TwoHandAim::Position(bones[arm[h][0]])).LengthSqr())*reach*upperFactor;
            const float lowerLength=sqrtf((current-TwoHandAim::Position(bones[arm[h][1]])).LengthSqr())*reach*lowerFactor;
            const auto pole=right*(h?1.f:-1.f)*.5f-forward*.3f-Vector(0,0,1),target=current+(TwoHandAim::Position(desired)-current)*influence;
            if(lengths?!ArmSegments(bones,arm[h],target,pole,upperLength,lowerLength,autoReach):!Limb(bones,arm[h],target,pole,reach,autoReach))return false;
            if(palmAnchor)for(int attempt=0;attempt<6;++attempt) {
                auto axis=TwoHandAim::Position(bones[arm[h][2]])-TwoHandAim::Position(bones[arm[h][1]]);if(!TwoHandAim::Unit(axis))break;
                Vector offset;for(int r=0;r<3;++r)offset[r]=(desired[r][0]*gripLocal[h].x+desired[r][1]*gripLocal[h].y+desired[r][2]*gripLocal[h].z)*palmDepth;
                offset=(offset*wristScale+axis*(TwoHandAim::Dot(offset,axis)*(1-wristScale)))*handScale;
                const auto target=gripTarget-offset;
                if((target-TwoHandAim::Position(bones[arm[h][2]])).LengthSqr()<.000001f)break;
                if(lengths?!ArmSegments(bones,arm[h],target,pole,upperLength,lowerLength,autoReach):!Limb(bones,arm[h],target,pole,1,autoReach))return false;
            }
            auto wrist=desired;const auto position=TwoHandAim::Position(bones[arm[h][2]]);
            for(int r=0;r<3;++r)wrist[r][3]=position[r];
            if(!deferTwist&&wristTwist[h]>=0)PoseTwist::Distribute(bones[wristTwist[h]],bones[arm[h][2]],wrist,position-TwoHandAim::Position(bones[arm[h][1]]),twistFraction);
            ReanchorBranch(bones,arm[h][2],wrist);
            Curl(bones,h,curls?curls[h]:nullptr,gripping[h],fingerGain,gripFloors?gripFloors[h]:.5f,thumbExtension);
        }
        if(headPose&&eyeBone==head)ReanchorBranch(bones,head,HandPose::Concat(*headPose,HandPose::InverseRigid(eyeLocal)));
        return true;
    }
    bool Solve(matrix3x4_t* bones,const Vector& headTarget,const Vector hands[2],Vector forward,float reach,float influence) const {
        if(!bones||count<1||!TwoHandAim::Finite(headTarget)||!std::isfinite(influence))return false;
        influence=std::clamp(influence,0.f,1.f);if(influence==0)return true;
        forward.z=0;if(!TwoHandAim::Unit(forward))forward={1,0,0};const auto right=CrossProduct(forward,{0,0,1});
        const Vector feet[2]={TwoHandAim::Position(bones[leg[0][2]]),TwoHandAim::Position(bones[leg[1][2]])};
        // Eyes sit above the head pivot. Retain that authored relationship and
        // use the input head target as a positional delta supplied by the caller.
        const float dz=std::clamp(headTarget.z-TwoHandAim::Position(bones[head]).z,-24.f,4.f)*influence;
        Shift(bones,hips,{0,0,dz*.7f});Shift(bones,spine,{0,0,dz});
        for(int h=0;h<2;++h) {
            Limb(bones,leg[h],feet[h],forward,1);
            const auto current=TwoHandAim::Position(bones[arm[h][2]]);
            const auto target=current+(hands[h]-current)*influence;
            Limb(bones,arm[h],target,right*(h?1.f:-1.f)*.5f-forward*.3f-Vector(0,0,1),reach);
        }
        return true;
    }
};
}
