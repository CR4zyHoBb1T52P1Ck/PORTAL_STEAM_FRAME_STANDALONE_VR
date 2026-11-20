#pragma once
void VR::CreatorAction(int action) {
    const auto head=GetViewOrigin(m_SetupOrigin);const auto angle=QAngle(m_HmdAngAbs.x,m_HmdAngAbs.y,m_HmdAngAbs.z);
    if(action==20){m_CreatorCamera.Reset(head,angle);m_GraphicsStatus="Creator camera reset at headset.";return;}
    if(action>=30&&action<40){
        if(!m_CreatorCamera.ready)m_CreatorCamera.Reset(head,angle);
        if(action<36){const int n=action-30;m_CreatorCamera.Move(n==0?1:n==1?-1:0,n==2?-1:n==3?1:0,n==4?1:n==5?-1:0,m_VRScale,m_Graphics.creatorStepCM/100);}
        else m_CreatorCamera.Turn(action==36?-5:action==37?5:0,action==38?-5:action==39?5:0);
        m_Graphics.creatorCamera=3;m_Graphics.mirror=true;SaveGraphicsSettings();m_GraphicsApplyPending=true;m_GraphicsStatus="Free spectator camera moved.";return;
    }
    char directory[MAX_PATH]{};if(!GetRuntimeBaseDirectory(directory,sizeof(directory)))return;
    const auto portal=std::filesystem::path(directory).parent_path()/"portal";
    std::string command;
    if(action==0){
        if(m_CreatorRecording){m_GraphicsStatus="Already recording; stop the current take first.";return;}
        m_CreatorTake="portalvr_take_"+std::to_string(std::time(nullptr))+"_"+std::to_string(GetTickCount64());
        command="record "+m_CreatorTake;m_CreatorRecording=true;m_GraphicsStatus="Demo recording requested: "+m_CreatorTake;
    }else if(action==1){if(!m_CreatorRecording){m_GraphicsStatus="No creator recording is active.";return;}command="stop";m_CreatorRecording=false;m_GraphicsStatus="Recording stopped; gameplay continues.";}
    else if(action==2){
        if(m_CreatorRecording){m_GraphicsStatus="Stop recording before selecting playback.";return;}
        try{std::filesystem::file_time_type newest{};bool found=false;for(const auto& item:std::filesystem::directory_iterator(portal)){
            const auto name=item.path().stem().string();if(item.is_regular_file()&&item.path().extension()==".dem"&&CreatorCamera::TakeName(name)){
                const auto time=item.last_write_time();if(!found||time>newest){newest=time;m_CreatorTake=name;found=true;}}}
            if(!found){m_GraphicsStatus="No portalvr_take_*.dem recordings found.";return;}
        }catch(...){m_GraphicsStatus="Could not read the local demo library.";return;}
        command="demo_quitafterplayback 0;playdemo "+m_CreatorTake;m_CreatorReplay=true;m_GraphicsStatus="Playing native world demo: "+m_CreatorTake;
    }else if(action==3||action==4||action==5||action==6||action==7){
        if(!m_CreatorReplay&&action!=6){m_GraphicsStatus="Select a recording before using replay controls.";return;}
        command=action==3?"demo_pause":action==4?"demo_resume":action==5?"demo_timescale "+VrMenu::Number(m_Graphics.replaySpeed):action==6?"demoui":"stopdemo";
        if(action==7)m_CreatorReplay=false;m_GraphicsStatus="Native replay command requested.";
    }
    if(!command.empty()){PortalVrLog("Creator command=%s",command.c_str());m_Game->ClientCmd_Unrestricted(command.c_str());
        if(action==2||action==6){CloseVrMenus(true);return;}}
}
void VR::CreatorMirror(Vector& origin,QAngle& angles,float& fov) {
    const auto& g=m_Graphics;const double now=GetTickCount64()/1000.;
    if(!g.creatorCamera)return;
    if(!m_CreatorCamera.ready)m_CreatorCamera.Reset(origin,angles);
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    if(g.creatorCamera==3&&g.creatorKeyboard&&pid==GetCurrentProcessId()&&m_SystemFocus.Allowed()&&!m_GraphicsOpen&&!m_Game->IsCursorVisible()){
        static double previous=0;const float dt=previous?float(std::clamp(now-previous,0.,.05)):0;previous=now;
        auto key=[](int k){return (GetAsyncKeyState(k)&0x8000)?1.f:0.f;};
        m_CreatorCamera.Move(key('W')-key('S'),key('D')-key('A'),key('E')-key('Q'),m_VRScale,dt*(key(VK_SHIFT)>0?3.f:1.f));
        m_CreatorCamera.Turn((key(VK_RIGHT)-key(VK_LEFT))*dt*60,(key(VK_DOWN)-key(VK_UP))*dt*60);
    }
    m_CreatorCamera.View(g.creatorCamera,origin,angles,m_VRScale,g.creatorDistance,g.creatorHeight,g.creatorOrbit,now,origin,angles);fov=g.creatorFov;
}
