// D3D9 / Shader Model 3 adapter for Qualcomm's BSD-3-Clause SGSR 1.
// The unmodified upstream header and complete license are retained in thirdparty/sgsr.
#define SGSR_MOBILE
#if SGSR_EDGE_DIRECTION
#define UseEdgeDirection
#endif
float4 ViewportInfo : register(c0);
float4 UpscaleControl : register(c1);
sampler2D LinearImage : register(s0);
sampler2D PointImage : register(s1);
half4 SGSRRGBH(float2 uv) { return tex2Dlod(LinearImage,float4(uv,0,0)); }
half4 SGSRH(float2 p,int channel) {
    float2 base=(floor(p*ViewportInfo.zw-.5)+.5)*ViewportInfo.xy;
    return half4(tex2Dlod(PointImage,float4(base+float2(0,ViewportInfo.y),0,0)).g,
        tex2Dlod(PointImage,float4(base+ViewportInfo.xy,0,0)).g,
        tex2Dlod(PointImage,float4(base+float2(ViewportInfo.x,0),0,0)).g,
        tex2Dlod(PointImage,float4(base,0,0)).g);
}
#include "../thirdparty/sgsr/sgsr1_vr.h"
float4 main(float2 uv:TEXCOORD0):COLOR0 {
#if BILINEAR_ONLY
    return tex2Dlod(LinearImage,float4(uv,0,0));
#else
    half4 color=half4(0,0,0,1);SgsrYuvH(color,uv,ViewportInfo);return color;
#endif
}
