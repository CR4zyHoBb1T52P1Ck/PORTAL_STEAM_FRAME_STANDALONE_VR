# Building the PCVR10 native source

Run `pcvr/Build-PCVR.cmd` (or `Build-PCVR.cmd` from the dedicated source ZIP). Requires Visual Studio C++ desktop/x86 tools, MSBuild, Windows SDK and the expected DirectX SDK headers/library environment (`DXSDK_DIR` where needed). Development qualification uses MSVCv145/x86; an explicit toolset may be passed as the first argument (for example v143). Older toolchains are not newly qualified.

This PCVR source snapshot includes the qualified DXVK source and generated shader headers directly. Do NOT apply the older repository-root Frame patch on top of it. The legacy root Frame source/submodule remains separate history. MinHook compiles from its included C sources; no private prebuilt MinHook library is needed. Import libraries and their upstream licenses are retained.

Build/temp output stays below this source folder. Post-build deployment is explicitly disabled: the script never installs into or launches Portal. The actual link output is **Build/bin/d3d9.dll**, even if MSBuild's preexisting TargetPath warning names QualifiedBin. Check actual file identity. Different toolchains/paths/timestamps can change a rebuilt DLL hash; the runtime download contains the exact owner-tested DLL, not the qualification rebuild.

The source download omits the runtime payload and all retail/generated game meshes. Download the runtime ZIP for installation. Local asset generation lives in Launcher/tools/frame and requires your owned game. Match any locally rebuilt DLL to a matching runtime/package before choosing to deploy; close the game yourself first.
