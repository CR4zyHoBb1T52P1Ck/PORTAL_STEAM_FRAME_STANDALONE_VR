#include "../dxvk/dxvk_include.h"

#include "d3d9_vr.h"

#include "d3d9_include.h"
#include "d3d9_surface.h"

#include "d3d9_device.h"

#include "L4D2VR/game.h"
#include "L4D2VR/vr.h"
#include "L4D2VR/sgsr-d3d9-interop.h"
#include "L4D2VR/overlaypixels.h"
#include "L4D2VR/gpuworkload.h"
#include "L4D2VR/renderpolicy.h"
#include "L4D2VR/mirrorgeometry.h"
#include <map>
#include <memory>

namespace dxvk {

    class D3D9VR final : public ComObjectClamp<IDirect3DVR9>
    {
    public:

        D3D9VR(IDirect3DDevice9 *pDevice)
            : m_device(static_cast<D3D9DeviceEx *>(pDevice))
        {}

        ~D3D9VR() {
            if(m_backgroundSurface)m_backgroundSurface->Release();
            if(m_background)m_background->Release();
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID riid,
            void **ppvObject)
        {
            if (ppvObject == nullptr)
                return E_POINTER;

            *ppvObject = nullptr;

            if (riid == __uuidof(IUnknown) ||
                riid == __uuidof(IDirect3DVR9)) {
                *ppvObject = ref(this);
                return S_OK;
            }

            Logger::warn("D3D9VR::QueryInterface: Unknown interface query");
            Logger::warn(str::format(riid));
            return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE GetVRDesc(
            IDirect3DSurface9 *pSurface,
            D3D9_TEXTURE_VR_DESC *pDesc)
        {
            if (unlikely(pSurface == nullptr || pDesc == nullptr))
                return D3DERR_INVALIDCALL;

            D3D9Surface *surface = static_cast<D3D9Surface *>(pSurface);

            const auto *tex = surface->GetCommonTexture();

            const auto &desc = tex->Desc();
            const auto &image = desc->MultiSample != D3DMULTISAMPLE_NONE ? const_cast<D3D9CommonTexture*>(tex)->GetResolveImage() : tex->GetImage();
            const auto &device = tex->Device()->GetDXVKDevice();

            // I don't know why the image randomly is a uint64_t in OpenVR.
            pDesc->Image = uint64_t(image->handle());
            pDesc->Device = device->handle();
            pDesc->PhysicalDevice = device->adapter()->handle();
            pDesc->Instance = device->instance()->handle();
            pDesc->Queue = device->queues().graphics.queueHandle;
            pDesc->QueueFamilyIndex = device->queues().graphics.queueIndex;

            pDesc->Width = desc->Width;
            pDesc->Height = desc->Height;
            pDesc->Format = tex->GetFormatMapping().FormatColor;
            pDesc->SampleCount = uint32_t(image->info().sampleCount);

            return D3D_OK;
        }

        HRESULT STDMETHODCALLTYPE TransferSurface(
            IDirect3DSurface9 *pSurface,
            BOOL waitResourceIdle)
        {
            if (unlikely(pSurface == nullptr))
                return D3DERR_INVALIDCALL;

            auto *tex = static_cast<D3D9Surface *>(pSurface)->GetCommonTexture();
            const auto image = tex->Desc()->MultiSample!=D3DMULTISAMPLE_NONE?tex->GetResolveImage():tex->GetImage();
            if(image==nullptr)return D3DERR_INVALIDCALL;
            m_device->TransformVrImage(image,true);

            // This wait may need to be on all Faces and Mip Levels (2 loops).
            if (waitResourceIdle)
                WaitDeviceIdle();

            return D3D_OK;
        }

        HRESULT STDMETHODCALLTYPE LockDevice()
        {
            m_lock = m_device->LockDevice();
            return D3D_OK;
        }

        HRESULT STDMETHODCALLTYPE UnlockDevice()
        {
            m_lock = D3D9DeviceLock();
            return D3D_OK;
        }

        HRESULT STDMETHODCALLTYPE WaitDeviceIdle()
        {
            m_device->Flush();
            // Not clear if we need all here, perhaps...
            m_device->SynchronizeCsThread(DxvkCsThread::SynchronizeAll);
            m_device->GetDXVKDevice()->waitForIdle();
            return D3D_OK;
        }

        HRESULT STDMETHODCALLTYPE GetBackBufferData(SharedTextureHolder* backBufferData)
        {
            if (!backBufferData)
                return D3DERR_INVALIDCALL;
            IDirect3DSurface9 *backBufferSurface = nullptr;
            HRESULT res = m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBufferSurface);
            if (FAILED(res) || !backBufferSurface)
                return FAILED(res) ? res : D3DERR_INVALIDCALL;
            D3D9_TEXTURE_VR_DESC textureDesc{};
            res = GetVRDesc(backBufferSurface, &textureDesc);
            backBufferSurface->Release();
            if (FAILED(res))
                return res;
            static_assert(sizeof(textureDesc) == sizeof(vr::VRVulkanTextureData_t));
            memcpy(&backBufferData->m_VulkanData, &textureDesc, sizeof(textureDesc));
            backBufferData->m_VRTexture.handle = &backBufferData->m_VulkanData;
            backBufferData->m_VRTexture.eColorSpace = vr::ColorSpace_Auto;
            backBufferData->m_VRTexture.eType = vr::TextureType_Vulkan;
            return D3D_OK;
        }

