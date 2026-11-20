// Native Windows bootstrap only: no graphics, VR, hooks or runtime deadline.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

static std::wstring Quote(const std::wstring& value) {
    std::wstring out=L"\"";unsigned slashes=0;
    for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}
        if(c==L'\"')out.append(slashes*2+1,L'\\');else out.append(slashes,L'\\');
        slashes=0;out+=c;}
    out.append(slashes*2,L'\\');return out+L"\"";
}
static void Log(const std::wstring& path,const std::string& text) {
    HANDLE f=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f!=INVALID_HANDLE_VALUE){DWORD written;WriteFile(f,text.data(),DWORD(text.size()),&written,nullptr);CloseHandle(f);}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(!argv)return 111;
    std::vector<std::wstring> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);LocalFree(argv);
    if(argc>=2&&args[1]==L"--probe-child")return 0;
    if(argc<3||args[1]!=L"--portal-root")return 112;
    const std::wstring root=args[2],exe=root+L"\\hl2.exe";
    if(GetFileAttributesW(exe.c_str())==INVALID_FILE_ATTRIBUTES)return 113;
    const bool probe=argc==4&&args[3]==L"--self-test";
    wchar_t module[32768]{};GetModuleFileNameW(nullptr,module,32768);
    const std::wstring self(module),folder=self.substr(0,self.find_last_of(L"\\/"));
    const auto log=folder+L"\\relay-events.log";
    std::wstring command=Quote(probe?self:exe);
    if(probe)command+=L" --probe-child";
    else for(int i=3;i<argc;++i)command+=L" "+Quote(args[i]);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    Log(log,probe?"CPU self-test: spawning harmless relay child in game cwd; Portal not launched.\n":"User launch: spawning owned Portal; no automatic deadline.\n");
    if(!CreateProcessW((probe?self:exe).c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,root.c_str(),&startup,&child)){
        Log(log,"CreateProcess failed error="+std::to_string(GetLastError())+"\n");return 114;}
    CloseHandle(child.hThread);
    // Wait only observes. No Job kill-on-exit, timeout, TerminateProcess or
    // destructor cleanup. Steam/user owns the game lifetime.
    const auto waited=WaitForSingleObject(child.hProcess,INFINITE);DWORD code=115;
    if(waited==WAIT_OBJECT_0)GetExitCodeProcess(child.hProcess,&code);
    CloseHandle(child.hProcess);
    Log(log,std::string(probe?"CPU self-test":"Portal process")+" exited code="+std::to_string(code)+"\n");
    return int(code);
}
