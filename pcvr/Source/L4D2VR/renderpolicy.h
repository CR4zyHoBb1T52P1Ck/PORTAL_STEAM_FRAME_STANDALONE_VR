#pragma once
#include <cstring>
#include <algorithm>
#include <cmath>
namespace RenderPolicy {
inline bool AvatarHands(bool nativeModels,bool body,bool ik,bool firstPerson,bool sourceReady,bool eligible){
    return nativeModels&&body&&ik&&firstPerson&&sourceReady&&eligible;
}
// Manual profiling keeps the original cadence. Automatic evidence is sparse
// and follows the logging control; no settings, visual quality or pacing changes.
inline unsigned GpuCadence(bool manual,bool automatic,bool logging,unsigned secondary){
    return manual?30:automatic&&logging?(secondary>=3?60:120):0;
}
// Successful upscaling already sampled/resolved both inputs. Only native
// submission (including a failed upscale) needs the explicit eye resolves.
inline bool ResolveNativeEyes(bool created,bool newFrame,bool upscaleReady,int mode){
    return created&&newFrame&&!(upscaleReady&&mode!=0);
}
// Retail SetupMain3DView redirects HDR to a desktop-sized FullFrameFB.
inline bool UseEyeTarget(bool legacyEffects,const char* nativeTarget) {
    return !nativeTarget || (!legacyEffects&&std::strncmp(nativeTarget,"_rt_FullFrameFB",15)==0);
}
template<class View> inline void PrepareEye(View& view,bool legacyEffects) {
    view.m_bDoBloomAndToneMapping=legacyEffects;
}
// World-space tracked skin must use the world's projection depth, as well as
// its viewport range. A viewmodel near plane of 2 against a scene near plane
// of 1 otherwise makes the gun win depth tests through hands and torso.
template<class View> inline void ViewmodelClip(View& view,bool sceneDepth) {
    if(sceneDepth){view.zNearViewmodel=view.zNear;view.zFarViewmodel=view.zFar;}
}
template<class View> inline void ViewmodelEyeClip(View& eye,const View& native,bool sceneDepth) {
    if(!sceneDepth){eye.zNear=native.zNear;eye.zFar=native.zFar;}
}
inline unsigned Visible(unsigned dimension,float low,float high) {
    return unsigned(std::lround(dimension*std::clamp(high-low,0.f,1.f)));
}
}
