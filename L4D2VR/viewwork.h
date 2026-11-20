#pragma once
#include <algorithm>
namespace ViewWork {
// Counts secondary 3D views, including portals/reflections/depth passes. These
// are deliberately not labelled portal-only without a native pass identity.
struct Frame {
    unsigned secondary=0,maxDepth=0,rebases=0,cacheHits=0;float bodyMs=0;
    void View(bool primary,unsigned depth){if(!primary)++secondary;maxDepth=std::max(maxDepth,depth);}
};
}
