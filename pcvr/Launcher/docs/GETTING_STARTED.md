# Portal PCVR setup and controls

Use the **PCVR runtime ZIP**, extract the whole folder, and leave `Launcher/` beside the silent VBS. This package is for Windows SteamVR and your owned Windows Steam Portal2007 installation. Python3.11+ with Tcl/Tk is required. Index is the tested PCVR controller profile; no equal acceptance is claimed for other profiles.

## First installation

1. Close Portal yourself. Start `Launch-Portal-PCVR-Silent.vbs` (or double-click `Launcher/Launch-PCVR.pyw`). If Python cannot be found, install/configure Python with Tk and run the `.pyw` entry directly. Do not run as administrator just to bypass an error.
2. Browse to your Steam Portal folder containing `hl2.exe` and `portal/`. Select **Install / update PCVR mod**. The installer checks payload hashes, reads your local game VPKs to generate the gun/hands, and backs up replaced files. Nothing is downloaded as a substitute for owning the game. It does not launch or stop Portal.
3. Select **Apply native rig + accurate eye mirror**. This separately backs up preferences and enables the native model path and eye-copy desktop profile while preserving unrelated settings.
4. Start SteamVR, ensure the headset/controllers are ready, and choose **Check setup**. Missing/invalid files or Index bindings are reported before Play.
5. Choose **Play Portal VR**. Neither closing the launcher nor waiting for diagnostics ends the game. Quit in-game or choose Steam Stop yourself when finished. Old extracted launcher versions retain their own manifest: use this release's entry after an update.

The launcher stores attempts/preferences/notes under sibling `State/`, caches/temp under the extracted folder, and leaves portable game backups in your selected game directory. Put the extracted package on a drive where you want this data. Some native game logs also remain in the Portal installation. No automatic log uploads or scheduled helper are installed.

## Index controls

The default right-handed profile maps left stick to movement, right stick to turning, rightA to jump, rightB to crouch, leftB to pause, and leftA to menu toggle. LeftB also supplies menu Escape/back. Right trigger fires the primary/blue portal and left trigger the secondary/orange portal unless the colour-swap preference is enabled. Gun handoffs retain physical trigger colours.

Both thumbstick clicks together jump to the mod menu. In menus, aim a laser and use a trigger to select; grip drags panels, corner grips scale, grip+stick adjusts depth, and grip+directional pad adjusts panel position. Tracking/focus must be live and controls neutral after a SteamVR dashboard change. Index input may be rebound through SteamVR.

Grip handling depends on weapon/carry state: grip supports the gun, enables a handoff or shoulder draw/stow, or routes to native Use/palm carry according to selected modes. Right grip is the normal mapped Use input in the default right-handed profile. Use the ordinary mapped interaction for red test buttons; physical hand/gun touch activation is unfinished. Handedness and optional interaction routing can change grip behaviour.

## Calibration and optional physics

Use Calibration for standing T-pose measurement and manual body/hand fallbacks. Tune each hand's translation/rotation, upper-arm and forearm lengths, neck/head offsets, wrist roll and body proportions before compensating with gun settings. Gun position/aim angles, Set/Reset grab point, support capture/pull-away and holster zones have separate controls. Render width/MSAA/upscaling and some visual changes request a restart; the restart button is an explicit request and does not impose a session deadline.

World interactions groups visual hand contact, carry, throw, physical pressure/impact and sandbox force controls. Optional object/gun pushes and controller-velocity throws are conservative orOff on fresh installs. To try palm carry, select palm-held native physics and the stowed-gun palm routing. The held object can rotate and release to drop/throw; holding one prop alongside the gun is supported, two independent props is not completed. Portal transitions reset/bypass contact where needed. Very strong sandbox settings are experimental.

## Recording and diagnostics

OBS Game Capture of the Portal window receives the chosen eye scene in normal gameplay with the accurate-eye mirror enabled. Fit-with-bars preserves the full view; fill crops. Mod-menu/reticle/dashboard compositor overlays remain separate. Spectator cameras deliberately use a different scene. Native demos capture game/world events, not full injected body/finger motion.

Diagnostics export produces a local ZIP for you to inspect/share. It can contain local paths, settings and logs. Nothing is automatically uploaded. When reporting an issue, include release tag, headset/controller, GPU/driver, the action that triggered it, and a reviewed diagnostic ZIP if useful. The launcher records startup failure, normal exit, cancellation and nonzero exit separately; a nonzero exit alone does not prove a particular crash cause.

## Known gaps and rollback

Commentary progress/native HUD presentation and physical red-button presses are unfinished. Native HUD is deliberately excluded from stereo scene renders to prevent the09 performance regression. Original-art HUD reticle options may share that limitation. Native Chell skin is used; localized jiggle/full self collision are experimental, and world-gun front recoil is less complete than first person. Portal-heavy performance and controller/scene coverage vary; there is no all-campaign/72/90FPS guarantee.

Keep your older downloaded release and the install/profile backups. With Portal closed, restore the matching previous installed files from that installation's backup (see `installation.json`) and restore its matching profile if you changed it. Do not mix a DLL from one version with another version's generated assets/config blindly. Steam verification can restore base files, but does not necessarily remove injected mod files; inspect the backup journal rather than deleting unrelated game data.

The separate Frame Heavy Petting release is unchanged. Do not run FrameSetup for this PCVR package.
