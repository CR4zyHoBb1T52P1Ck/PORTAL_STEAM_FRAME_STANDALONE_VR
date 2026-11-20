#pragma once
namespace PropHold {
// One native held object; either controller may own it. Selection is a fresh
// squeeze, hold is sustained, and only the owner's release can request a throw.
struct Input {
    int hand=-1;bool attached=false,use=false,release=false,throwRelease=false,cancelled=false;
    bool previous[2]{},armed[2]{true,true};
    void Detached(){hand=-1;attached=use=release=throwRelease=cancelled=false;}
    void CancelThrow(){throwRelease=false;if(release)cancelled=true;}
    void Suspend(){use=false;throwRelease=false;cancelled=true;release=attached;armed[0]=armed[1]=false;if(!attached)hand=-1;}
    void Attach(){if(hand>=0)attached=true;}
    void Step(bool allowed,const bool grip[2],const bool tracked[2],const bool eligible[2],bool transferNear=false) {
        if(!allowed){Suspend();return;}
        bool rising[2]{};
        for(int h=0;h<2;++h){if(!tracked[h])armed[h]=false;if(tracked[h]&&!grip[h])armed[h]=true;rising[h]=tracked[h]&&armed[h]&&grip[h]&&!previous[h];}
        if(hand>=0&&attached&&!release&&transferNear){const int other=1-hand;
            if(rising[other]&&eligible[other]){hand=other;use=true;throwRelease=cancelled=false;}}
        if(hand>=0) {
            if(!tracked[hand]||!grip[hand]) {
                use=false;
                if(attached){release=true;if(!tracked[hand]){cancelled=true;throwRelease=false;}else throwRelease=throwRelease||(!cancelled&&previous[hand]&&!grip[hand]);}
                else Detached();
            } else if(!release)use=true;
        } else for(int h=0;h<2;++h)if(rising[h]&&eligible[h]){hand=h;use=true;cancelled=false;break;}
        for(int h=0;h<2;++h)previous[h]=grip[h];
    }
};
}
