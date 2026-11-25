#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace GripHold05 {
// Input hysteresis only. No process/session timers. Acquisition always needs
// the ordinary button/fallback; only an existing weapon grip can be sustained.
struct State {
    uint64_t lastCertain=0;
    bool Resolve(bool raw,bool eligible,bool fresh,bool skeleton,const float* curls,
                 bool previous,uint64_t now,float threshold,float graceMS) {
        if(!fresh){lastCertain=0;return false;}
        if(raw){lastCertain=now;return true;}
        if(!eligible||!previous||!skeleton||!curls||!std::isfinite(threshold)||!std::isfinite(graceMS)){lastCertain=0;return false;}
        float average=0;
        for(int i=2;i<5;++i){if(!std::isfinite(curls[i])){lastCertain=0;return false;}average+=std::clamp(curls[i],0.f,1.f)/3;}
        if(average>=std::clamp(threshold,.3f,.7f)){lastCertain=now;return true;}
        // Clearly opening the hand releases immediately, including during fire.
        if(average<=.2f||now<lastCertain){lastCertain=0;return false;}
        return lastCertain&&now-lastCertain<=uint64_t(std::clamp(graceMS,0.f,150.f));
    }
};
}
