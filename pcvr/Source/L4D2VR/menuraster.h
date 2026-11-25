#pragma once
#include <cstdint>
namespace MenuRaster {
struct Swap {
    bool ready=false,pending=false;
    int front=0,shownPage=-1,uploadPage=-1;
    uint64_t started=0,last=0,retryAfter=0;
    bool Begin(uint64_t now,int page) {
        if(now<retryAfter||pending||(ready&&page==shownPage&&now-last<90))return false;
        pending=true;uploadPage=page;started=last=now;return true;
    }
    bool Loaded() {
        if(!pending)return false;
        pending=false;ready=true;retryAfter=0;front=1-front;shownPage=uploadPage;return true;
    }
    void Failed(uint64_t now=0){pending=false;retryAfter=now?now+500:0;}
    bool AcceptsClicks(int page) const {return ready&&shownPage==page;}
};
}
