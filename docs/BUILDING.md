# Building Interaction-26

Initialize dxvk at the submodule commit 75167aa2859fe7118ae683e717b97303ad379efa,
then apply patches/dxvk-frame-all.patch from the repository root with
`git -C dxvk apply ../patches/dxvk-frame-all.patch`.
Build l4d2vr.sln Release/x86 with MSVC, Windows SDK and the expected DirectX SDK
(`DXSDK_DIR`). Development used v145 and frame-build.props, /Z7, no linker PDB.
For example, from a configured x86 VS shell:
`msbuild l4d2vr.sln /p:Configuration=Release /p:Platform=x86 /p:PlatformToolset=v145 /p:ForceImportAfterCppTargets=frame-build.props /p:PostBuildEventUseInBuild=false`.
Do not let the legacy project post-build copy deploy into a live game.

The source ZIP has no retail fixtures or release DLL. Use the runtime ZIP for
installation. tests/interaction26.cpp needs your own extracted Chell model path
as its first argument. No recording or private model fixture is distributed.
Portable launcher checks: `python -m unittest discover -s tests -p "test_*.py"`.
