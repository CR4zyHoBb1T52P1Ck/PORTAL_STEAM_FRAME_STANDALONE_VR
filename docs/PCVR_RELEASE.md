# Portal VR — First PCVR Release

**Public PCVR Beta 1 · v0.2.0-pcvr.1 · November 25, 2026 · owner-tested PCVR-10**

Portal VR is coming to desktop SteamVR. If you have followed the standalone Steam Frame project, this is the first dedicated PCVR release bringing that work together with the Index controls, body tuning and launcher developed during our PCVR tests. If you are discovering the project today: welcome! This is Portal with tracked VR hands, a customizable Chell body, a physically handled portal gun and a lot of ways to make the experience your own.

This package contains the **exact PCVR-10 DLL from the latest owner test**, which I consider our most feature-complete and stable PCVR build so far. The latest test was extremely positive about the overall experience. It is still a public beta, rather than a claim of universal headset support, a crash-free campaign or a fixed frame-rate guarantee.

**Thank you to everyone who has watched, followed, tested, shared ideas and supported the project.** Seeing these pieces come together has been incredibly exciting, and your interest has helped keep it moving. I am really happy to finally get this version into more people's hands. Have fun exploring Aperture in VR, and please share your feedback and creations!

## What PCVR players get

### Tracked Portal gameplay and a gun you can actually handle

- Stereoscopic VR with head tracking and tracked six-degree-of-freedom portal-gun aiming, including tilt and roll. Native Portal gameplay, puzzle logic, portal travel and funneling remain the foundation.
- Complete portal-gun housing assembled locally from your owned game assets. The first-person model retains its normal colours, scale and firing animation; the underside is filled in instead of leaving the flat-screen model's open shell exposed.
- Estimated hand/finger contact around the gun shell, including the rounded white front and lower housing. A support grip captures position and orientation relative to the selected part instead of continuously sliding along an aiming socket.
- Two-handed aiming, adjustable influence, grip coverage/capture and pull-away distances, and optional legacy support-placement offsets. A grip on the animated front follows that firing animation in first person.
- Left/right gun handoffs, shoulder stow/draw, visible holstered back gun in portal views, configurable shoulder zones and haptic feedback options. Physical trigger portal colours remain consistent across gun handoffs, with a colour-swap preference.
- Gun position and pitch/yaw/roll controls, customizable grab offsets, and **Set / Reset portal gun grab point** using the free hand's sampled position.
- Improved gun-light containment, top indicator glow and last-shot portal-colour feedback. Configurable VR reticle styles, colour, size, opacity and depth/distance preferences include last-shot colour.

### Chell, hands and calibration

- Native Chell body and hands, tracked Index finger articulation with fuller finger curl and thumb-extension tuning. Valve Index is the primary headset/controller setup tested for this PCVR release.
- Full-body visual IK, inferred legs with native walking/jumping animation, rigid torso/neck fitting and actor-floor-aware crouch correction. This does not require body or foot trackers.
- A dedicated calibration/tuning/comfort menu, standing T-pose calibration and manual fallbacks. Tune hand positions/orientations, body height/span/shoulder measurements, torso fit, separate left/right upper-arm and forearm lengths, wrist/forearm roll and hand size.
- Body proportion controls, including wrist/forearm/upper-arm thickness and torso tuning. Show full arms, hands or gun-only arrangements according to your preferences.
- Tracked body and held/holstered gun placement are reflected in portal views. The portal-world gun remains a rigid model; its front recoil animation can differ from the animated first-person front.

### Optional world interaction and physics play

- Visual hand collision and estimated flat-palm contact with world surfaces, with finger-pose override and touch haptic options.
- Optional physical hand/gun pushes, contact pressure and impact/melee forces. Separate stiffness, impulse, strength, rate and radius controls let you tune the feel; expanded sandbox force ranges are available for experimentation.
- Native float grab or palm-held native-physics carry modes, stowed-gun palm-grab routing, handoff controls, held-object rotation and adaptive carry smoothing.
- Optional controller-velocity release throws and drops, with configurable throw strength, speed limit and spin. The gun may stay in one hand while the other carries an object; simultaneous independent two-object holding is not a completed feature.
- Contact reset/clearance guards around portal travel and related transitions. These systems are approximations layered over Portal's native interactions, rather than a replacement physics engine.

### VR menus and comfort choices

- A separate VR settings panel alongside Portal's native menu, pointer selection, highlights, haptics and an Escape/back fallback. Click both thumbsticks together to jump to the mod menu.
- Movable panels with grab-and-drag placement, corner scaling, grab-plus-thumbstick depth adjustment and grab-plus-directional controls for panel movement. Menu visibility is kept above the scene, including elevator/menu situations.
- Locomotion direction, movement speed/deadzone, smooth or snap turning, turn speed/angle, handedness, seated/standing/height and room-scale options, vignette and menu placement/feedback preferences.
- Per-eye render-width controls, MSAA, texture/filter/shadow/effect settings and optional spatial-upscaling/sharpening controls. Settings that need a restart are labelled, and the menu has a restart request for new visual settings. No claim is made that an upscaler automatically improves every GPU or image.

