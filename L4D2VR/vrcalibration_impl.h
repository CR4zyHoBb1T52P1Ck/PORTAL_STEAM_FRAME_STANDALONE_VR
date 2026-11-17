#pragma once
bool VR::ApplyBodyMeasurements(const BodyCalibration::Measurements& m) {
    if(!BodyCalibration::Valid(m)){m_GraphicsStatus="Invalid body measurements. Check span, width and shoulder drop.";return false;}
    const auto previous=m_Graphics;
    m_Graphics.bodyEyeCM=m.eyeCM;m_Graphics.bodySpanCM=m.spanCM;m_Graphics.bodyWidthCM=m.widthCM;m_Graphics.bodyDropCM=m.dropCM;
    m_Graphics.bodyFit=true;m_Graphics.bodyIK=true;m_Graphics.body=true;m_Graphics.controllerHands=true;m_Graphics.bodyArmScale=1;
    if(!SaveGraphicsSettings()){m_Graphics=previous;m_GraphicsStatus="Save failed. Previous body fit preserved.";return false;}
    m_GraphicsApplyPending=true;m_BodyCapture.Cancel();
    m_GraphicsStatus="Body fit saved. Shoulder width is estimated; wrist offsets remain adjustable.";
    PortalVrLog("Body calibration saved eyeCM=%f spanCM=%f shoulderCM=%f dropCM=%f",m.eyeCM,m.spanCM,m.widthCM,m.dropCM);
    MenuHaptic(0,true);MenuHaptic(1,true);return true;
}
float VR::StandingEyeHeightCM() {
    if(!m_System||!m_Poses[0].bPoseIsValid)return m_Graphics.bodyEyeCM;
    auto pose=MenuMatrix(m_Poses[0].mDeviceToAbsoluteTracking);const auto space=vr::VRCompositor()->GetTrackingSpace();
    if(space==vr::TrackingUniverseSeated)pose=HandPose::Concat(MenuMatrix(m_System->GetSeatedZeroPoseToStandingAbsoluteTrackingPose()),pose);
    else if(space==vr::TrackingUniverseRawAndUncalibrated)pose=HandPose::Concat(MenuMatrix(m_System->GetRawZeroPoseToStandingAbsoluteTrackingPose()),pose);
    return pose[1][3]*100;
}
matrix3x4_t VR::GetHolsteredGunFrame() {
    Vector f=m_HmdForward;f.z=0;if(!TwoHandAim::Unit(f))f={1,0,0};const auto r=CrossProduct(f,{0,0,1});
    const auto& g=m_Graphics;const auto p=HolsterCenter(m_HolsteredShoulder)+(f*g.backGunX+r*g.backGunY+Vector(0,0,g.backGunZ))*(m_VRScale/100);
    return NativeFit::Gun(HandPose::Frame(r,-f,{0,0,-1},p),g.backGunPitch,g.backGunYaw,g.backGunRoll);
}
void VR::UpdateBodyCalibration() {
    if(!m_BodyCapture.active)return;
    const auto now=GetTickCount64();const float dt=m_CalibrationPrevious?float(now-m_CalibrationPrevious)/1000:.016f;m_CalibrationPrevious=now;
    const bool left=MenuDigital(m_Input,m_MenuTriggers[0]),right=MenuDigital(m_Input,m_MenuTriggers[1]);
    Vector points[3]{};bool valid=m_ActionStateFresh&&m_SystemFocus.Allowed();
    vr::HmdMatrix34_t standing{};bool convert=false;
    const auto space=vr::VRCompositor()->GetTrackingSpace();
    if(space==vr::TrackingUniverseSeated){standing=m_System->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();convert=true;}
    else if(space==vr::TrackingUniverseRawAndUncalibrated){standing=m_System->GetRawZeroPoseToStandingAbsoluteTrackingPose();convert=true;}
    const vr::TrackedDeviceIndex_t ids[]={0,m_System->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand),m_System->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand)};
    for(int i=0;i<3;++i) {
        if(ids[i]>=vr::k_unMaxTrackedDeviceCount||!m_Poses[ids[i]].bPoseIsValid){valid=false;continue;}
        auto pose=MenuMatrix(m_Poses[ids[i]].mDeviceToAbsoluteTracking);if(convert)pose=HandPose::Concat(MenuMatrix(standing),pose);
        const auto p=TwoHandAim::Position(pose);points[i]={-p.z,-p.x,p.y};
    }
    auto head=MenuMatrix(m_Poses[0].mDeviceToAbsoluteTracking);if(convert)head=HandPose::Concat(MenuMatrix(standing),head);
    const auto front=-TwoHandAim::Axis(head,2);const Vector forward(-front.z,-front.x,front.y);
    const bool captured=m_BodyCapture.Step(dt,points[0],points[1],points[2],forward,valid,left&&right,left||right);
    const int progress=m_BodyCapture.review?10:int(m_BodyCapture.elapsed*10);
    if(progress!=m_CalibrationProgress||captured) {
        m_CalibrationProgress=progress;m_GraphicsDirty=true;
        if(captured) {
            const auto& m=m_BodyCapture.measured;
            m_GraphicsStatus="Captured: eyes "+VrMenu::Number(m.eyeCM)+" cm, span "+VrMenu::Number(m.spanCM)+" cm. Select Apply to save.";
            MenuHaptic(0,true);MenuHaptic(1,true);ReleaseMenuInput();
            m_MenuTriggerHeld[0]=left;m_MenuTriggerHeld[1]=right;m_MenuSuppressUntil=now+350;
        } else m_GraphicsStatus=m_BodyCapture.elapsed>0?"Hold both triggers and stay still: "+std::to_string(progress*10)+"%":"Arms at shoulder height. Release, then hold BOTH triggers. VIEW cancels.";
    }
}
Vector VR::HolsterCenter(int physical) {
    Vector forward=m_HmdForward;forward.z=0;if(!TwoHandAim::Unit(forward))forward={1,0,0};
    const auto right=CrossProduct(forward,{0,0,1});
    const auto head=m_SetupOrigin+(m_6DOF?m_HmdPosRelative:Vector(0,0,0))+Vector(0,0,HeightOffset())+m_CameraCollisionOffset;
    const auto& g=m_Graphics;
    return HolsterFit::Center(head,forward,right,physical,g.shoulderWidthCM,g.shoulderBackCM,g.shoulderDownCM,
        physical?Vector(g.holsterRightX,g.holsterRightY,g.holsterRightZ):Vector(g.holsterLeftX,g.holsterLeftY,g.holsterLeftZ),m_VRScale/100);
}
float VR::HolsterRadius(int physical){return m_Graphics.shoulderRadiusCM*m_VRScale/100*(physical?m_Graphics.holsterRightSize:m_Graphics.holsterLeftSize)/100;}
void VR::UpdateHolsterReachTest() {
    if(!m_GraphicsOpen||!m_Graphics.holsterReachTest||m_BodyCapture.active){m_HolsterFitHover[0]=m_HolsterFitHover[1]=false;return;}
    for(int physical=0;physical<2;++physical) {
        const bool logicalLeft=(physical==0)!=m_LeftHanded;
        const bool valid=m_ActionStateFresh&&m_HmdPose.isValid&&(logicalLeft?m_LeftControllerPose.isValid:m_RightControllerPose.isValid);
        const auto p=logicalLeft?GetLeftControllerAbsPos():GetRightControllerAbsPos();
        const bool inside=valid&&(p-HolsterCenter(physical)).LengthSqr()<=powf(HolsterRadius(physical)*(m_HolsterFitHover[physical]?1.2f:1.f),2);
        const auto now=GetTickCount64();
        if(inside&&!m_HolsterFitHover[physical]&&now-m_HolsterFitPulse[physical]>=500){m_HolsterFitPulse[physical]=now;ControllerHaptic(physical,m_Graphics.grabHapticStrength,m_Graphics.hapticDurationMS/1000,m_Graphics.hapticFrequency,"holster-fit-test");}
        m_HolsterFitHover[physical]=inside;
    }
}
