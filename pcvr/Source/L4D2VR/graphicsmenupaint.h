#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include "graphicssettings.h"
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"user32.lib")
namespace VrGraphics {
inline std::vector<uint32_t> Paint(const Settings& settings,int selected,const std::string& current,const std::string& status) {
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=Width;info.bmiHeader.biHeight=-Height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(nullptr);void* pixels=nullptr;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!dc || !bitmap || !pixels) {if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return {};}
    const auto old=SelectObject(dc,bitmap);
    auto fill=[&](RECT rect,COLORREF color) {HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&rect,brush);DeleteObject(brush);};
    fill({0,0,Width,Height},RGB(18,24,32));SetBkMode(dc,TRANSPARENT);
    HFONT font=CreateFontA(-28,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");
    const auto oldFont=SelectObject(dc,font);
    auto text=[&](int x,int y,const std::string& value,COLORREF color=RGB(235,240,246)) {
        SetTextColor(dc,color);TextOutA(dc,x,y,value.c_str(),int(value.size()));};
    text(36,24,"PORTAL FRAME  /  VR GRAPHICS",RGB(80,195,255));
    text(36,68,current);text(36,110,"Point and click < or >. Changes save automatically.");
    text(36,148,"Resolution and MSAA apply on the next launch.",RGB(255,202,100));
    static const char* names[]={"Eye render resolution","Edge anti-aliasing","Texture quality","Anisotropic filtering","Motion blur","Bloom","Dynamic shadows","Desktop mirror","FPS counter","Spatial upscaling","Performance comparison","Back"};
    for(int row=0;row<Rows;++row) {
        const int y=Top+row*RowHeight;
        fill({30,y,Width-30,y+52},row==selected ? RGB(38,78,103) : RGB(31,41,53));
        text(48,y+10,names[row]);
        if(row<10) {text(550,y+10,"<",RGB(80,195,255));text(1115,y+10,">",RGB(80,195,255));}
        text(610,y+10,Value(settings,row));
    }
    text(36,920,"Select a row with the stick; left/right adjusts it.");
    text(36,962,status,RGB(160,220,180));
    text(36,1006,"Back closes this panel. Your save is kept.");
    GdiFlush();
    auto* data=static_cast<uint32_t*>(pixels);
    std::vector<uint32_t> result(data,data+Width*Height);
    // SetOverlayRaw expects RGBA; a Windows DIB is BGRA and has no alpha.
    for(auto& p:result) p=0xff000000u|((p&255)<<16)|(p&0xff00)|((p>>16)&255);
    SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    return result;
}
}
