#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
// Offline-tested sidecar foundation. No engine calls, file I/O, GPU use or
// integration into live tracking. Coordinates are metres, quaternion xyzw.
namespace ReplayPose {
static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559,"IEEE754 float32 required");
constexpr size_t HeaderBytes=160,FrameBytes=216;
using Hash=std::array<uint8_t,32>;
using HeaderPacket=std::array<uint8_t,HeaderBytes>;
using FramePacket=std::array<uint8_t,FrameBytes>;
enum Event:uint32_t {Portal=1,Recenter=2,TrackingGap=4,MapChange=8,Seek=16,QueueGap=32};
enum Gun:uint32_t {Stowed=1,HeldLeft=2,HeldRight=4,SupportGrip=8};
struct Pose {std::array<float,3> position{};std::array<float,4> rotation{0,0,0,1};};
struct Frame {
    uint32_t tick=0;uint64_t micros=0;uint32_t segment=0,settings=0,events=0,tracked=0,gun=0;
    std::array<Pose,5> poses{}; // head, left, right, gun, body root
    std::array<float,10> curls{}; // left then right; thumb through little finger
};
inline bool Nonzero(const Hash& hash){for(auto byte:hash)if(byte)return true;return false;}
struct Header {
    uint32_t tickNumerator=60,tickDenominator=1;std::array<Hash,4> hashes{}; // demo, engine, runtime, settings schema
    bool finalized=false;
    bool Linked()const{for(const auto& h:hashes)if(!Nonzero(h))return false;return true;}
    bool Valid()const{return tickNumerator&&tickDenominator&&tickNumerator<=1000000&&tickDenominator<=1000000&&double(tickNumerator)/tickDenominator>=1&&double(tickNumerator)/tickDenominator<=1000&&(!finalized||Linked());}
    bool Matches(const Header& expected)const{return Valid()&&expected.Valid()&&finalized&&expected.finalized&&Linked()&&hashes==expected.hashes&&tickNumerator==expected.tickNumerator&&tickDenominator==expected.tickDenominator;}
};
inline bool Valid(const Frame& f){
    if(f.tick>0x7fffffffu||f.micros>0x7fffffffffffffffull||(f.events&~63u)||(f.tracked&~31u)||(f.gun&~15u))return false;
    if((f.gun&HeldLeft)&&(f.gun&HeldRight))return false;
    if((f.gun&SupportGrip)&&!(f.gun&(HeldLeft|HeldRight)))return false;
    if((f.gun&Stowed)&&(f.gun&(HeldLeft|HeldRight|SupportGrip)))return false;
    for(const auto& pose:f.poses){
        for(float x:pose.position)if(!std::isfinite(x)||std::abs(x)>1000000)return false;
        double norm=0;for(float x:pose.rotation){if(!std::isfinite(x))return false;norm+=double(x)*x;}
        if(!std::isfinite(norm)||std::abs(norm-1)>.02)return false;
    }
    for(float x:f.curls)if(!std::isfinite(x)||x<0||x>1)return false;
    return true;
}
inline uint32_t CRC(const uint8_t* data,size_t n){
    uint32_t result=~0u;for(size_t i=0;i<n;++i){result^=data[i];for(int bit=0;bit<8;++bit)result=(result>>1)^(0xedb88320u&uint32_t(-int(result&1)));}return ~result;
}
struct Put {
    uint8_t* data;size_t at=0;
    void U32(uint32_t x){for(int i=0;i<4;++i)data[at++]=uint8_t(x>>(i*8));}
    void U64(uint64_t x){U32(uint32_t(x));U32(uint32_t(x>>32));}
    void Float(float x){static_assert(sizeof(float)==4,"32-bit float required");uint32_t bits;std::memcpy(&bits,&x,4);U32(bits);}
};
struct Get {
    const uint8_t* data;size_t at=0;
    uint32_t U32(){uint32_t x=0;for(int i=0;i<4;++i)x|=uint32_t(data[at++])<<(i*8);return x;}
    uint64_t U64(){const uint64_t low=U32();return low|(uint64_t(U32())<<32);}
    float Float(){const auto bits=U32();float x;std::memcpy(&x,&bits,4);return x;}
};
inline bool Encode(const Header& h,HeaderPacket& out){
    if(!h.Valid())return false;HeaderPacket packet{};Put p{packet.data()};
    const char magic[8]={'P','V','R','P','O','S','E','1'};for(auto c:magic)packet[p.at++]=uint8_t(c);
    p.U32(1);p.U32(uint32_t(FrameBytes));p.U32(h.tickNumerator);p.U32(h.tickDenominator);
    for(const auto& hash:h.hashes)for(auto c:hash)packet[p.at++]=c;
    p.U32(h.finalized?1:0);p.U32(CRC(packet.data(),HeaderBytes-4));out=packet;return true;
}
inline bool Decode(const uint8_t* data,size_t size,Header& out){
    if(!data||size!=HeaderBytes||std::memcmp(data,"PVRPOSE1",8))return false;
    Get crc{data,HeaderBytes-4};if(crc.U32()!=CRC(data,HeaderBytes-4))return false;
    Get p{data,8};if(p.U32()!=1||p.U32()!=FrameBytes)return false;
    Header h;h.tickNumerator=p.U32();h.tickDenominator=p.U32();
    for(auto& hash:h.hashes)for(auto& c:hash)c=data[p.at++];
    const auto flags=p.U32();if(flags>1)return false;h.finalized=flags!=0;
    if(!h.Valid())return false;out=h;return true;
}
inline bool Encode(const Frame& f,FramePacket& out){
    if(!Valid(f))return false;FramePacket packet{};Put p{packet.data()};
    p.U32(f.tick);p.U64(f.micros);p.U32(f.segment);p.U32(f.settings);p.U32(f.events);p.U32(f.tracked);p.U32(f.gun);
    for(const auto& pose:f.poses){for(float x:pose.position)p.Float(x);for(float x:pose.rotation)p.Float(x);}
    for(float x:f.curls)p.Float(x);p.U32(CRC(packet.data(),FrameBytes-4));out=packet;return true;
}
inline bool Decode(const uint8_t* data,size_t size,Frame& out){
    if(!data||size!=FrameBytes)return false;Get crc{data,FrameBytes-4};if(crc.U32()!=CRC(data,FrameBytes-4))return false;
    Get p{data};Frame f;f.tick=p.U32();f.micros=p.U64();f.segment=p.U32();f.settings=p.U32();f.events=p.U32();f.tracked=p.U32();f.gun=p.U32();
    for(auto& pose:f.poses){for(float& x:pose.position)x=p.Float();for(float& x:pose.rotation)x=p.Float();}
    for(float& x:f.curls)x=p.Float();if(!Valid(f))return false;out=f;return true;
}
inline std::array<float,4> Slerp(std::array<float,4> a,std::array<float,4> b,float t){
    const auto normalize=[](auto& q){double n=0;for(float x:q)n+=double(x)*x;const auto inv=1/std::sqrt(n);for(auto& x:q)x=float(x*inv);};
    normalize(a);normalize(b);float dot=0;for(int i=0;i<4;++i)dot+=a[i]*b[i];
    if(dot<0){for(auto& x:b)x=-x;dot=-dot;}dot=std::clamp(dot,0.f,1.f);
    float wa=1-t,wb=t;if(dot<.9995f){const float theta=std::acos(dot),sine=std::sin(theta);wa=std::sin((1-t)*theta)/sine;wb=std::sin(t*theta)/sine;}
    std::array<float,4> out{};for(int i=0;i<4;++i)out[i]=wa*a[i]+wb*b[i];normalize(out);return out;
}
inline bool Blend(const Frame& a,const Frame& b,float t,Frame& out){
    if(!Valid(a)||!Valid(b)||!std::isfinite(t)||t<0||t>1||a.segment!=b.segment||a.settings!=b.settings||a.tracked!=b.tracked||a.gun!=b.gun||b.events||b.tick<a.tick||b.micros<=a.micros||b.micros-a.micros>100000)return false;
    for(int i=0;i<5;++i)if(a.tracked&(1u<<i)){
        double distance=0;for(int n=0;n<3;++n){const double d=b.poses[i].position[n]-a.poses[i].position[n];distance+=d*d;}
        if(distance>.75*.75)return false;
    }
    Frame f=a;f.events=0;f.tick=a.tick+uint32_t(double(b.tick-a.tick)*t);f.micros=a.micros+uint64_t(double(b.micros-a.micros)*t);
    for(int i=0;i<5;++i){for(int n=0;n<3;++n)f.poses[i].position[n]=a.poses[i].position[n]+(b.poses[i].position[n]-a.poses[i].position[n])*t;f.poses[i].rotation=Slerp(a.poses[i].rotation,b.poses[i].rotation,t);}
    for(int i=0;i<10;++i)f.curls[i]=a.curls[i]+(b.curls[i]-a.curls[i])*t;out=f;return true;
}
// Exactly one producer and one consumer. No allocation, mutex, wait or I/O on
// the producer. A dropped/invalid record cuts the next accepted interpolation.
template<size_t N=128> class Queue {
    static_assert(N>=2&&N<=4096&&(N&(N-1))==0,"bounded power-of-two capacity required");
    static_assert(std::atomic<uint32_t>::is_always_lock_free,"lock-free 32-bit atomics required");
    std::array<Frame,N> data{};alignas(64) std::atomic<uint32_t> write{0};alignas(64) std::atomic<uint32_t> read{0};bool gap=false;
public:
    std::atomic<uint32_t> dropped{0},rejected{0};
    bool Push(const Frame& f){
        if(!Valid(f)){rejected.fetch_add(1,std::memory_order_relaxed);gap=true;return false;}
        const auto w=write.load(std::memory_order_relaxed),r=read.load(std::memory_order_acquire);
        if(w-r>=N){dropped.fetch_add(1,std::memory_order_relaxed);gap=true;return false;}
        data[w&(N-1)]=f;if(gap)data[w&(N-1)].events|=QueueGap;gap=false;write.store(w+1,std::memory_order_release);return true;
    }
    bool Pop(Frame& f){
        const auto r=read.load(std::memory_order_relaxed),w=write.load(std::memory_order_acquire);if(r==w)return false;
        f=data[r&(N-1)];read.store(r+1,std::memory_order_release);return true;
    }
};
struct Writer {
    bool failed=false;uint64_t written=0;
    template<size_t N,class Sink> unsigned Drain(Queue<N>& queue,Sink sink,unsigned budget=32){
        if(failed)return 0;budget=std::min(budget,256u);unsigned count=0;Frame frame;
        while(count<budget&&queue.Pop(frame)){
            FramePacket packet;
            try{if(!Encode(frame,packet)||!sink(packet.data(),packet.size())){failed=true;break;}}
            catch(...){failed=true;break;}
            ++written;++count;
        }
        return count;
    }
};
}
