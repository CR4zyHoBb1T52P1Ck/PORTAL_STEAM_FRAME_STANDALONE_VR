# EMERGENCY DAY ONE WOOPS PATCH — v0.1.0-beta.4

Fixes direct standalone launching from the Steam Frame library. No PC or Wi-Fi
is required to launch or play after the game/runtime and shortcut are set up.

**Existing users: run Install.py, then FrameSetup.py on your Frame and update
your Steam shortcut using the settings it prints. Replacing the DLL alone does
not repair the shortcut.** See [the setup guide](https://github.com/CR4zyHoBb1T52P1Ck/PORTAL_STEAM_FRAME_STANDALONE_VR/blob/main/docs/GETTING_STARTED.md).

The old shortcut targeted hl2.exe directly. Portal regenerates a steam_appid.txt
file identifying app400; Steam can then refuse the next unmanaged/devkit launch
because it does not consider that app installed. Development's PC launcher
archived this marker, so that path worked while direct headset Play failed.
The published instructions also omitted the required -dx9 renderer argument.

This patch supplies a marker-free Windows relay folder and a Frame-local launch
prefix. It archives the expected generated marker, applies saved startup
settings, starts the local restart helper, and launches Portal in the correct
working directory with -dx9 and the native d3d9 override. Unexpected app-ID
contents are preserved and reported. All startup/exit logging is local and
silent. No remote desktop, personal log uploader or recurring PC task is needed.
Steam shortcut paths/options still need to be set as printed by FrameSetup.py;
the tool does not install Proton/FEX or transfer the owned game for you.

New-install gun grab defaults are Devon's latest tested alignment:
right -2 cm, up -3 cm, forward -3.5 cm. Existing saved preferences are preserved.
You can still adjust the grip in the VR menu. These are configuration defaults;
the existing explicit authored-grip reset behavior is unchanged.

Uses the exact tested Embodiment-23 / beta.3 game DLL. No unfinished interaction,
body/jiggle, crosshair or performance prototypes are bundled. The owner confirmed
the repaired library launch worked perfectly on the Frame. Portable setup and
identity/path/checksum behavior are covered by host tests; CPU relay checks pass
on Windows and Frame. Other users' runtime/shortcut setups can differ.

The mod remains beta software. Portal-heavy frame-rate drops and existing
experimental interaction/body limitations remain. This patch does not claim
those gameplay fixes or diagnose historical headset power-offs.

All previous credits, license and upstream notices are preserved. Requires
your own Windows x86 Steam Portal assets and a working Frame ARM64 Proton/FEX
VR environment. No retail game assets, recordings, saves, private logs or SSH
credentials are distributed. No game is launched by either setup tool, and no
automatic gameplay deadline, watchdog kill or termination is installed.
