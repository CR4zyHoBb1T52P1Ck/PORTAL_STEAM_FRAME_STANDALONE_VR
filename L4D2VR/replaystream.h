#pragma once
#include "replaypose.h"
// Unused recording-worker prototype. Call from ONE consumer only. No engine,
// renderer, tracking or thread control. The producer remains ReplayPose::Queue.
namespace ReplayPose {
enum class StreamError {None,Header,Sequence,Encode,Write,Flush,Close,Capacity};
template<class Sink> class DraftStream {
    Sink& sink;bool started=false,closed=false;bool previous=false;
    Frame last{};uint64_t cap;
    bool Fail(StreamError error){if(errorCode==StreamError::None)errorCode=error;return false;}
public:
    StreamError errorCode=StreamError::None;uint64_t written=0;
    explicit DraftStream(Sink& target,uint64_t maxFrames=2000000):sink(target),cap(maxFrames){}
    DraftStream(const DraftStream&)=delete;DraftStream& operator=(const DraftStream&)=delete;
    ~DraftStream(){Finish();}
    bool Good()const{return started&&!closed&&errorCode==StreamError::None;}
    bool Begin(const Header& header){
        if(started||closed||errorCode!=StreamError::None)return false;
        HeaderPacket packet;
        if(header.finalized||!cap||cap>2000000||!Encode(header,packet))return Fail(StreamError::Header);
        try{if(sink.Write(packet.data(),packet.size())!=packet.size())return Fail(StreamError::Write);}
        catch(...){return Fail(StreamError::Write);}
        started=true;return true;
    }
    bool Append(const Frame& frame){
        if(!Good())return false;
        if(written>=cap)return Fail(StreamError::Capacity);
        FramePacket packet;if(!Encode(frame,packet))return Fail(StreamError::Encode);
        if(previous&&(frame.micros<=last.micros||(frame.tick<last.tick&&frame.segment==last.segment&&!(frame.events&(MapChange|Seek)))))return Fail(StreamError::Sequence);
        try{if(sink.Write(packet.data(),packet.size())!=packet.size())return Fail(StreamError::Write);}
        catch(...){return Fail(StreamError::Write);}
        ++written;last=frame;previous=true;return true;
    }
    template<size_t N> unsigned Drain(Queue<N>& queue,unsigned budget=32){
        if(!Good())return 0;budget=std::min(budget,256u);unsigned count=0;Frame frame;
        while(count<budget){
            // Stop the recorder at its data cap without consuming another item.
            // Never stop the application or the producer's session.
            if(written>=cap){Fail(StreamError::Capacity);break;}
            if(!queue.Pop(frame)||!Append(frame))break;++count;
        }
        return count;
    }
    bool Flush(){
        if(!Good())return false;
        try{if(!sink.Flush())return Fail(StreamError::Flush);}
        catch(...){return Fail(StreamError::Flush);}return true;
    }
    bool Finish(){
        if(closed)return errorCode==StreamError::None;
        if(Good())Flush();closed=true;
        try{if(!sink.Close())Fail(StreamError::Close);}catch(...){Fail(StreamError::Close);}
        return errorCode==StreamError::None;
    }
};
}
