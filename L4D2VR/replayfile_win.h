#pragma once
#include <windows.h>
#include <cstdint>
#include <limits>
// Worker-only, exclusively NEW local file. No truncation, rename, deletion,
// thread/process control or runtime integration. Flush may block this worker.
namespace ReplayPose {
class NewDraftFile {
    HANDLE file=INVALID_HANDLE_VALUE;
public:
    NewDraftFile()=default;
    NewDraftFile(const NewDraftFile&)=delete;NewDraftFile& operator=(const NewDraftFile&)=delete;
    ~NewDraftFile(){Close();}
    bool Open(const wchar_t* path){
        if(file!=INVALID_HANDLE_VALUE||!path||!*path)return false;
        file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        return file!=INVALID_HANDLE_VALUE;
    }
    size_t Write(const uint8_t* data,size_t size){
        if(file==INVALID_HANDLE_VALUE||size>std::numeric_limits<DWORD>::max())return 0;
        DWORD written=0;if(!WriteFile(file,data,DWORD(size),&written,nullptr))return 0;return written;
    }
    bool Flush(){return file!=INVALID_HANDLE_VALUE&&FlushFileBuffers(file)!=FALSE;}
    bool Close(){
        if(file==INVALID_HANDLE_VALUE)return true;
        const auto old=file;file=INVALID_HANDLE_VALUE;return CloseHandle(old)!=FALSE;
    }
};
}
