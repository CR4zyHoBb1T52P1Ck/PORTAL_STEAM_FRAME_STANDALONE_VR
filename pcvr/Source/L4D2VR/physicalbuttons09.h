#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>
// ButtonPress09 is policy only. The caller must obtain an exact red-switch
// model hit, unobstructed physical contact and matching native FindUseEntity
// result on the server, and revalidate the serial handle before dispatch.
// No entity methods, player fields, hulls or physics are changed here.
namespace ButtonPress09 {
struct Contact {
    void* entity=nullptr;
    uint32_t handle=0xffffffffu;
    int source=-1; // two palms followed by the three existing gun probes
    bool redSwitch=false,nativeMatch=false,visible=false,contact=false;
    bool Valid() const {return entity&&handle!=0xffffffffu&&source>=0&&source<5
        &&redSwitch&&nativeMatch&&visible&&contact;}
};
struct Request {void* entity=nullptr;uint32_t handle=0xffffffffu;int source=-1;
    explicit operator bool()const{return entity&&handle!=0xffffffffu&&source>=0&&source<5;}};
struct Entry {void* entity=nullptr;uint32_t handle=0xffffffffu;int source=-1;
    double contactSince=0,lastSeen=0,lastFire=-1000;bool seen=false,latched=false;};
struct Gate {
    Entry entries[8]{};double lastTime=-1,clearSince=-1;bool neutralRequired=true;
    void Reset(){*this=Gate{};}
    Request Step(double now,bool allowed,const Contact* contacts,size_t count) {
        if(!std::isfinite(now)||now<0){Reset();return {};}
        if(lastTime>=0&&(now<lastTime||now-lastTime>.5)){Reset();}
        lastTime=now;
        bool any=false;
        for(size_t i=0;contacts&&i<count&&i<5;++i)any|=contacts[i].Valid();
        if(!allowed){neutralRequired=true;clearSince=-1;for(auto& e:entries)e={};return {};}
        if(neutralRequired){
            if(any)clearSince=-1;
            else if(clearSince<0)clearSince=now;
            else if(now-clearSince>=.12){neutralRequired=false;clearSince=-1;}
            return {};
        }
        for(auto& e:entries)e.seen=false;
        Request request;
        for(size_t i=0;contacts&&i<count&&i<5;++i){const auto& c=contacts[i];if(!c.Valid())continue;
            Entry* e=nullptr;
            for(auto& item:entries)if(item.entity==c.entity&&item.handle==c.handle){e=&item;break;}
            if(!e)for(auto& item:entries)if(!item.entity){e=&item;*e={};e->entity=c.entity;e->handle=c.handle;e->source=c.source;e->contactSince=now;e->lastSeen=now;break;}
            if(!e)continue;
            if(e->seen)continue; // two hands/gun probes cannot multiply presses
            e->seen=true;e->source=c.source;
            if(now-e->lastSeen>=.12){e->latched=false;e->contactSince=now;}
            e->lastSeen=now;
            if(!e->latched&&now-e->contactSince>=.025&&now-e->lastFire>=.5&&!request){
                e->latched=true;e->lastFire=now;request={e->entity,e->handle,e->source};
            }
        }
        for(auto& e:entries)if(e.entity&&!e.seen&&now-e.lastSeen>=.12){
            e.latched=false;e.contactSince=now;
            if(now-e.lastSeen>1)e={};
        }
        return request;
    }
};
// One-command pulse with a short query validity window. The native input path
// determines pressed/released edges and dispatches Use. A caller must never
// bypass validation or call a guessed target Use virtual.
struct Pulse {
    Request pending{},active{};double queued=0,until=0;bool releaseCommand=false;
    void Reset(){*this=Pulse{};}
    bool Queue(Request request,double now){
        if(!request||!std::isfinite(now)||now<0||pending||active||releaseCommand)return false;
        pending=request;queued=now;return true;
    }
    bool Command(double now,bool allowed,bool manualUse){
        if(!allowed||manualUse||!std::isfinite(now)){Reset();return false;}
        if(releaseCommand){releaseCommand=false;return false;}
        if(!pending)return false;
        const auto request=pending;pending={};
        if(now<queued||now-queued>.12)return false;
        active=request;until=now+.12;releaseCommand=true;return true;
    }
    Request Query(double now) const {
        return std::isfinite(now)&&now>=until-.12&&now<=until?active:Request{};
    }
    void Completed(){active={};until=0;}
};
}
