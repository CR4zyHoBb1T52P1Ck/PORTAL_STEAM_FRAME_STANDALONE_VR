#pragma once
#include <cstddef>
namespace ModelPath {
// Retail Source MDLs mix slash styles; model-info names usually use '/'.
inline bool Equal(const char* a,const char* b,size_t limit=64) {
    if(!a||!b)return false;
    for(size_t i=0;i<limit;++i) {
        unsigned char x=a[i],y=b[i];if(x=='\\')x='/';if(y=='\\')y='/';
        if(x>='A'&&x<='Z')x+='a'-'A';if(y>='A'&&y<='Z')y+='a'-'A';
        if(x!=y)return false;if(!x)return true;
    }
    return false;
}
}
