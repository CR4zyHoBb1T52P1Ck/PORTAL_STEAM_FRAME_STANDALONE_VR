# Interaction-26 nightly — November 20, 2026

This is an experimental nightly prerelease for Steam Frame standalone and
Windows PCVR/SteamVR. It contains the exact Interaction-26 DLL installed for
the owner's next test. In-headset/GPU qualification is still pending. Stable
emergency beta.4 remains available separately; this nightly does not establish
a fix for historical headset black screens or guarantee frame rates.

## Changes

- Gun-stowed grabs select direct palm carry. Carry style stays latched until
  release. Surface acquisition, visible-hand binding and grip handoffs improve
  holding; release velocity determines drop/throw. Drawing or handing off the
  gun preserves an already-held prop.
- Both hands gain conservative gun-surface finger stops and an optional scoped
  scene-depth correction for gun rendering. Tracking and gun aim stay separate.
- Sustained pressure works for slow hand/gun pushes. Optional sandbox settings
  offer up to 10x push/melee and 5x throw; finite final limits remain.
- Creator controls add native demo recording/playback, pause/speed/timeline and
  desktop head/follow/orbit/free cameras. Cameras are opt-in extra render work.
- Windows PCVR launcher adds installation, runtime checks, per-attempt logs,
  explicit restart support and local diagnostic ZIP export.
- Includes intervening crosshair angular-size, portal/body-cache and body-menu
  work. These changes have not been measured for headset performance gains.

## Install or update

Download the runtime ZIP (the source ZIP is for building). Extract everything.
Requires your own Windows x86 Steam Portal assets; no retail assets are included.

**Frame:** close Portal yourself, run Install.py against your owned game folder,
then run FrameSetup.py on Frame and use its printed Steam shortcut settings.
Re-run FrameSetup.py for this update even after beta.4: it refreshes the launch
profile's DLL hash. A DLL-only copy leaves that profile out of date. The repaired
marker-free local relay/prefix is retained. No PC/Wi-Fi is required afterward.
See docs/GETTING_STARTED.md for the complete setup commands and requirements.

**Windows PCVR (experimental):** Python 3 with Tk, Steam, SteamVR and working
x86 Vulkan support are required. Double-click Launch-PCVR.pyw, select your owned
Portal directory, install/update, optionally enable the PC mirror profile,
start SteamVR and press Play. Native PC GPU/headset/controller use is untested.

Existing preferences and saves are preserved by the installer. New installs
retain generic body defaults and the approved grab offset (-2/-3/-3.5cm).
Contact, force/throw and sandbox features remain optional. Tune one at a time.
Click both thumbsticks for the VR mod menu. See docs/INTERACTION26.md for controls.

## Limits and testing

- One native carried prop plus portal gun; independent dual-prop carry is not
  implemented. Finger stops use approximate proxies, not a full concave collider.
- Native linked-portal carry owns cross-portal behavior; native player movement
  and funneling are retained. Contact/portal edge cases still need headset tests.
- Native demos capture world/game events, not injected VR finger/body poses.
  Creator cameras change desktop output only. Full VR-pose replay, stereo flycam
  and a theatre screen are not implemented. Playing a demo leaves the current
  level, so save first. Disable the mirror for maximum headset performance.
- Whole-torso dynamic inflation is suppressed. Localized jiggle assets are not
  included; static body morph controls remain available.
- Build succeeded; CPU/ASAN checks cover 52 menus, carry/handoff states, contact
  transforms, finger lengths/stops, pressure integration and camera math.
  61 Python launcher/logging/update tests passed for the installed trial.
  Installed payload verification passed. These are not headset stability tests.

Logging stays local and silent. There is no automatic log upload, game deadline
or watchdog kill. Closing the PCVR launcher leaves the game running. Quit in-game
or use Steam Stop when you choose. Setup never launches a game. Native demo
playback explicitly disables automatic game quit.

All existing credits, licenses and upstream notices are preserved.
