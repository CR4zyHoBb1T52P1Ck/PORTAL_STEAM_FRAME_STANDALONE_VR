#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <initializer_list>
// Read-only Source RecvTable contract. Never call a possibly shifted legacy
// client virtual method: accept only a trivial MOV/RET getter and read its data.
namespace GroundState08 {
struct Table;
struct Prop {const char* name;int type,flags,stringSize,inside;void* extra;Prop* array;void* lengthProxy;void* proxy;void* tableProxy;Table* table;int offset,stride,elements;const char* arrayName;};
struct Table {Prop* props;int count;void* decoder;const char* name;bool initialized,mainList;};
struct Class {void* create;void* event;const char* name;Table* table;Class* next;int id;};
static_assert(sizeof(void*)==4&&sizeof(Prop)==60&&offsetof(Prop,offset)==44&&sizeof(Class)==24,"Source x86 RecvTable contract");
template<class Reader> bool Equal(const char* actual,const char* expected,Reader read){
 for(size_t i=0;i<64;++i){if(!actual||!read(reinterpret_cast<uintptr_t>(actual+i),1))return false;if(actual[i]!=expected[i])return false;if(!expected[i])return true;}return false;
}
template<class Reader> Class* Getter(void* client,unsigned slot,Reader read){
 if(!client||!read(reinterpret_cast<uintptr_t>(client),4))return nullptr;
 const auto table=*reinterpret_cast<uintptr_t*>(client);if(!read(table+slot*4,4))return nullptr;
 const auto fn=*reinterpret_cast<const uintptr_t*>(table+slot*4);if(!read(fn,6))return nullptr;
 const auto* code=reinterpret_cast<const unsigned char*>(fn);if(code[5]!=0xc3||(code[0]!=0xa1&&code[0]!=0xb8))return nullptr;
 uintptr_t value=0;memcpy(&value,code+1,4);
 if(code[0]==0xa1){if(!read(value,4))return nullptr;value=*reinterpret_cast<const uintptr_t*>(value);}
 return reinterpret_cast<Class*>(value);
}
struct Binding {
 int offset=-1;
 template<class Reader> bool Grounded(const void* player,Reader read,bool& known)const{
  known=offset>=0&&player&&read(reinterpret_cast<uintptr_t>(player)+offset,4);
  if(!known)return false;
  unsigned flags=0;memcpy(&flags,reinterpret_cast<const unsigned char*>(player)+offset,4);return (flags&1)!=0;
 }
};
template<class Reader> void Walk(Table* table,int base,Table* path[16],int depth,int& budget,int& found,bool& conflict,Reader read){
 if(!table||depth>=16||!read(reinterpret_cast<uintptr_t>(table),sizeof(Table)))return;
 for(int i=0;i<depth;++i)if(path[i]==table)return;
 if(table->count<1||table->count>1024||!table->props||!read(reinterpret_cast<uintptr_t>(table->props),size_t(table->count)*sizeof(Prop)))return;
 path[depth]=table;
 for(int i=0;i<table->count&&budget-->0;++i){const auto& prop=table->props[i];if(prop.offset<0||prop.offset>65535-base)continue;const int at=base+prop.offset;
  if(prop.type==0&&Equal(prop.name,"m_fFlags",read)){if(found>=0&&found!=at)conflict=true;found=at;}
  if(prop.type==6&&prop.table)Walk(prop.table,at,path,depth+1,budget,found,conflict,read);
 }
}
template<class Reader> Binding Resolve(void* client,Reader read){
 Binding out;bool conflict=false;
 for(unsigned slot:{8u,9u}){
  auto* entry=Getter(client,slot,read);Class* visited[1024]{};int visitedCount=0;
  for(int n=0;entry&&n<1024;++n){
   if(!read(reinterpret_cast<uintptr_t>(entry),sizeof(Class)))break;bool repeat=false;for(int i=0;i<visitedCount;++i)if(visited[i]==entry)repeat=true;if(repeat)break;visited[visitedCount++]=entry;
   auto* table=entry->table;
   if(table&&read(reinterpret_cast<uintptr_t>(table),sizeof(Table))&&(Equal(table->name,"DT_Portal_Player",read)||Equal(table->name,"DT_PortalPlayer",read))){Table* path[16]{};int budget=4096,found=-1;Walk(table,0,path,0,budget,found,conflict,read);if(found>=0){if(out.offset>=0&&out.offset!=found)conflict=true;out.offset=found;}}
   entry=entry->next;
  }
 }
 if(conflict)out.offset=-1;return out;
}
}
