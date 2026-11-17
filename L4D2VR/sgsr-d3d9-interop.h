#pragma once
#include "sgsr-d3d9-bytecode.h"
#include "debuglog.h"
// D3D9 draw submitted through the existing DXVK queue. Each eye has an
// independent output image; there is no shared temporal history or readback.
class PortalFrameUpscaler {
    IDirect3DPixelShader9* shaders[3]{};
    IDirect3DStateBlock9* state=nullptr;
    IDirect3DTexture9* textures[2]{};
    IDirect3DSurface9* surfaces[2]{};
    unsigned width=0,height=0;
    template<class T> static void Release(T*& value) {if(value)value->Release();value=nullptr;}
    void ClearTargets() {for(auto& p:surfaces)Release(p);for(auto& p:textures)Release(p);width=height=0;}
public:
    IDirect3DSurface9* Surface(int eye) const {return surfaces[eye];}
    ~PortalFrameUpscaler() {ClearTargets();Release(state);for(auto& shader:shaders)Release(shader);}
    HRESULT Render(IDirect3DDevice9* device,VR* vr) {
        vr->m_UpscaleReady=false;
        if(!vr->m_Graphics.upscaler || !vr->m_RenderedNewFrame || !vr->m_D9LeftEyeSurface || !vr->m_D9RightEyeSurface) return S_FALSE;
        const auto started=std::chrono::steady_clock::now();
        const int mode=std::clamp(vr->m_Graphics.upscaler,1,3)-1;
        const unsigned char* programs[]={PortalFrameSGSR,PortalFrameSGSREdge,PortalFrameBilinear};
        auto& shader=shaders[mode];
        if(!shader) {
            if(FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(programs[mode]),&shader)))return E_FAIL;
            PortalVrLog("Spatial shader ready mode=%d",mode+1);
        }
        if(!state && FAILED(device->CreateStateBlock(D3DSBT_ALL,&state))) return E_FAIL;
        D3DSURFACE_DESC desc{};if(FAILED(vr->m_D9LeftEyeSurface->GetDesc(&desc))) return E_FAIL;
        if(width!=vr->m_UpscaleWidth || height!=vr->m_UpscaleHeight) {
            ClearTargets();
            for(int eye=0;eye<2;++eye) {
                if(FAILED(device->CreateTexture(vr->m_UpscaleWidth,vr->m_UpscaleHeight,1,D3DUSAGE_RENDERTARGET,
                    desc.Format,D3DPOOL_DEFAULT,&textures[eye],nullptr)) || FAILED(textures[eye]->GetSurfaceLevel(0,&surfaces[eye]))) {
                    ClearTargets();return E_FAIL;
                }
            }
            width=vr->m_UpscaleWidth;height=vr->m_UpscaleHeight;
            PortalVrLog("Spatial targets created mode=%d input=%ux%u output=%ux%u twoEyeBytes=%llu",mode+1,desc.Width,desc.Height,width,height,2ull*width*height*4);
        }
        if(FAILED(state->Capture())) return E_FAIL;
        // State blocks exclude render targets/depth surfaces. Preserve those
        // explicitly and apply the saved viewport after restoring them.
        struct Restore {
            IDirect3DDevice9* device;IDirect3DStateBlock9* state;
            IDirect3DSurface9* targets[4]{};IDirect3DSurface9* depth=nullptr;
            bool begun=false;
            Restore(IDirect3DDevice9* d,IDirect3DStateBlock9* s):device(d),state(s) {
                for(int i=0;i<4;++i)device->GetRenderTarget(i,&targets[i]);device->GetDepthStencilSurface(&depth);
            }
            ~Restore() {
                if(begun)device->EndScene();
                device->SetDepthStencilSurface(nullptr);
                for(int i=0;i<4;++i) {device->SetRenderTarget(i,targets[i]);if(targets[i])targets[i]->Release();}
                device->SetDepthStencilSurface(depth);if(depth)depth->Release();state->Apply();
            }
        } restore(device,state);
        if(FAILED(device->BeginScene())) return E_FAIL;restore.begun=true;
        device->SetDepthStencilSurface(nullptr);
        for(int i=1;i<4;++i)device->SetRenderTarget(i,nullptr);
        device->SetVertexShader(nullptr);device->SetPixelShader(shader);
        device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);
        for(auto setting:{D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_ALPHATESTENABLE,
            D3DRS_STENCILENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_FOGENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_CLIPPLANEENABLE}) device->SetRenderState(setting,FALSE);
        device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
        device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
        for(int sampler=0;sampler<2;++sampler) {
            device->SetSamplerState(sampler,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);device->SetSamplerState(sampler,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
            device->SetSamplerState(sampler,D3DSAMP_MINFILTER,sampler ? D3DTEXF_POINT : D3DTEXF_LINEAR);
            device->SetSamplerState(sampler,D3DSAMP_MAGFILTER,sampler ? D3DTEXF_POINT : D3DTEXF_LINEAR);
            device->SetSamplerState(sampler,D3DSAMP_MIPFILTER,D3DTEXF_NONE);device->SetSamplerState(sampler,D3DSAMP_SRGBTEXTURE,FALSE);
        }
        const float constants[]={1.f/desc.Width,1.f/desc.Height,float(desc.Width),float(desc.Height)};
        device->SetPixelShaderConstantF(0,constants,1);
        const float upscaleControl[]={std::clamp(vr->m_Graphics.upscaleSharpness,1.f,2.f),0,0,0};
        device->SetPixelShaderConstantF(1,upscaleControl,1);
        const D3DVIEWPORT9 viewport={0,0,width,height,0,1};device->SetViewport(&viewport);
        struct Vertex {float x,y,z,w,u,v;};
        const Vertex quad[]={{-.5f,-.5f,0,1,0,0},{float(width)-.5f,-.5f,0,1,1,0},
            {-.5f,float(height)-.5f,0,1,0,1},{float(width)-.5f,float(height)-.5f,0,1,1,1}};
        IDirect3DSurface9* sources[]={vr->m_D9LeftEyeSurface,vr->m_D9RightEyeSurface};
        SharedTextureHolder* outputs[]={&vr->m_VKUpscaledLeft,&vr->m_VKUpscaledRight};
        for(int eye=0;eye<2;++eye) {
            IDirect3DTexture9* input=nullptr;
            if(FAILED(sources[eye]->GetContainer(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(&input))) || !input) return E_FAIL;
            const auto renderTargetResult=device->SetRenderTarget(0,surfaces[eye]);
            device->SetTexture(0,input);device->SetTexture(1,input);input->Release();
            if(FAILED(renderTargetResult) || FAILED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex)))) return E_FAIL;
            D3D9_TEXTURE_VR_DESC optical{};
            if(FAILED(g_D3DVR9->GetVRDesc(surfaces[eye],&optical))) return E_FAIL;
            memcpy(&outputs[eye]->m_VulkanData,&optical,sizeof(optical));
            outputs[eye]->m_VRTexture={&outputs[eye]->m_VulkanData,vr::TextureType_Vulkan,vr::ColorSpace_Auto};
        }
        vr->m_UpscaleReady=true;
        static unsigned samples=0;
        if((samples++%600)==0) {
            const double cpu=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
            PortalVrLog("Spatial pass mode=%d input=%ux%u output=%ux%u sharpness=%f cpuSubmissionMs=%f (not GPU time)",mode+1,desc.Width,desc.Height,width,height,vr->m_Graphics.upscaleSharpness,cpu);
        }
        return S_OK;
    }
};
