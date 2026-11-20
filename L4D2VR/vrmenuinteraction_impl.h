#pragma once
void VR::UpdateMenuChrome(int width,int height) {
    if(!m_Overlay)return;
    if(!m_Game->IsCursorVisible()||!m_Graphics.menuGrab){if(m_MenuChrome)m_Overlay->HideOverlay(m_MenuChrome);return;}
    if(!m_MenuChrome){m_Overlay->CreateOverlay("portal.frame.menu.controls","Portal menu corner controls",&m_MenuChrome);if(!m_MenuChrome)return;
        m_Overlay->SetOverlayInputMethod(m_MenuChrome,vr::VROverlayInputMethod_None);m_Overlay->SetOverlaySortOrder(m_MenuChrome,1101);}
    if(width!=m_MenuChromeWidth||height!=m_MenuChromeHeight) {
        m_MenuChromePixels.assign(width*height,0);
        for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
            const int dx=std::min(x,width-1-x),dy=std::min(y,height-1-y);
            if((dx<42&&dy<7)||(dy<42&&dx<7)||(x>width*.4f&&x<width*.6f&&y<7))m_MenuChromePixels[y*width+x]=0xffffc74b;
        }
        if(UploadOverlayFile(m_MenuChrome,m_MenuChromePixels,width,height,"menu-corners.tga")!=vr::VROverlayError_None)return;
        m_MenuChromeWidth=width;m_MenuChromeHeight=height;
    }
    const auto transform=OpenVrMatrix(m_MenuTransform);
    m_Overlay->SetOverlayTransformAbsolute(m_MenuChrome,vr::VRCompositor()->GetTrackingSpace(),&transform);
    m_Overlay->SetOverlayWidthInMeters(m_MenuChrome,m_Graphics.menuWidth);
    m_Overlay->SetOverlayFlag(m_MenuChrome,vr::VROverlayFlags_SortWithNonSceneOverlays,false);
        m_Overlay->SetOverlayFlag(m_MenuChrome,vr::VROverlayFlags_VisibleInDashboard,false);
    m_Overlay->ShowOverlay(m_MenuChrome);
}
void VR::UpdateMenuPointer(int hand,const Vector& source,const Vector& destination,bool hit,int panel) {
    if(!m_Overlay)return;
    if(!m_Graphics.menuLaser){if(m_MenuBeam[hand])m_Overlay->HideOverlay(m_MenuBeam[hand]);if(m_MenuDot[hand])m_Overlay->HideOverlay(m_MenuDot[hand]);return;}
    const auto origin=vr::VRCompositor()->GetTrackingSpace();
    if(!m_MenuBeam[hand]) {
        const std::string key="portal.frame.laser."+std::to_string(hand);
        m_Overlay->CreateOverlay(key.c_str(),"Portal menu pointer",&m_MenuBeam[hand]);
        if(m_MenuBeam[hand]) {
            const std::vector<uint32_t> color{0xfff2c832};UploadOverlayFile(m_MenuBeam[hand],color,1,1,hand?"laser-right.tga":"laser-left.tga");
            m_Overlay->SetOverlayInputMethod(m_MenuBeam[hand],vr::VROverlayInputMethod_None);
            m_Overlay->SetOverlayFlag(m_MenuBeam[hand],vr::VROverlayFlags_SortWithNonSceneOverlays,false);
        m_Overlay->SetOverlayFlag(m_MenuBeam[hand],vr::VROverlayFlags_VisibleInDashboard,false);
            m_Overlay->SetOverlaySortOrder(m_MenuBeam[hand],1300);
        }
    }
    const Vector head=MenuPose::Position(MenuMatrix(m_Poses[0].mDeviceToAbsoluteTracking));
    const float length=std::sqrt((destination-source).LengthSqr());
    if(m_MenuBeam[hand]&&length>.01f) {
        const auto pose=OpenVrMatrix(MenuPose::Beam(source,destination,head));
        m_Overlay->SetOverlayTransformAbsolute(m_MenuBeam[hand],origin,&pose);
        m_Overlay->SetOverlayWidthInMeters(m_MenuBeam[hand],length);
        m_Overlay->SetOverlayTexelAspect(m_MenuBeam[hand],length/.0025f);
        m_Overlay->SetOverlayColor(m_MenuBeam[hand],hit?.4f:.2f,hit?1.f:.7f,1);
        m_Overlay->SetOverlayAlpha(m_MenuBeam[hand],.85f);m_Overlay->ShowOverlay(m_MenuBeam[hand]);
    }
    if(!hit){if(m_MenuDot[hand])m_Overlay->HideOverlay(m_MenuDot[hand]);return;}
    if(!m_MenuDot[hand]) {
        const std::string key="portal.frame.cursor."+std::to_string(hand);
        m_Overlay->CreateOverlay(key.c_str(),"Portal menu cursor",&m_MenuDot[hand]);
        if(m_MenuDot[hand]) {
            std::vector<uint32_t> pixels(32*32,0);
            for(int y=0;y<32;++y)for(int x=0;x<32;++x){const float r=std::hypot(x-15.5f,y-15.5f);if(r<14)pixels[y*32+x]=r>10?0xff000000:0xffffffff;}
            UploadOverlayFile(m_MenuDot[hand],pixels,32,32,hand?"cursor-right.tga":"cursor-left.tga");
            m_Overlay->SetOverlayInputMethod(m_MenuDot[hand],vr::VROverlayInputMethod_None);
            m_Overlay->SetOverlayFlag(m_MenuDot[hand],vr::VROverlayFlags_SortWithNonSceneOverlays,false);
        m_Overlay->SetOverlayFlag(m_MenuDot[hand],vr::VROverlayFlags_VisibleInDashboard,false);
            m_Overlay->SetOverlaySortOrder(m_MenuDot[hand],1301);m_Overlay->SetOverlayWidthInMeters(m_MenuDot[hand],.018f);
        }
    }
    if(m_MenuDot[hand]) {
        auto pose=MenuPanelTransform(panel);MenuPose::Translate(pose,destination+MenuPose::Axis(pose,2)*.003f);
        const auto transform=OpenVrMatrix(pose);m_Overlay->SetOverlayTransformAbsolute(m_MenuDot[hand],origin,&transform);m_Overlay->ShowOverlay(m_MenuDot[hand]);
    }
}
void VR::UpdateMenuInteraction() {
    if(!m_Overlay || (!m_GraphicsOpen&&!m_Game->IsCursorVisible()))return;
    const auto now=GetTickCount64();const float dt=m_MenuLastUpdate ? std::clamp(float(now-m_MenuLastUpdate)/1000,.001f,.05f):.016f;m_MenuLastUpdate=now;
    if(!m_MenuWasVisible){ReleaseMenuInput();SetMenuPlacement();m_MenuSuppressUntil=now+250;m_MenuWasVisible=true;}
    if(!m_MenuPoseValid||(m_Graphics.menuFollow&&m_MenuGrabHand<0))SetMenuPlacement(m_MenuLayoutFocused);
    if(!m_MenuPoseValid)return;
    int widths[2]={0,VrMenu::Width},heights[2]={0,VrMenu::Height};GetOverlayWindowSize(*this,widths[0],heights[0]);
    if(widths[0]<=0||heights[0]<=0)return;
    const float ratios[2]={float(heights[0])/widths[0],float(heights[1])/widths[1]};
    for(auto overlay:{m_MainMenuHandle,m_GraphicsHandle})if(overlay)m_Overlay->SetOverlayFlag(overlay,vr::VROverlayFlags_MakeOverlaysInteractiveIfVisible,false);
    UpdateMenuChrome(widths[0],heights[0]);
    MenuPose::Hit hits[2],panelHits[2][2];int hitPanels[2]={-1,-1};matrix3x4_t hands[2];bool valid[2]{},trigger[2]{},grip[2]{};
    vr::InputAnalogActionData_t sticks[2]{};
    const auto rows=VrMenu::Rows(m_Graphics,m_MenuPage);
    int active=-1;
    for(int hand=0;hand<2;++hand) {
        const auto role=hand?vr::TrackedControllerRole_RightHand:vr::TrackedControllerRole_LeftHand;
        const auto device=m_System->GetTrackedDeviceIndexForControllerRole(role);
        trigger[hand]=MenuDigital(m_Input,m_MenuTriggers[hand]);grip[hand]=MenuDigital(m_Input,m_MenuGrips[hand]);sticks[hand]=MenuAnalog(m_Input,m_MenuSticks[hand]);
        if(device>=vr::k_unMaxTrackedDeviceCount||!m_Poses[device].bPoseIsValid){if(m_MenuBeam[hand])m_Overlay->HideOverlay(m_MenuBeam[hand]);if(m_MenuDot[hand])m_Overlay->HideOverlay(m_MenuDot[hand]);continue;}
        valid[hand]=true;hands[hand]=MenuMatrix(m_Poses[device].mDeviceToAbsoluteTracking);
        if(device!=m_MenuTipDevice[hand]||now-m_MenuTipRefresh[hand]>2000){m_MenuTip[hand]=GetControllerTipMatrix(role);m_MenuTipDevice[hand]=device;m_MenuTipRefresh[hand]=now;}
        const auto aim=HandPose::Concat(hands[hand],MenuMatrix(m_MenuTip[hand]));
        const Vector source=MenuPose::Position(aim),direction=-MenuPose::Axis(aim,2);
        for(int panel=0;panel<2;++panel)if(panel?m_GraphicsOpen:m_Game->IsCursorVisible())
            panelHits[hand][panel]=MenuPose::Intersect(MenuPanelTransform(panel),MenuPanelWidth(panel),ratios[panel],source,direction,widths[panel],heights[panel]);
        hitPanels[hand]=MenuPose::Nearest(panelHits[hand][0],panelHits[hand][1]);
        if(hitPanels[hand]>=0)hits[hand]=panelHits[hand][hitPanels[hand]];
        UpdateMenuPointer(hand,source,hits[hand].hit?hits[hand].point:source+direction*1.5f,hits[hand].hit,std::max(0,hitPanels[hand]));
        if(hits[hand].hit && (active<0||hand==m_MenuHoverHand))active=hand;
        if(hits[hand].hit&&trigger[hand]&&!m_MenuTriggerHeld[hand])active=hand;
    }
    if(m_MenuMouseHand>=0) {
        if(!trigger[m_MenuMouseHand]||!valid[m_MenuMouseHand]){MenuMouse(false);m_MenuMouseHand=-1;}
        else {active=m_MenuMouseHand;hitPanels[active]=0;hits[active]=panelHits[active][0];}
    }
    const bool dpad[]={MenuDigital(m_Input,m_MenuDpad[0]),MenuDigital(m_Input,m_MenuDpad[1]),MenuDigital(m_Input,m_MenuDpad[2]),MenuDigital(m_Input,m_MenuDpad[3])};
    if(m_Graphics.menuGrab)for(int hand=0;hand<2;++hand) {
        if(m_MenuGrabHand<0 && valid[hand] && grip[hand]&&!m_MenuGripHeld[hand] && hits[hand].hit && now>=m_MenuSuppressUntil) {
            m_MenuGrabHand=hand;m_MenuGrabPanel=hitPanels[hand];m_MenuFocusPanel=m_MenuGrabPanel;
            m_MenuGrabCorner=MenuPose::Corner(hits[hand].x,hits[hand].y,widths[m_MenuGrabPanel],heights[m_MenuGrabPanel]);
            m_PreviousGrabPose=hands[hand];m_MenuGrabWidth=MenuPanelWidth(m_MenuGrabPanel);
            m_MenuGrabRadius=std::max(.05f,std::sqrt((hits[hand].point-MenuPose::Position(MenuPanelTransform(m_MenuGrabPanel))).LengthSqr()));
            MenuHaptic(hand,true);MenuMouse(false);m_MenuMouseHand=-1;
        }
    }
    if(m_MenuGrabHand>=0) {
        const int hand=m_MenuGrabHand;
        if(!valid[hand]||!grip[hand]||!m_Graphics.menuGrab) {
            m_MenuGrabHand=-1;m_MenuGrabCorner=-1;
            if(!SaveGraphicsSettings())m_GraphicsStatus="Menu moved; saving its size/depth failed.";
        } else {
            auto& pose=MenuPanelTransform(m_MenuGrabPanel);auto& panelWidth=MenuPanelWidth(m_MenuGrabPanel);
            if(m_MenuGrabCorner>=0) {
                const auto aim=HandPose::Concat(hands[hand],MenuMatrix(m_MenuTip[hand]));
                const auto point=MenuPose::Intersect(pose,10,ratios[m_MenuGrabPanel],MenuPose::Position(aim),-MenuPose::Axis(aim,2),widths[m_MenuGrabPanel],heights[m_MenuGrabPanel]);
                if(point.distance>0){const float radius=std::sqrt((point.point-MenuPose::Position(pose)).LengthSqr());
                    panelWidth=std::clamp(m_MenuGrabWidth*radius/m_MenuGrabRadius,.5f,2.5f);}
            } else pose=MenuPose::Drag(pose,m_PreviousGrabPose,hands[hand]);
            m_PreviousGrabPose=hands[hand];
            const Vector head=MenuPose::Position(MenuMatrix(m_Poses[0].mDeviceToAbsoluteTracking));
            MenuPose::Depth(pose,head,NativeFit::Axis(sticks[hand].y,.2f)*dt);
            const Vector slide=(MenuPose::Axis(pose,0)*float(int(dpad[3])-int(dpad[2]))
                +MenuPose::Axis(pose,1)*float(int(dpad[0])-int(dpad[1])))*(.4f*dt);
            MenuPose::Translate(pose,MenuPose::Position(pose)+slide);
            if(!m_MenuGrabPanel)m_Graphics.menuDepth=std::clamp(std::sqrt((MenuPose::Position(pose)-head).LengthSqr()),.4f,4.f);
            ApplyMenuPlacement();
        }
    } else if(now>=m_MenuSuppressUntil) {
        const bool back=PressedDigitalAction(m_MenuBack,true)||PressedDigitalAction(m_Pause,true);
        if(back){MenuMouse(false);m_MenuMouseHand=-1;if(m_GraphicsOpen&&m_MenuFocusPanel==1){if(m_MenuPage==VrMenu::Home)CloseVrMenus(false);else{m_MenuPage=VrMenu::Parent(m_MenuPage);m_GraphicsSelected=0;m_GraphicsDirty=true;}}else MenuKey(VK_ESCAPE);}
        if(active>=0) {
            const int hand=active;const auto& hit=hits[hand];
            const int panel=hitPanels[hand];if(hits[hand].hit)m_MenuFocusPanel=panel;
            if(panel==1&&m_GraphicsOpen&&m_MenuRaster.AcceptsClicks(int(m_MenuPage))) {
                const int row=VrMenu::Hit(hit.x,hit.y,int(rows.size()));
                if(row>=0 && row!=m_MenuHoveredRow){m_GraphicsSelected=row;m_GraphicsDirty=true;MenuHaptic(hand,false);}
                m_MenuHoveredRow=row;
                if(trigger[hand]&&!m_MenuTriggerHeld[hand]) {
                    MenuHaptic(hand,true);
                    if(hit.y<65&&hit.x>1100)CloseVrMenus(false);
                    else if(row>=0)ChangeGraphicsSetting(row,hit.x<600?-1:1);
                }
            } else if(panel==0) {
                if(hit.hit||(m_MenuMouseHand==hand&&hit.distance>0))
                    m_Game->SetCursorPos(std::clamp(int(hit.x),0,widths[0]-1),std::clamp(int(hit.y),0,heights[0]-1));
                if(auto* input=m_Game->GetVguiInput()) {
                    const auto panel=input->GetMouseOver();
                    if(panel&&panel!=m_NativeMenuHoverPanel)MenuHaptic(hand,false);
                    m_NativeMenuHoverPanel=panel;
                }
                if(hit.hit&&trigger[hand]&&!m_MenuTriggerHeld[hand]){PortalVrLog("Native menu click physical=%d x=%f y=%f hover=%u",hand,hit.x,hit.y,m_NativeMenuHoverPanel);MenuMouse(true);m_MenuMouseHand=hand;MenuHaptic(hand,true);}
                if(!trigger[hand]&&m_MenuTriggerHeld[hand]&&m_MenuMouseHand==hand){MenuMouse(false);m_MenuMouseHand=-1;}
            }
        }
        if(now>=m_MenuNavNext) {
            int direction=dpad[0]?-1:dpad[1]?1:0;bool adjusted=false;
            if(m_GraphicsOpen&&m_MenuFocusPanel==1) {
                if(direction){m_GraphicsSelected=(m_GraphicsSelected+direction+int(rows.size()))%int(rows.size());m_GraphicsDirty=true;MenuHaptic(active>=0?active:0,false);adjusted=true;}
                if(dpad[2]||dpad[3]){ChangeGraphicsSetting(m_GraphicsSelected,dpad[2]?-1:1);MenuHaptic(0,true);adjusted=true;}
                if(active<0&&PressedDigitalAction(m_MenuSelect,true)){ChangeGraphicsSetting(m_GraphicsSelected,1);MenuHaptic(1,true);}
            } else {
                for(int i=0;i<4;++i)if(dpad[i]){static const WORD keys[]={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT};MenuKey(keys[i]);MenuHaptic(0,false);adjusted=true;}
                if(active<0&&PressedDigitalAction(m_MenuSelect,true)){MenuKey(VK_RETURN);MenuHaptic(1,true);}
            }
            if(adjusted)m_MenuNavNext=now+220;
        }
    }
    // Mouse-up is unconditional: a hand can leave a menu between press and
    // release. Neither hovering nor switching panels may leave Source stuck.
    for(int hand=0;hand<2;++hand){if(m_MenuMouseHand==hand&&(!trigger[hand]||!valid[hand])){MenuMouse(false);m_MenuMouseHand=-1;}m_MenuTriggerHeld[hand]=trigger[hand];m_MenuGripHeld[hand]=grip[hand];}
    m_MenuHoverHand=active;
}
void VR::UpdateComfortVignette() {
    if(!m_Overlay)return;
    const auto walk=MenuAnalog(m_Input,ResolveAction(m_ActionWalk)),turn=MenuAnalog(m_Input,ResolveAction(m_ActionTurn));
    const bool moving=(m_Graphics.movement&&(std::abs(walk.x)>.2f||std::abs(walk.y)>.2f))||(m_Graphics.turning&&std::abs(turn.x)>.25f);
    const float target=m_Graphics.vignette&&moving&&!m_GraphicsOpen&&!m_Game->IsCursorVisible()&&m_Game->IsInGame()?m_Graphics.vignetteStrength:0;
    m_MenuVignetteAlpha+=std::clamp(target-m_MenuVignetteAlpha,-.06f,.06f);
    if(m_MenuVignetteAlpha<.005f){if(m_VignetteHandle)m_Overlay->HideOverlay(m_VignetteHandle);return;}
    if(!m_VignetteHandle) {
        m_Overlay->CreateOverlay("portal.frame.comfort","Portal movement comfort",&m_VignetteHandle);if(!m_VignetteHandle)return;
        std::vector<uint32_t> pixels(256*256);
        for(int y=0;y<256;++y)for(int x=0;x<256;++x){const float radius=std::hypot((x-127.5f)/127.5f,(y-127.5f)/127.5f);pixels[y*256+x]=uint32_t(std::clamp((radius-.38f)/.47f,0.f,1.f)*255)<<24;}
        UploadOverlayFile(m_VignetteHandle,pixels,256,256,"comfort-vignette.tga");m_Overlay->SetOverlayWidthInMeters(m_VignetteHandle,1.7f);
        m_Overlay->SetOverlaySortOrder(m_VignetteHandle,100);
        m_Overlay->SetOverlayFlag(m_VignetteHandle,vr::VROverlayFlags_SortWithNonSceneOverlays,false);
        m_Overlay->SetOverlayFlag(m_VignetteHandle,vr::VROverlayFlags_VisibleInDashboard,false);
        m_Overlay->SetOverlayInputMethod(m_VignetteHandle,vr::VROverlayInputMethod_None);
        const vr::HmdMatrix34_t pose={1,0,0,0,0,1,0,0,0,0,1,-.6f};
        m_Overlay->SetOverlayTransformTrackedDeviceRelative(m_VignetteHandle,vr::k_unTrackedDeviceIndex_Hmd,&pose);
    }
    m_Overlay->SetOverlayAlpha(m_VignetteHandle,m_MenuVignetteAlpha);m_Overlay->ShowOverlay(m_VignetteHandle);
}
void VR::UpdateFpsOverlay() {
    if(!m_Overlay)return;
    if(!m_Graphics.showFps||!m_Game->IsInGame()||m_Game->IsCursorVisible()||m_GraphicsOpen){if(m_FpsHandle)m_Overlay->HideOverlay(m_FpsHandle);m_FpsLastUpdate=0;m_FpsFrames=0;m_FpsImageReady=false;return;}
    const auto now=GetTickCount64();++m_FpsFrames;
    if(!m_FpsLastUpdate){m_FpsLastUpdate=now;return;}
    if(now-m_FpsLastUpdate<1000){if(m_FpsHandle&&m_FpsImageReady)m_Overlay->ShowOverlay(m_FpsHandle);return;}
    const float fps=m_FpsFrames*1000.f/float(now-m_FpsLastUpdate);m_FpsLastUpdate=now;m_FpsFrames=0;
    if(!m_FpsHandle){m_Overlay->CreateOverlay("portal.frame.fps","Portal application FPS",&m_FpsHandle);if(!m_FpsHandle)return;
        m_Overlay->SetOverlayInputMethod(m_FpsHandle,vr::VROverlayInputMethod_None);m_Overlay->SetOverlayWidthInMeters(m_FpsHandle,.27f);
        m_Overlay->SetOverlaySortOrder(m_FpsHandle,101);m_Overlay->SetOverlayFlag(m_FpsHandle,vr::VROverlayFlags_SortWithNonSceneOverlays,false);
        m_Overlay->SetOverlayFlag(m_FpsHandle,vr::VROverlayFlags_VisibleInDashboard,false);
        const vr::HmdMatrix34_t pose={1,0,0,.42f,0,1,0,.28f,0,0,1,-.85f};m_Overlay->SetOverlayTransformTrackedDeviceRelative(m_FpsHandle,0,&pose);}
    BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),320,-80,1,32,BI_RGB};
    HDC dc=CreateCompatibleDC(nullptr);void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!dc||!bitmap||!pixels){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return;}
    auto old=SelectObject(dc,bitmap);RECT rect{0,0,320,80};FillRect(dc,&rect,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    HFONT font=CreateFontA(-37,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");
    auto oldFont=SelectObject(dc,font);SetTextColor(dc,RGB(230,245,255));SetBkMode(dc,TRANSPARENT);
    char text[64];sprintf_s(text,"App FPS: %.1f",fps);TextOutA(dc,12,15,text,int(strlen(text)));GdiFlush();
    auto* data=static_cast<uint32_t*>(pixels);m_FpsPixels.assign(data,data+320*80);
    for(auto& p:m_FpsPixels)p=0xff000000u|((p&255)<<16)|(p&0xff00)|((p>>16)&255);
    SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    // Repeated raw pixel uploads exhausted the Frame vrclient's shared-memory
    // pool (201 outstanding blocks in the owner crash log). Send a native file
    // path instead; two files keep an image stable while the next is decoded.
    static unsigned slot=0;const auto name="fps-counter-"+std::to_string(slot++%2)+".tga";
    const auto upload=UploadOverlayFile(m_FpsHandle,m_FpsPixels,320,80,name.c_str());
    static unsigned failures=0;
    if(upload!=vr::VROverlayError_None){if(failures++%30==0)PortalVrLog("FPS overlay upload failed=%d; counter hidden until recovery",upload);m_FpsImageReady=false;m_Overlay->HideOverlay(m_FpsHandle);return;}failures=0;m_FpsImageReady=true;
    m_Overlay->SetOverlayAlpha(m_FpsHandle,.85f);m_Overlay->ShowOverlay(m_FpsHandle);
}
