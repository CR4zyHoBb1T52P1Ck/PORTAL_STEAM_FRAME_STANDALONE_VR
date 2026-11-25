// Offline shader compilation only. This program creates no graphics device.
#include <windows.h>
#include <d3dcompiler.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <iomanip>
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    const auto pending=std::filesystem::path(argv[2]).wstring()+L".pending";
    std::ofstream header(pending);if(!header)return 3;
    header<<"#pragma once\n// Generated offline by tools/frame/compile-upscalers.cpp; ps_3_0.\n";
    const char* symbols[]={"PortalFrameSGSR","PortalFrameSGSREdge","PortalFrameBilinear"};
    for(int mode=0;mode<3;++mode) {
        D3D_SHADER_MACRO macros[]={{"SGSR_EDGE_DIRECTION",mode==1?"1":"0"},{"BILINEAR_ONLY",mode==2?"1":"0"},{nullptr,nullptr}};
        ID3DBlob *code=nullptr,*errors=nullptr,*assembly=nullptr;
        const auto source=std::filesystem::path(argv[1]);
        const auto result=D3DCompileFromFile(source.c_str(),macros,D3D_COMPILE_STANDARD_FILE_INCLUDE,"main","ps_3_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
        if(errors){std::cerr.write((char*)errors->GetBufferPointer(),errors->GetBufferSize());errors->Release();}
        if(FAILED(result)||!code)return 4;
        if(FAILED(D3DDisassemble(code->GetBufferPointer(),code->GetBufferSize(),0,nullptr,&assembly)))return 5;
        const auto path=std::filesystem::path(argv[3])/(std::string(symbols[mode])+".asm");
        std::ofstream output(path,std::ios::binary);output.write((char*)assembly->GetBufferPointer(),assembly->GetBufferSize());
        header<<"alignas(4) inline constexpr unsigned char "<<symbols[mode]<<"[]={\n";
        auto bytes=(unsigned char*)code->GetBufferPointer();
        for(size_t i=0;i<code->GetBufferSize();++i) {
            header<<"0x"<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(bytes[i])<<",";
            if(i%16==15)header<<"\n";
        }
        header<<"\n};\n";std::cout<<symbols[mode]<<" bytes="<<std::dec<<code->GetBufferSize()<<"\n";
        code->Release();assembly->Release();
    }
    header.close();if(!header)return 6;
    return MoveFileExW(pending.c_str(),std::filesystem::path(argv[2]).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?0:7;
}

