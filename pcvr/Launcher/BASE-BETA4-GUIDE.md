# PORTAL_STEAM_FRAME_STANDALONE_VR
The goal is simple. Portal in your pocket. Or backpack.... Hardcase? Whatever. I wanted Portal VR on the go. So here it is.

## v0.1.0-beta.4 â€” EMERGENCY DAY ONE WOOPS PATCH

Portal running locally in full VR on Steam Frame: tracked head and controllers,
native Chell hands/body, portal gun, two-hand aiming, shoulder holstering,
hand switching, body calibration and a movable VR settings panel.
This is the **Embodiment-23 development build**, with the standalone-launch emergency patch published November 20, 2026.
It is a community mod and is not affiliated with Valve.

**This is a beta. The captured body-lighting crash path has been fixed. A later
headset power-off remains unexplained.** Not all new physics features were tested. Use the game's save
system regularly. Portal traversal was observed in the logs; funneling and all
momentum/carry combinations are not yet comprehensively qualified. No 72/90 FPS
guarantee is made. Performance varies with scene, settings and recording.

Download **Portal-Frame-VR-v0.1.0-beta.4.zip** from
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
3. If installing on your PC, transfer the patched **Windows** game folder to
   Frame using your existing developer/devkit transfer workflow. Transfer this
   extracted release folder too. Runtime/Proton/FEX installation and game
   transfer are prerequisites; these tools do not install those components.
4. On Frame, from the extracted release directory, prepare its local launcher:

   ```sh
   python3 FrameSetup.py --portal-dir "/actual/path/to/Portal" --dry-run
   python3 FrameSetup.py --portal-dir "/actual/path/to/Portal"
   ```

   Replace the path with your copied Windows Portal directory. The tool prints
   an **executable**, **working directory** and full **launch options** and saves
   them in `Steam-Shortcut-Settings.txt` beside the game in its setup folder.
   Keep that setup folder on Frame. It contains the launcher, not a PC service.
5. Edit your existing Portal VR non-Steam shortcut, or add the printed
   `portal-standalone.exe` as a new non-Steam shortcut. Set its executable and
   working directory to the printed values. Mark it as VR / include it in the
   VR library, select your working **ARM64 Proton/FEX** compatibility runtime
   (development used Proton11 ARM64), and paste the **entire printed launch
   options line** into Launch Options. Keep `%command%` exactly as printed.

   **Target the relay executable and its separate launch folder, not hl2.exe
   or the game folder.** The new command includes `-dx9` and
   `WINEDLLOVERRIDES="d3d9=n,b"`; both are needed for this mod. Do not add
   `SteamAppId=400` / `SteamGameId=400` overrides from the older instructions.
   The launcher starts the real game from its correct working directory.
6. Press Play **in the Frame library**. Once these local files/runtime and the
   shortcut are configured, the PC can be disconnected. Local logs are silent,
   and there is no automatic game/test deadline. Quit in-game or use Steam Stop
   when you decide to finish. Visual-resource changes can use the mod menu's
   Restart Portal button; missing restart identity falls back to manual restart.

### Upgrading from beta.1–beta.3

Close Portal yourself, run the new `Install.py` and `FrameSetup.py`, then update
the shortcut as above. **A DLL-only update leaves the old launch bug intact.**
Existing config, calibration and saves are preserved. Fresh installs receive
the tuned gun grab point: right -2cm, up -3cm, forward -3.5cm. Existing users can
choose those values in the gun grab menu without replacing their other settings.

The launch prefix archives only Portal's expected generated `400` app-ID marker
before the game starts. An unexpected marker is preserved and reported in the
local launch log. You should not have to manually remove this file each run.

The installer accepts the verified retail asset layout and stops on unsupported
assets. It does not modify saves or retail VPKs. To roll back, close Portal,
restore previous files from the timestamped backup's `files` directory, and use
`installation.json` to identify mod files that were newly created. Do not remove
your saves or unrelated custom mods.

## Controls and setup

- Triggers: blue/orange portals. Either-hand grip: native use/grab; one held prop at a time.
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
grabs and a fully simulated avatar are not implemented. Optional hand/gun
impacts are available under Physical contact. Legs and shoulders are inferred from head/controllers.

The rigid fitted torso fix is enabled for calibrated bodies, with fixed torso
length and camera clearance controls. Portal gun/back model and portal previews
were confirmed in prior headset testing. This beta still needs further headset
qualification, including portal momentum/funneling and contact edge cases.

## Logs and reporting

The runtime writes local silent diagnostics to `portalvr.log` in the game working directory (a Proton log
may also be enabled by your launch configuration). Frame-local launcher records
are stored in the printed setup folder under `Standalone-Launch/Attempts`.
They preserve startup errors and process exit codes without showing consoles. No personal log uploader or
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

## Embodiment-23 controls and known limits

World interactions > Physical contact contains hand/gun push toggles and force
controls. Its subpages tune separate impact gains and gun wall clearance.
Prop grab > Palm carry and object hand pose controls snap, offset and curl.
Home > Crosshair and trigger colors keeps physical trigger colors consistent.
Calibration, tuning and comfort > Body shape contains chest/hip width/depth;
the owner could not find those sliders during the headset test, so menu
discoverability remains a known issue. Cosmetic jiggle is subtle and is being
reworked. Independent two-prop carry and hand-driven jiggle are not included.
Portal-heavy scenes can run slower than ordinary rooms;71/72/90FPS is not
guaranteed. See RELEASE_NOTES.md for owner observations and validation limits.