### PCVR launcher, diagnostics and creator tools

- An Aperture/terminal-themed launcher with scrollable tabs, selectable/copyable text, notes, searchable logs and clipboard actions. Saved light/dark themes, blue/orange/green accents, text sizing and pane layout let you adjust it to your display.
- Explicit Install/update, setup checking, native rig/accurate-eye capture profile, SteamVR start and Play controls. Existing installation files/settings receive backups; owned-asset generation happens locally.
- Complete versioned Index input bindings and a binding audit, dependency/file checks, local attempt records, startup/exit evidence and diagnostic ZIP export. Logging is silent and local. Nothing is automatically uploaded, and there is no game-session timer or automatic forced shutdown. Closing the launcher leaves a running game alone.
- Gameplay desktop capture copies a selected eye's scene instead of rendering an independently posed desktop camera. Fit/crop and eye selection are configurable. This avoids the extra gameplay scene render; SteamVR compositor overlays such as the mod menu and VR dot remain separate from that scene copy.
- Experimental spectator follow/orbit/free-camera modes and native demo recording/playback controls for capture work. Native demos preserve game/world events, **not** a full replay of injected VR body/finger poses; this is not a complete pose-replay theatre.
- Built-in GitHub/issues/releases, Ko-fi and YouTube links so players can report problems and follow development.

## Important remaining gaps

- **Developer-commentary playback progress and some native HUD elements are not presented in VR.** PCVR-09's attempt to render the native HUD inside both eyes caused a severe performance regression. PCVR-10 withdraws that path. Commentary HUD needs a separately qualified implementation.
- **Physically pressing the small red pedestal buttons with a hand or gun is not working in the owner test.** The experimental touch option is not a completed feature. Use the normal mapped Use/grab interaction; doors and puzzle logic are still native Portal systems.
- Experimental localized skin/jiggle options and full body self-collision are not being advertised as completed features. This release uses the native Chell skin; rejected private skin experiments are not shipped. Multi-object carry and full VR-pose replay remain stretch goals.
- Portal-heavy views can still be expensive. Performance depends on the PC, headset, scene and settings; there is no sustained72/90FPS promise, full-campaign certification or all-controller acceptance claim. Other included controller profiles have not received the same PCVR acceptance as Index.

## Download and start

Download **Portal-VR-PCVR-v0.2.0-pcvr.1.zip** for installation. The `-source.zip` and GitHub's automatic source downloads are for developers and omit the runtime payload.

1. Own and install the Windows Steam version of **Portal (2007)**. This is not Portal2 and does not bundle either game.
2. Install Python3.11 or newer with Tcl/Tk, plus SteamVR and a suitable Vulkan-capable graphics driver. Valve Index is the primary tested setup.
3. Extract the runtime ZIP to a writable folder. Keep its `Launcher/` directory and files together. Open **Launch-Portal-PCVR-Silent.vbs**, or `Launcher/Launch-PCVR.pyw` if Python file associations are configured.
4. Close Portal yourself, select its Steam folder (the one containing `hl2.exe` and `portal/`), then choose **Install / update PCVR mod**. Do not merely copy the DLL: the installer generates the complete gun/hands and shell data from your own retail files.
5. Apply **native rig + accurate eye mirror**, start SteamVR, run **Check setup**, and use **Play Portal VR** when ready. Calibrate in VR before fine-tuning interactions.

Detailed setup, controls, optional physics settings, diagnostics and rollback are in `Launcher/docs/GETTING_STARTED.md`. New installs use generic calibration and conservative optional-physics defaults; a developer's personal fit is not bundled. Existing settings are preserved. Never install/update over a running Portal session.

## PCVR and standalone Frame are separate downloads

This is the **PCVR-10** release for desktop SteamVR. It does not replace the Frame-local launch profile or qualify a new standalone build. The previous **Heavy Petting / Contact-55** Frame release remains available and unchanged. Standalone development is still on hardware hiatus while my Frame charging/support situation is resolved; PC testing is not claimed as Frame testing.

## Credits and keeping in touch

Built on BowmanFox/portal1vr and its Portal2VR/L4D2VR ancestry, with DXVK/dxvk_l4d2vr, OpenVR, MinHook, Snapdragon GSR and Valve Source SDK work. Portal belongs to Valve. Existing project credits, licenses and third-party notices are preserved; the package supplies the mod and transformation tools, not retail game assets.

Thank you again to everyone interested in the project and everyone supporting the development journey. I hope you enjoy taking this version for a spin!

[Development videos](https://www.youtube.com/@DevDoesDevThings) · [Ko-fi](https://ko-fi.com/devdoesdevthings) · [Report an issue](https://github.com/CR4zyHoBb1T52P1Ck/PORTAL_STEAM_FRAME_STANDALONE_VR/issues/new)
