#include <Windows.h>
#include <cassert>
#include <fstream>
#include <cstdio>
#include "graphicsmenupaint.h"
#include "vrsettings.h"
#include "portalpose.h"
#include "gunattachments.h"
#include "nativefit.h"
int main(int argc,char** argv) {
    using namespace VrGraphics;
    assert(argc==3);
    for(int scale=60;scale<=150;++scale) {
        const auto size=Target(1728,1728,.8592448f,.841430f,scale);
        assert(size.width%8==0 && size.height%8==0);
        assert(size.width*.8592448f>=1728*scale/100.f);
        assert(size.height*.841430f>=1728*scale/100.f);
    }
    const auto full=Target(1728,1728,.8592448f,.841430f,100);
    assert(full.width==2016 && full.height==2056);
    Settings settings;
    for(int i=0;i<100;++i) {Change(settings,0,1);Change(settings,1,1);}
    assert(settings.scale==150 && settings.msaa==8);
    for(int i=0;i<100;++i) {Change(settings,0,-1);Change(settings,1,-1);}
    assert(settings.scale==60 && settings.msaa==0);
    for(int row=0;row<Rows;++row) {
        assert(Hit(32,Top+row*RowHeight)==row);
        assert(Hit(Width-31,Top+row*RowHeight+51)==row);
        assert(Hit(31,Top+row*RowHeight+53)==-1);
    }
    assert(Hit(10,Top)==-1 && Hit(32,Top-1)==-1 && Hit(32,Top+Rows*RowHeight)==-1);
    assert(VrSettings::Parse("portal1vr_graphics")==VrSettings::Command::Graphics);
    assert(VrSettings::Parse("portal1vr_graphics;quit")==VrSettings::Command::None);
    const std::string config="Untouched=abc\r\nAntiAliasing=0 # comment\r\nAntiAliasing=4\r\n";
    const auto edited=VrSettings::SetValue(config,"AntiAliasing","2");
    assert(edited=="Untouched=abc\r\nAntiAliasing=2# comment\r\nAntiAliasing=2\r\n");
    assert(VrSettings::SetBool("Unrelated=true\n","MotionBlur",false)=="Unrelated=true\nMotionBlur=false\n");
    settings=Settings{};
    assert(Commands(settings).find("mat_motion_blur_enabled 0")!=std::string::npos);
    auto image=Paint(settings,0,"Active: 2016 x 2056 per eye, MSAA 2x","Saved. Restart Portal to apply resolution / MSAA.");
    assert(image.size()==Width*Height);
    for(auto& pixel:image) pixel=(pixel&0xff00ff00)|((pixel&255)<<16)|((pixel>>16)&255);
    BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
    header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);header.bfSize=header.bfOffBits+unsigned(image.size()*4);
    info.biSize=sizeof(info);info.biWidth=Width;info.biHeight=-Height;info.biPlanes=1;info.biBitCount=32;
    std::ofstream bitmap(argv[2],std::ios::binary);bitmap.write(reinterpret_cast<char*>(&header),sizeof(header));
    bitmap.write(reinterpret_cast<char*>(&info),sizeof(info));bitmap.write(reinterpret_cast<char*>(image.data()),image.size()*4);bitmap.close();
    std::ifstream modelFile(argv[1],std::ios::binary);
    std::vector<unsigned char> model((std::istreambuf_iterator<char>(modelFile)),{});
    assert(model.size()>248);int count,offset;memcpy(&count,model.data()+156,4);memcpy(&offset,model.data()+160,4);
    assert(count==45 && offset>0 && size_t(offset)+45*216<=model.size());
    matrix3x4_t bind[45];
    for(int i=0;i<45;++i) {matrix3x4_t inverse;memcpy(&inverse,model.data()+offset+i*216+96,sizeof(inverse));bind[i]=HandPose::InverseRigid(inverse);}
    GunAttachments::Model native;assert(native.Read(model.data(),model.size(),bind,true) && native.hasBarrel);
    const auto unscaled=HandPose::FitGunToPalm(bind[24],true);assert(!memcmp(&unscaled,&bind[24],sizeof(unscaled)));
    auto source=bind[24];for(int r=0;r<3;++r)source[r][3]=bind[8][r][3];
    unsigned cases=0;
    for(float pitch:{-80.f,0.f,80.f}) for(float yaw:{-160.f,0.f,160.f}) for(float roll:{-150.f,0.f,150.f}) {
        const auto controller=NativeFit::Gun(PortalPose::Frame({120,-75,60},{pitch,yaw,roll}),17,-23,31);
        const auto gun=HandPose::Reanchor(bind[24],source,controller);
        for(int i=1;i<=native.count;++i) {
            matrix3x4_t actual;assert(native.Resolve(i,controller,bind,actual));
            const auto& a=native.attachments[i-1];
            const auto reference=HandPose::Concat(HandPose::Reanchor(bind[a.bone],bind[24],gun),a.local);
            for(int r=0;r<3;++r)for(int c=0;c<4;++c)assert(std::abs(actual[r][c]-reference[r][c])<.0001f);
            ++cases;
        }
    }
    matrix3x4_t visible[45];memcpy(visible,bind,sizeof(visible));settings.arms=2;settings.gun=true;
    NativeFit::Visibility(visible,45,true,false,settings);
    for(int i=24;i<45;++i)assert(!memcmp(&visible[i],&bind[i],sizeof(bind[i])));
    for(int i=0;i<24;++i)for(int r=0;r<3;++r)for(int c=0;c<3;++c)assert(visible[i][r][c]==0);
    printf("PASS: %u native gun attachment cases, optical resolution coverage, settings, panel hit regions, CPU-only menu raster\n",cases);
}
