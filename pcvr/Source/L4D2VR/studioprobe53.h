#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace StudioProbe {
// One call's validated prefix only. Never caches readable memory across draws.
struct Prefix {const unsigned char* header=nullptr;int length=0;unsigned checks=0;};
template<class Readable> Prefix Read(const void* state,Readable readable){
 Prefix out;if(!state)return out;++out.checks;if(!readable(reinterpret_cast<uintptr_t>(state),sizeof(void*)))return out;
 const unsigned char* header=nullptr;std::memcpy(&header,state,sizeof(header));if(!header)return out;
 ++out.checks;if(!readable(reinterpret_cast<uintptr_t>(header),164))return out;
 out.header=header;std::memcpy(&out.length,header+76,sizeof(out.length));return out;
}
}
