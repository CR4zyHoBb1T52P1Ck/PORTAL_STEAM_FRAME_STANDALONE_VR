@echo off
setlocal
set "TEMP=%~dp0Temp"
set "TMP=%~dp0Temp"
if not exist "%TEMP%" mkdir "%TEMP%"
set "_CL_=/Z7"
if defined VSINSTALLDIR goto configured
set "PCVR_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%PCVR_VSWHERE%" exit /b 2
for /f "usebackq tokens=*" %%i in (`"%PCVR_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PCVR_VS=%%i"
if not defined PCVR_VS exit /b 3
call "%PCVR_VS%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if not "%errorlevel%"=="0" exit /b 4
:configured
set "PCVR_TOOLSET=v143"
if "%VisualStudioVersion:~0,2%"=="18" set "PCVR_TOOLSET=v145"
if not "%~1"=="" set "PCVR_TOOLSET=%~1"
msbuild "%~dp0Source\l4d2vr.sln" /m:1 /nologo /p:Configuration=Release /p:Platform=x86 /p:PlatformToolset=%PCVR_TOOLSET% /p:MultiProcessorCompilation=false /p:PostBuildEventUseInBuild=false /p:TrackFileAccess=false "/p:OutDir=%~dp0Build/QualifiedBin/" "/p:IntDir=%~dp0Build/QualifiedObj/" "/p:ForceImportAfterCppTargets=%~dp0Build.props"
exit /b %errorlevel%
