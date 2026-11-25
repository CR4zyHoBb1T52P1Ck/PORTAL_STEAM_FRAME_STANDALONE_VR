#pragma once
#include <cstdint>
namespace SceneBookmark {
struct Request {
 bool pending=false;uint32_t serial=0;
 bool Toggle(){pending=!pending;if(pending&&++serial==0)++serial;return pending;}
 bool Take(bool inGame,bool cursor,bool menu,bool focused){
  if(!pending||!inGame||cursor||menu||!focused)return false;pending=false;return true;
 }
};
}
