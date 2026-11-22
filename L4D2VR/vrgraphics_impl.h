#pragma once
#include "vrmenus_impl.h"
#include "visualtrial.h"
void VR::ImageBaseline(bool restore) {
    char directory[MAX_PATH]{};if(!GetRuntimeBaseDirectory(directory,sizeof(directory)))return;
    const auto path=std::filesystem::path(directory)/"VR"/"image-comparison-baseline.txt";
    const auto temporary=path.parent_path()/"image-comparison-baseline.tmp";
    try {
        if(std::filesystem::is_symlink(path)||std::filesystem::is_symlink(temporary))throw std::runtime_error("linked baseline path preserved");
        if(restore){
            if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>512)throw std::runtime_error("Save an image baseline first");
            std::ifstream input(path,std::ios::binary);const std::string text((std::istreambuf_iterator<char>(input)),{});
            VisualTrial::Baseline baseline;if(!VisualTrial::Baseline::Read(text,baseline))throw std::runtime_error("invalid image baseline preserved");
            const auto previous=m_Graphics;baseline.Apply(m_Graphics);
            if(!SaveGraphicsSettings()){m_Graphics=previous;throw std::runtime_error("settings save failed; current settings kept");}
            m_GraphicsApplyPending=true;m_GraphicsStatus="Image baseline restored. Restart Portal when ready.";
        }else{
            const auto baseline=VisualTrial::Baseline::Capture(m_Graphics);if(!baseline.Valid())throw std::runtime_error("current image settings are invalid");
            std::ofstream output(temporary,std::ios::binary|std::ios::trunc);output<<baseline.Text();output.close();
            if(!output||!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("baseline save failed");
            m_GraphicsStatus="Image baseline saved. Calibration and interactions are unchanged.";
        }
    }catch(const std::exception& error){m_GraphicsStatus=error.what();PortalVrLog("Image baseline operation failed: %s",error.what());}
    m_GraphicsDirty=true;
}
void VR::SampleFrameDiagnostics() {
    if(!m_Graphics.frameTimingLog){m_FrameTimingPrevious=0;m_FrameTimingStart=0;m_FrameTimingWindow.Clear();return;}
    // This is an aggregation interval, never an application/session deadline.
    const auto now=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    const int phase=!m_SystemFocus.focused?4:!m_Game->IsInGame()?0:!m_RenderedNewFrame?1:(m_GraphicsOpen||m_Game->IsCursorVisible())?2:3;
    const char* names[]={"menu-no-scene","loading-no-eye","paused-scene","gameplay","system-focus"};
    const auto report=[&]() {
        auto& window=m_FrameTimingWindow;if(window.frames.empty()||m_FrameTimingPhase<0)return;
        vr::Compositor_FrameTiming timing{};timing.m_nSize=sizeof(timing);
        const bool available=vr::VRCompositor()->GetFrameTiming(&timing);
        const auto& left=m_VKLeftEye.m_VulkanData;const auto& right=m_VKRightEye.m_VulkanData;
        PortalVrLog("VR frame timing phase=%s samples=%u meanMs=%f p95Ms=%f fps=%f eyeCpuMeanMs=%f gpuWaitMeanMs=%f compositorGpuLastMs=%f left=%ux%u right=%ux%u upscaleReady=%d output=%ux%u secondaryViewsMean=%f maxViewDepth=%u bodyCpuMeanMs=%f avatarRebasesMean=%f avatarCacheHitsMean=%f mirrorCpuMeanMs=%f",
            names[m_FrameTimingPhase],unsigned(window.frames.size()),window.Mean(),window.P95(),1000/window.Mean(),window.eye/window.frames.size(),window.wait/window.frames.size(),
            available?timing.m_flPreSubmitGpuMs:-1,left.m_nWidth,left.m_nHeight,right.m_nWidth,right.m_nHeight,m_UpscaleReady,
            m_UpscaleReady?m_VKUpscaledLeft.m_VulkanData.m_nWidth:left.m_nWidth,m_UpscaleReady?m_VKUpscaledLeft.m_VulkanData.m_nHeight:left.m_nHeight,window.secondary/window.frames.size(),window.maxViewDepth,window.body/window.frames.size(),window.rebases/window.frames.size(),window.cacheHits/window.frames.size(),window.mirror/window.frames.size());
        const char* groupNames[]={"0-2","3-4","5plus"};
        for(int i=0;i<3;++i){const auto& group=window.groups[i];if(!group.samples)continue;
            PortalVrLog("VR view workload phase=%s secondaryBand=%s samples=%u meanMs=%f eyeCpuMeanMs=%f mirrorCpuMeanMs=%f idleWaitMeanMs=%f bodyCpuMeanMs=%f over72Pct=%f over90Pct=%f",names[m_FrameTimingPhase],groupNames[i],group.samples,group.frame/group.samples,group.eye/group.samples,group.mirror/group.samples,group.wait/group.samples,group.body/group.samples,100.*group.over72/group.samples,100.*group.over90/group.samples);
        }
    };
    if(phase!=m_FrameTimingPhase) {
        report();m_FrameTimingWindow.Clear();m_FrameTimingPhase=phase;m_FrameTimingPrevious=0;m_FrameTimingStart=now;
        PortalVrLog("VR render phase=%s",names[phase]);
    }
    const bool scene=phase==2||phase==3;
    if(m_FrameTimingPrevious)m_FrameTimingWindow.Add(float(now-m_FrameTimingPrevious)/1000,scene?m_LastEyeRenderMs:0,m_LastGpuWaitMs,scene?m_LastSecondaryViews:0,scene?m_LastViewDepth:0,scene?m_LastAvatarRebases:0,scene?m_LastAvatarCacheHits:0,scene?m_LastBodyBuildMs:0,scene?m_LastMirrorRenderMs:0);
    m_FrameTimingPrevious=now;if(!m_FrameTimingStart)m_FrameTimingStart=now;
    if(now-m_FrameTimingStart>=2000000){report();m_FrameTimingWindow.Clear();m_FrameTimingStart=now;}
}
void VR::SamplePerformance() {
    SampleFrameDiagnostics();
    if(!m_PerfStart || !m_SystemFocus.focused || !m_Game->IsInGame() || m_GraphicsOpen || m_Game->IsCursorVisible() || !m_RenderedNewFrame) {m_PerfPrevious=0;return;}
    const auto now=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    if(m_PerfStart==1) {m_PerfStart=now;PortalVrLog("VR benchmark started width=%u height=%u msaa=%d mirror=%u upscaler=%d output=%ux%u",m_RenderWidth,m_RenderHeight,m_ActiveMsaa,m_RenderWindow,m_Graphics.upscaler,m_UpscaleWidth,m_UpscaleHeight);}
    if(m_PerfPrevious) {const float delta=float(now-m_PerfPrevious)/1000;m_PerfIntervals.push_back(delta);m_PerfElapsedMs+=delta;}
    m_PerfPrevious=now;
    vr::Compositor_FrameTiming timing{};timing.m_nSize=sizeof(timing);
    if(vr::VRCompositor()->GetFrameTiming(&timing) && std::isfinite(timing.m_flPreSubmitGpuMs) && timing.m_flPreSubmitGpuMs>0)
        m_PerfGpu.push_back(timing.m_flPreSubmitGpuMs);
    if(m_PerfElapsedMs<30000 || m_PerfIntervals.empty()) return;
    std::sort(m_PerfIntervals.begin(),m_PerfIntervals.end());
    double total=0,gpu=0;for(float ms:m_PerfIntervals) total+=ms;for(float ms:m_PerfGpu)gpu+=ms;
    const double mean=total/m_PerfIntervals.size();
    const float p95=m_PerfIntervals[size_t((m_PerfIntervals.size()-1)*.95)];
    PortalVrLog("VR benchmark result samples=%u width=%u height=%u msaa=%d mirror=%u upscaler=%d upscaleReady=%d meanMs=%f p95Ms=%f fps=%f gpuReportedMs=%f gpuSamples=%u eyeCpuLastMs=%f mirrorCpuLastMs=%f idleLastMs=%f",
        unsigned(m_PerfIntervals.size()),m_RenderWidth,m_RenderHeight,m_ActiveMsaa,m_RenderWindow,m_Graphics.upscaler,m_UpscaleReady,mean,p95,1000/mean,
        m_PerfGpu.empty() ? -1 : gpu/m_PerfGpu.size(),unsigned(m_PerfGpu.size()),m_LastEyeRenderMs,m_LastMirrorRenderMs,m_LastGpuWaitMs);
    m_GraphicsStatus="Sample saved in automatic log. Mean "+std::to_string(int(1000/mean))+" FPS.";
    m_PerfStart=m_PerfPrevious=0;m_GraphicsDirty=true;
}
