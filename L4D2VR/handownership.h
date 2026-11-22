#pragma once
namespace HandOwnership {
// Readiness is captured before BOTH eyes. Draw order and asynchronous material
// readiness must not select another copy of the same physical hands.
inline bool Body(bool native,bool enabled,bool ik,bool firstPerson,bool sourceReady){return native&&enabled&&ik&&firstPerson&&sourceReady;}
inline bool HideGunHands(bool native,bool pairedRenderable,bool body){return body||(native&&pairedRenderable);}
inline bool TrackingReady(bool fresh,bool primaryTracked,bool bonesReady,float forwardLengthSquared){return fresh&&primaryTracked&&bonesReady&&forwardLengthSquared>.9f;}
}
