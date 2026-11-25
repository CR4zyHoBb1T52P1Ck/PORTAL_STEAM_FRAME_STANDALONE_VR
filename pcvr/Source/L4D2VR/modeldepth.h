#pragma once
// Pure GPU viewport policy. Source's viewport, state blocks, projection,
// lighting, depth/stencil clear decisions and draw order remain untouched.
namespace ModelDepth {
constexpr int SpriteMode=2;
template<class DepthState,class Compare> inline void SpriteTest(bool sprites,DepthState& state,Compare lessOrEqual){
 if(sprites){state.enableDepthTest=true;state.enableDepthWrite=false;state.depthCompareOp=lessOrEqual;}
}
struct Range {float low,high;};
inline Range ForDraw(bool scene,float low,float high){
 // Normalize only the recognized native viewmodel slice. Other native ranges
 // (including portal/custom/reversed/degenerate ranges) retain their semantics.
 return scene&&low==0&&high>0&&high<=.25f?Range{0,1}:Range{low,high};
}
struct State {
 bool active=false,sprites=false;
 bool Set(bool next,bool sprite=false){const bool nextSprite=next&&sprite;if(next==active&&nextSprite==sprites)return false;active=next;sprites=nextSprite;return true;}
 Range Draw(float low,float high)const{return ForDraw(active,low,high);}
};
}
