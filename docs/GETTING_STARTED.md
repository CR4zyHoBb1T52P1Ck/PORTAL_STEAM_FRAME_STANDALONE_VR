# PORTAL_STEAM_FRAME_STANDALONE_VR
The goal is simple. Portal in your pocket. Or backpack.... Hardcase? Whatever. I wanted Portal VR on the go. So here it is.

## v0.1.0-beta.1 — first public standalone beta

Portal running locally in full VR on Steam Frame: tracked head and controllers,
native Chell hands/body, portal gun, two-hand aiming, shoulder holstering,
hand switching, body calibration and a movable VR settings panel.
This is the **Physics-16 development build**, published November 17, 2026.
It is a community mod and is not affiliated with Valve.

**This is a beta. A crash was reported in the latest playtest and remains under
investigation.** Not all new physics features were tested. Use the game's save
system regularly. Portal traversal was observed in the logs; funneling and all
momentum/carry combinations are not yet comprehensively qualified. No 72/90 FPS
guarantee is made. Performance varies with scene, settings and recording.

Download **Portal-Frame-VR-v0.1.0-beta.1.zip** from
[Releases](https://github.com/CR4zyHoBb1T52P1Ck/PORTAL_STEAM_FRAME_STANDALONE_VR/releases).
The source checkout is for development; the release ZIP includes the compiled DLL.

## Requirements and install

- An owned Steam **Windows x86 version of Portal**, including its `hl2` and
  `portal` directories. The Linux native game binary is not the mod's target.
- Steam Frame's standalone SteamVR/OpenVR environment and a Windows compatibility
  runtime supporting x86 on ARM64. Development used Proton 11 ARM64/FEX.
- Python 3.11+ for the installer; only the standard library is needed.
- Enough space for the game and backups. No retail game data is distributed here.

1. Close Portal yourself and extract the release ZIP outside the game directory.
2. Run the installer against the Windows Portal folder. On Windows:

   ```powershell
   py -3 Install.py --portal-dir "D:\SteamLibrary\steamapps\common\Portal" --dry-run
   py -3 Install.py --portal-dir "D:\SteamLibrary\steamapps\common\Portal"
   ```

   On Frame/Linux, use `python3` and the actual copied Windows game folder:

   ```sh
   python3 Install.py --portal-dir "/path/to/Portal" --dry-run
   python3 Install.py --portal-dir "/path/to/Portal"
   ```

   The installer verifies the package, generates the gun/hand model overrides
   from your own retail VPKs, and preserves existing settings and comments.
   Changed files are backed up under `.portal-frame-backups` in the game folder.
   It never starts or terminates Portal. A failed write attempts rollback.
3. If installing on your PC, transfer your patched Windows game folder to Frame
   using your Steam developer/devkit transfer workflow. This repository does not
   include Steam Frame runtime installation or a universal one-click deployment.
4. Add/select the copied `hl2.exe` as a **VR** Steam shortcut on Frame. Set its
   working directory to the game folder and force the ARM64 Proton/FEX runtime.
   Start from the following launch options:

   ```sh
   WINEDLLOVERRIDES="d3d9=n,b" SteamAppId=400 SteamGameId=400 %command% -steam -game portal -insecure -window -novid -width 1280 -height 720 +mat_queue_mode 0 +mat_vsync 0 +mat_antialias 0
   ```

   The desktop window is not the eye render resolution. Adjust eye resolution
   through the VR settings. Existing working Frame shortcut/runtime settings
   should be kept. The headset must run the game locally; PC streaming is not
   required for standalone play.
5. A devkit launch can generate `steam_appid.txt` containing `400`. If the next
   shortcut launch fails, back up that file outside the game directory and retry.
   Preserve an unexpected value and investigate. `tools/frame/appid_guard.py`
   contains the checked archive operation used during development.

The installer accepts the verified retail asset layout and stops on unsupported
assets. It does not modify saves or retail VPKs. To roll back, close Portal,
restore previous files from the timestamped backup's `files` directory, and use
`installation.json` to identify mod files that were newly created. Do not remove
your saves or unrelated custom mods.

## Controls and setup

- Triggers: blue/orange portals. Gun-hand grip: native use/grab.
- Left stick: movement; right stick: turning. Left-handed controls are selectable.
- Right A: jump; right B: crouch. Menu/View controls are shown in the bindings.
- **Click both thumbsticks** to bring the mod menu in front of you.
- Point and trigger to select; grip and drag to move panels; corners resize.
  Grip plus stick adjusts depth; grip plus D-pad slides the panel.
- Support grip under the gun enables two-handed aiming. Grip near a shoulder
  stows/draws the gun. Hold both grips and release the old hand to transfer it.
- Use **Calibration, tuning and comfort** for standing T-pose setup or manual
  measurements, hand alignment, body shape, shoulder zones and comfort settings.
- Visual resolution/MSAA changes require restarting Portal. Save in-game first.
  Automatic menu restart requires the per-launch Frame restart helper; without
  that integration, exit and relaunch manually. No automatic test deadline exists.

## Experimental world interactions

Home > **World interactions (beta)** contains three independent features:

- Visual hand collision: palms stop at surfaces, with optional flat poses and
  touch haptics. A deliberate finger squeeze overrides the automatic pose.
- Hand-held physics carry: near-hand acquisition and six-axis carry through
  Portal's native physics shadow controller. Native float carry remains available.
- Controller throws: sampled linear/angular release velocity, speed cap and spin.

All three default **off** in this public beta. Enable individually. Stow the gun
to try hand contact/carry. There is one native held object; dual independent
grabs, pushing unheld props with palms and a fully simulated avatar are not
implemented. Legs and shoulders are inferred from head/controllers.

The rigid fitted torso fix is enabled for calibrated bodies, with fixed torso
length and camera clearance controls. Portal gun/back model and portal previews
were confirmed in prior headset testing. Physics-16 still needs further headset
qualification, including portal momentum/funneling and contact edge cases.

## Logs and reporting

The runtime writes local silent diagnostics to `portalvr.log` in the game working directory (a Proton log
may also be enabled by your launch configuration). No personal log uploader or
background scheduled task is installed. This release includes no device IPs,
SSH keys, personal recordings, saves or private test logs.
Report reproduction steps, settings, runtime version and the relevant final log
lines in GitHub Issues. Review your logs for personal paths before posting.

## Source and build

Current C++ runtime source and asset generators are included in this repository.
The exact shipped DLL SHA-256 is:

```text
98bb961699b3f174cb356a30f726f5abd40a2d713a52efa9bf2b1734ef4e36dd
```

The DXVK dependency is pinned to `75167aa2859fe7118ae683e717b97303ad379efa`
from [fholger/dxvk_l4d2vr](https://github.com/fholger/dxvk_l4d2vr).
Initialize the submodule, then apply `patches/dxvk-frame-all.patch` to that clean
revision. The combined patch contains the local Frame/D3D9 changes. Build the
x86 Release configuration of `l4d2vr.sln` with Visual Studio C++ tools, Windows
SDK and the DirectX SDK headers/libraries expected by the project (`DXSDK_DIR`).
Development used toolset v145, `/Z7`, and `frame-build.props` to suppress linker
debug output. Shader source/embedded output lives with the runtime source.
Retail model fixtures are intentionally absent; tests that require them must
receive paths to your own extracted assets. This is not a clean-room engine.

## Credits

Built on [BowmanFox/portal1vr](https://github.com/BowmanFox/portal1vr),
[Gistix/portal2vr](https://github.com/Gistix/portal2vr),
[sd805/l4d2vr](https://github.com/sd805/l4d2vr), and
[fholger/dxvk_l4d2vr](https://github.com/fholger/dxvk_l4d2vr).
Valve supplies Portal/SteamVR/OpenVR; other dependencies include MinHook and
Qualcomm Snapdragon Game Super Resolution. See **THIRD_PARTY_NOTICES.md** and
the bundled licenses. Existing copyright/license terms remain in effect.