        HRESULT STDMETHODCALLTYPE UpscaleEyes() {
            return g_Game && g_Game->m_VR ? m_upscaler.Render(m_device,g_Game->m_VR) : E_FAIL;
        }

        HRESULT STDMETHODCALLTYPE GetMenuBackgroundData(SharedTextureHolder* data) {
            if(!data)return D3DERR_INVALIDCALL;
            if(!m_backgroundReady) {
                // A scene background must never alias the desktop image that
                // SteamVR is simultaneously importing as the menu overlay.
                if(!m_background&&FAILED(m_device->CreateTexture(2,2,1,D3DUSAGE_RENDERTARGET,
                    D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&m_background,nullptr)))return E_FAIL;
                if(!m_backgroundSurface&&FAILED(m_background->GetSurfaceLevel(0,&m_backgroundSurface)))return E_FAIL;
                if(FAILED(m_device->ColorFill(m_backgroundSurface,nullptr,D3DCOLOR_ARGB(255,0,0,0))))return E_FAIL;
                if(FAILED(TransferSurface(m_backgroundSurface,TRUE))||FAILED(WaitDeviceIdle()))return E_FAIL;
                m_backgroundReady=true;
                PortalVrLog("Independent immutable menu scene background ready 2x2 (desktop is overlay only)");
            }
            D3D9_TEXTURE_VR_DESC desc{};
            const auto result=GetVRDesc(m_backgroundSurface,&desc);if(FAILED(result))return result;
            static_assert(sizeof(desc)==sizeof(data->m_VulkanData));
            memcpy(&data->m_VulkanData,&desc,sizeof(desc));
            data->m_VRTexture={&data->m_VulkanData,vr::TextureType_Vulkan,vr::ColorSpace_Auto};
            return D3D_OK;
        }

