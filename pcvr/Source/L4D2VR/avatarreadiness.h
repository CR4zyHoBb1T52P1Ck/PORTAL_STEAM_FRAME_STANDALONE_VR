#pragma once
namespace AvatarReadiness {
// Decide once before either eye. A pose that becomes ready in eye one may
// qualify the NEXT frame, never switch representation halfway through stereo.
struct State {
 const void* model=nullptr;bool complete=false,useBodyHands=false;
 void Begin(const void* next,bool sourceReady,bool enabled){useBodyHands=enabled&&sourceReady&&model==next&&complete;model=next;complete=false;}
 void Complete(){complete=true;}
 void Reset(){*this={};}
};
}
