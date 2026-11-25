# Install The Heavy Petting Update — Contact-55

Use the runtime ZIP from [Releases](https://github.com/CR4zyHoBb1T52P1Ck/PORTAL_STEAM_FRAME_STANDALONE_VR/releases/tag/v0.1.0-beta.5-contact55).
The source ZIP is for building. Read [release notes](../RELEASE_NOTES.md) and the
[detailed progress/known-issue/hiatus account](HEAVY_PETTING_PROGRESS.md).

## Requirements

- Owned **Windows x86 Steam Portal**, including `hl2.exe`, `hl2/` and `portal/`.
  The native Linux executable is not this mod's target.
- On Frame: working SteamVR/OpenVR and ARM64 Proton/FEX. These scripts do not
  install that runtime or transfer the game. Use your compatible working setup;
  development used Proton11ARM64/FEX.
- Python3.11+ standard library; Windows PCVR GUI also requires Tk.
- Space for one game, the small overlay and changed-file backups. No retail
  game data, textures, personal settings, saves or recordings are distributed.

## Install the mod

Close Portal yourself. Extract the entire runtime ZIP outside the game folder.

Windows example:

```powershell
py -3 Install.py --portal-dir "D:\SteamLibrary\steamapps\common\Portal" --dry-run
py -3 Install.py --portal-dir "D:\SteamLibrary\steamapps\common\Portal"
```

Frame/Linux example, pointing at a copied **Windows** game:

```sh
python3 Install.py --portal-dir "/path/to/Portal" --dry-run
python3 Install.py --portal-dir "/path/to/Portal"
```

The installer verifies checksums, generates gun/hand model overrides and gun-light
occlusion data from owned VPKs, preserves existing config/comments and saves,
and adds missing generic defaults. Changed files/journal are retained under
`.portal-frame-backups/` inside the game. Dry run validates using temporary
generated files and leaves the game unchanged. No app is launched or stopped.

## Configure standalone launch on Frame — required after every DLL update

If installed on PC, transfer the patched Windows game and extracted release to
Frame using your existing devkit workflow. Run on Frame from the release folder:

```sh
python3 FrameSetup.py --portal-dir "/actual/path/to/Portal" --dry-run
python3 FrameSetup.py --portal-dir "/actual/path/to/Portal"
```

For an existing installation retain its setup location explicitly:

```sh
python3 FrameSetup.py --portal-dir "/actual/path/to/Portal" --setup-dir "/actual/path/to/existing/setup"
```

Use ALL three printed Steam shortcut values: executable, working directory and
complete launch options. They are saved in `Steam-Shortcut-Settings.txt` in the
separate setup folder. Edit the existing non-Steam shortcut or add its printed
`portal-standalone.exe`, select your working ARM64 Proton runtime, and mark it
as VR. A useful label is **Portal VR — Heavy Petting / Contact-55**. Put it in
your VR collection; this public installer does not edit collections automatically.

Do not target hl2.exe directly. Do not place a retail steam_appid.txt beside the
relay. Keep the setup folder on Frame. The relay/script/profile retain the
standalone launch repair and validate the DLL. An old profile will refuse the
new DLL: **Install.py plus FrameSetup.py are both required**, even from beta.4
or the nightly. After setup, play needs no PC, Wi-Fi or background PC service.

Start Portal yourself from the Frame library. Both thumbsticks open the mod
menu. Calibration has body/arm/hand alignment; World interactions has optional
contact, carry, throw and force settings. New installs use generic body defaults
and the previously approved grab offsets. Existing users keep their preferences.

## Windows PCVR — experimental

Requires Steam/SteamVR and working x86 Vulkan/VR drivers. Double-click
Launch-PCVR.pyw, select Portal, install/update, optionally apply the desktop
mirror profile, then start SteamVR and press Play when ready. Setup checks and
diagnostic ZIP export are local. No automatic upload. Index/other PC headsets
are not freshly qualified for this release; Frame acceptance is not PCVR acceptance.

## Exits, logs and rollback

Quit in-game or use Steam Stop when YOU choose. There is no automatic session
deadline or watchdog kill. Closing the PCVR GUI leaves Portal running. Logs are
local; helper processes stay silent. Native demo playback does not automatically
quit the game. Preserve startup/error evidence and remove personal details
before sharing diagnostic archives.

To roll back, close Portal yourself, use a retained older release's installer,
then re-run THAT version's FrameSetup against the same setup folder. Retain
settings and backup journals. Do not manually combine arbitrary DLLs/models/
profiles. Older installers may leave newer generated data unused. This release
installs no custom Chell skin: if you added private/third-party body overrides,
restore the original skin before evaluating documented Contact-55 behavior.

Save regularly. Read the known performance, door/contact, portal-world gun,
jiggle, dual-prop and replay limits. The hardware hiatus means future standalone
fixes or a support timetable cannot currently be promised.