        HRESULT STDMETHODCALLTYPE LockVulkanQueue() {
            if(m_queueLocked)return D3DERR_INVALIDCALL;
            m_device->Flush();
            m_device->SynchronizeCsThread(DxvkCsThread::SynchronizeAll);
            m_device->GetDXVKDevice()->lockSubmission();
            m_queueLocked=true;return D3D_OK;
        }
        HRESULT STDMETHODCALLTYPE UnlockVulkanQueue() {
            if(!m_queueLocked)return D3DERR_INVALIDCALL;
            m_device->GetDXVKDevice()->unlockSubmission();
            m_queueLocked=false;return D3D_OK;
        }
        HRESULT STDMETHODCALLTYPE BeginTextureSubmission() {
            if(m_queueLocked||!m_externalImages.empty())return D3DERR_INVALIDCALL;
            IDirect3DSurface9* desktop=nullptr;
            if(SUCCEEDED(m_device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&desktop))&&desktop) {
                AddExternalImage(desktop);desktop->Release();
            }
            auto* vr=g_Game?g_Game->m_VR:nullptr;
            if(vr&&vr->m_RenderedNewFrame&&vr->m_CreatedVRTextures) {
                const bool upscaled=vr->m_UpscaleReady&&vr->m_Graphics.upscaler!=0;
                AddExternalImage(upscaled?m_upscaler.Surface(0):vr->m_D9LeftEyeSurface);
                AddExternalImage(upscaled?m_upscaler.Surface(1):vr->m_D9RightEyeSurface);
            }
            for(const auto& entry:m_overlays)if(entry.second->dirty)AddExternalImage(entry.second->surface);
            if(m_lastExternalCount!=int(m_externalImages.size())) {
                m_lastExternalCount=int(m_externalImages.size());
                // FPS overlay dirtiness alternates this count every second.
                // Keep initial/periodic evidence without opening a log file for
                // the same image descriptors repeatedly. No session deadline.
                const auto now=GetTickCount64();
                if(++m_bridgeChanges<=8||now>=m_nextBridgeLog) {
                    m_nextBridgeLog=now+10000;
                    PortalVrLog("Vulkan bridge imageCount=%d submitLayout=TRANSFER_SRC_OPTIMAL queue=serialized restore=after-submit changes=%llu",m_lastExternalCount,m_bridgeChanges);
                    for(const auto& image:m_externalImages)PortalVrLog("Vulkan bridge image=%llu size=%ux%u samples=%u normalLayout=%d",uint64_t(image->handle()),image->info().extent.width,image->info().extent.height,unsigned(image->info().sampleCount),int(image->info().layout));
                }
            }
            // One complete wait covers render, resolves, upscaling and barriers.
            const auto waitStart=std::chrono::steady_clock::now();
            WaitDeviceIdle();
            if(vr)vr->m_LastGpuWaitMs=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-waitStart).count();
            return LockVulkanQueue();
        }
        HRESULT STDMETHODCALLTYPE EndTextureSubmission() {
            const auto result=UnlockVulkanQueue();
            for(const auto& image:m_externalImages)m_device->TransformVrImage(image,false);
            m_externalImages.clear();
            return result;
        }
        HRESULT STDMETHODCALLTYPE UploadOverlayPixels(uint64_t handle,const uint32_t* pixels,uint32_t width,uint32_t height) {
            // D3D uploads happen before BeginTextureSubmission owns the native
            // queue. Reuse each image; do not allocate a raw IPC block per edit.
            if(m_queueLocked||!handle||!pixels||!width||!height||width>4096||height>4096)return D3DERR_INVALIDCALL;
            auto found=m_overlays.find(handle);
            if(found==m_overlays.end()||found->second->width!=width||found->second->height!=height) {
                if(m_overlays.size()>=16&&found==m_overlays.end())return E_OUTOFMEMORY;
                if(m_retiredOverlays.size()>=16)return E_OUTOFMEMORY;
                auto image=std::make_unique<OverlayImage>();image->width=width;image->height=height;
                auto status=m_device->CreateTexture(width,height,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&image->texture,nullptr);
                if(FAILED(status))return status;
                status=image->texture->GetSurfaceLevel(0,&image->surface);if(FAILED(status))return status;
                if(found!=m_overlays.end())m_retiredOverlays.push_back(std::move(found->second));
                m_overlays[handle]=std::move(image);found=m_overlays.find(handle);
            }
            auto& image=*found->second;D3DLOCKED_RECT lock{};
            auto status=image.texture->LockRect(0,&lock,nullptr,D3DLOCK_DISCARD);if(FAILED(status))return status;
            const bool copied=OverlayPixels::Copy(pixels,width,height,lock.pBits,lock.Pitch);
            const auto unlocked=image.texture->UnlockRect(0);if(!copied||FAILED(unlocked))return D3DERR_INVALIDCALL;
            D3D9_TEXTURE_VR_DESC desc{};status=GetVRDesc(image.surface,&desc);if(FAILED(status))return status;
            static_assert(sizeof(desc)==sizeof(image.shared.m_VulkanData));
            memcpy(&image.shared.m_VulkanData,&desc,sizeof(desc));
            image.shared.m_VRTexture={&image.shared.m_VulkanData,vr::TextureType_Vulkan,vr::ColorSpace_Auto};
            image.dirty=true;return D3D_OK;
        }
        HRESULT STDMETHODCALLTYPE SubmitOverlayTextures() {
            auto* vr=g_Game?g_Game->m_VR:nullptr;
            if(!m_queueLocked||!vr||!vr->m_Overlay)return D3DERR_INVALIDCALL;
            for(auto& entry:m_overlays)if(entry.second->dirty) {
                const auto result=vr->m_Overlay->SetOverlayTexture(entry.first,&entry.second->shared.m_VRTexture);
                entry.second->dirty=false;vr->OverlayTextureSubmitted(entry.first,result);
            }
            return D3D_OK;
        }

        HRESULT STDMETHODCALLTYPE WorldModelDepth(BOOL enabled) {
            if(enabled){
                if(m_worldDepth)return D3DERR_INVALIDCALL;
                m_worldDepth=true;m_device->SetVrModelSceneDepth(true,enabled==ModelDepth::SpriteMode);return S_OK;
            }
            if(!m_worldDepth)return S_FALSE;m_worldDepth=false;m_device->SetVrModelSceneDepth(false);return S_OK;
        }
        HRESULT STDMETHODCALLTYPE ProfileScene(UINT marker) {
            auto* vr=g_Game?g_Game->m_VR:nullptr;if(!vr)return S_FALSE;
            GpuWorkload::Metadata meta;meta.views=vr->m_LastSecondaryViews;meta.width=vr->m_RenderWidth;meta.height=vr->m_RenderHeight;
            meta.upscaler=vr->m_Graphics.upscaler;meta.mirror=vr->m_EyeMirrorCandidate?1:vr->m_RenderWindow?2:0;
            meta.phase=!vr->m_SystemFocus.focused?2:vr->m_GraphicsOpen||g_Game->IsCursorVisible()?1:0;meta.msaa=vr->m_ActiveMsaa;
            meta.upscaleReady=vr->m_UpscaleReady;meta.outputWidth=meta.upscaleReady?vr->m_UpscaleWidth:meta.width;meta.outputHeight=meta.upscaleReady?vr->m_UpscaleHeight:meta.height;
            if(marker==0){
                m_gpuWorkload.Poll([](const GpuWorkload::Result& r){PortalVrLog("VR gpu workload phase=%s serial=%llu secondaryViews=%u scene=%ux%u output=%ux%u msaa=%d mirrorMode=%d upscaler=%d upscaleReady=%d leftGpuMs=%f rightGpuMs=%f mirrorGpuMs=%f mirrorCopyGpuMs=%f upscaleGpuMs=%f nativeResolveGpuMs=%f",r.meta.phase==0?"gameplay":r.meta.phase==1?"paused-scene":"system-focus",r.meta.serial,r.meta.views,r.meta.width,r.meta.height,r.meta.outputWidth,r.meta.outputHeight,r.meta.msaa,r.meta.mirror,r.meta.upscaler,r.meta.upscaleReady,r.left,r.right,r.mirror,r.copy,r.upscale,r.resolve);});
                const auto cadence=RenderPolicy::GpuCadence(vr->m_Graphics.detailedGpuTiming,vr->m_Graphics.autoPortalGpuTiming,vr->m_Graphics.frameTimingLog,vr->m_LastSecondaryViews);
                m_gpuWorkload.Begin(cadence!=0,meta,[&](D3DQUERYTYPE type,IDirect3DQuery9** out){return m_device->CreateQuery(type,out);},cadence);
                if(m_gpuWorkload.Failed()&&!m_gpuFailureLogged){m_gpuFailureLogged=true;PortalVrLog("GPU workload queries unavailable; profiling disabled, rendering continues unchanged");}
            }else {if(marker==3||marker==GpuWorkload::Markers-1)m_gpuWorkload.Update(meta);m_gpuWorkload.Mark(marker);}
            return m_gpuWorkload.Active()?S_OK:S_FALSE;
        }
        HRESULT STDMETHODCALLTYPE CopyEyeMirror() {
            auto* vr=g_Game?g_Game->m_VR:nullptr;
            if(!vr||!vr->m_RenderedNewFrame||!vr->m_EyeMirrorCandidate||!vr->CanReuseEyeMirror())return S_FALSE;
            const auto started=std::chrono::steady_clock::now();
            const int eye=std::clamp(vr->m_Graphics.mirrorEye,0,1);
            auto* source=eye?vr->m_D9RightEyeSurface:vr->m_D9LeftEyeSurface;
            IDirect3DSurface9* target=nullptr;D3DSURFACE_DESC src{},dst{};
            HRESULT result=m_device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&target);
            if(SUCCEEDED(result)&&target&&source&&SUCCEEDED(source->GetDesc(&src))&&SUCCEEDED(target->GetDesc(&dst))){
                const auto& bounds=vr->m_TextureBounds[eye];
                const auto layout=MirrorGeometry::Fit(src.Width,src.Height,dst.Width,dst.Height,bounds.uMin,bounds.vMin,bounds.uMax,bounds.vMax,vr->m_Graphics.mirrorFit!=0);
                if(layout.valid){
                    RECT from{layout.source.left,layout.source.top,layout.source.right,layout.source.bottom};
                    RECT to{layout.destination.left,layout.destination.top,layout.destination.right,layout.destination.bottom};
                    result=m_device->ColorFill(target,nullptr,D3DCOLOR_ARGB(255,0,0,0));
                    if(SUCCEEDED(result))result=m_device->StretchRect(source,&from,target,&to,D3DTEXF_LINEAR);
                }else result=D3DERR_INVALIDCALL;
            }else result=E_FAIL;
            if(target)target->Release();
            vr->m_LastMirrorRenderMs+=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-started).count();
            if(FAILED(result)){vr->m_EyeMirrorFailed=true;PortalVrLog("Eye mirror copy failed hr=%lx; separate desktop render resumes next frame",result);}
            static bool copyLogged=false;if(SUCCEEDED(result)&&!copyLogged){copyLogged=true;PortalVrLog("Accurate eye mirror active eye=%d source=%ux%u desktop=%ux%u fit=%d sourceMsaa=%d; eye scene and native HUD copied, SteamVR overlays separate",eye,src.Width,src.Height,dst.Width,dst.Height,vr->m_Graphics.mirrorFit,src.MultiSampleType);}
            return result;
        }
    private:
        GpuWorkload::Ring m_gpuWorkload;bool m_gpuFailureLogged=false;
        bool m_worldDepth=false;
        struct OverlayImage {
            IDirect3DTexture9* texture=nullptr;IDirect3DSurface9* surface=nullptr;
            uint32_t width=0,height=0;bool dirty=false;SharedTextureHolder shared;
            ~OverlayImage(){if(surface)surface->Release();if(texture)texture->Release();}
        };
        std::map<uint64_t,std::unique_ptr<OverlayImage>> m_overlays;
        // Preserve a resized texture while the native compositor may still own
        // its old image. This bounded list lives only for the app session.
        std::vector<std::unique_ptr<OverlayImage>> m_retiredOverlays;
        std::vector<Rc<DxvkImage>> m_externalImages;
        bool m_queueLocked=false;
        int m_lastExternalCount=-1;
        uint64_t m_bridgeChanges=0,m_nextBridgeLog=0;
        void AddExternalImage(IDirect3DSurface9* surface) {
            if(!surface)return;
            auto* texture=static_cast<D3D9Surface*>(surface)->GetCommonTexture();
            auto image=texture->Desc()->MultiSample!=D3DMULTISAMPLE_NONE?texture->GetResolveImage():texture->GetImage();
            if(image==nullptr)return;
            for(const auto& existing:m_externalImages)if(existing==image)return;
            m_device->TransformVrImage(image,true);m_externalImages.push_back(image);
        }
        IDirect3DTexture9* m_background=nullptr;
        IDirect3DSurface9* m_backgroundSurface=nullptr;
        bool m_backgroundReady=false;
        PortalFrameUpscaler m_upscaler;
        D3D9DeviceEx *m_device;
        D3D9DeviceLock m_lock;
    };

}

HRESULT __stdcall Direct3DCreateVRImpl(IDirect3DDevice9 *pDevice, IDirect3DVR9 **pInterface) {
    if (pInterface == nullptr)
        return D3DERR_INVALIDCALL;

    *pInterface = new dxvk::D3D9VR(pDevice);

    return D3D_OK;
}
