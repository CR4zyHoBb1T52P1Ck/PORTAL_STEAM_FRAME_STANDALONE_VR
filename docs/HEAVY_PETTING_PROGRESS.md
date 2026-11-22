# The Heavy Petting Update: progress, blockers and the Frame hiatus

November 22, 2026 · Contact-55 · Public beta 5

This is the best version of Portal standalone VR that I have personally tested
on my Steam Frame so far. It is also, unfortunately, the last standalone Frame
update I can offer for the foreseeable future. My Frame has stopped charging.
After an afternoon of troubleshooting I have contacted Valve support, and I am
waiting to find out whether we can fix it or arrange an RMA. There is no confirmed
diagnosis or replacement timetable. I cannot currently perform further Frame
acceptance tests.

I am really sorry to everyone who has been following along, waiting for the next
build, or hoping to take Portal VR out in their bag. I am sad to have to stop at
this point: the last few tests were the first time that so much of the handling,
embodiment and presentation felt as though it was coming together. I hope you
can still have a lot of fun with this mostly playable version. Thank you for
the excitement, patience and encouragement that helped get it here. There is
no promised return date, but I very much hope this is a hardware interruption
rather than the end of the project.

The charging failure is the reason for this hiatus. We have not established its
cause, and are not claiming that the mod caused it or that an RMA is approved.
The repository remains public and the release stays available. Windows
PCVR/SteamVR and other projects may still be explored separately; that does not
substitute for testing standalone compatibility on a working Frame.

## What is actually being released

The runtime is the exact Contact-55 DLL used in the last completed owner test,
not a rebuild with extra features added afterward. Its SHA-256 is:

```text
e53e2f798c61920c4396c6b97190cccd03dd956a9d091dadf044e97607b6a2c9
```

Contact-56's moving-door recovery and neutral skin experiment were built and
checked on the host, but never installed or tested on Frame. Neither is in this
release. The offline ledge-mantle prototype and animated portal-world gun
prototype are also excluded. A successful CPU test is useful evidence, but it
cannot establish headset visibility, comfort, GPU stability or campaign-wide
acceptance.

The package contains the mod, launchers, generic defaults, source and local
asset-generation tools. You must own Portal. Retail models, textures, recordings,
saves, development logs, personal calibration and private device configuration
are not distributed. Gun/hand overrides and the gun-light occlusion geometry
are generated locally from your own game files. The original packed Chell body
is used; no experimental jiggle skin is installed.

## From a flat launch to actual standalone VR

The starting point was a game that could launch in a flat presentation, with
unknown startup errors and inherited mod naming. Getting it into VR was only
the first step. Stereo cameras were initially mismatched; head roll, tilt and
lean produced obvious warping. Early image quality was poor even when menu
panels looked sharp, and simply raising the requested per-eye resolution did
not produce the expected clarity. There were also serious freezes during the
early rendering work.

The project now runs Portal locally on Frame through the Windows x86 game and
the ARM64 Proton/FEX compatibility stack, with tracked head and controller
poses. Stereo projection, eye/viewmodel alignment, render-target handling and
portal-aware transforms received repeated fixes and owner tests. Actual scene
dimensions and sparse frame timing are logged, so the requested menu resolution
can be checked against what is rendered. The latest development run recorded
2016×2064 scene and output per eye with native rendering and 4× MSAA.

This is not a claim that every camera, effect, map or hardware configuration has
been qualified. It is a substantial improvement over the early flat/warped
result, supported by repeated tests through several chambers. Source engine
portal rendering remains the hardest performance workload.

## Menus, settings and comfort

The native pause menu and the separate VR mod panel were made visible in the
world, including situations such as elevators where they previously disappeared
or became obstructed. Panel interaction gained pointer highlighting, controller
feedback, placement, depth and scale controls. Clicking both thumbsticks opens
the mod menu. The menu provides calibration, movement, turning, comfort, body,
gun, world interaction, crosshair, visual and creator settings rather than
forcing one fixed setup for everyone.

Settings work also addressed invalid configuration parsing and inherited error
presentation. Visual controls include scene/output resolution, MSAA and available
spatial upscaling options, with explicit restart support for settings requiring
a game restart. The later builds add image-baseline comparison and scene
bookmarks to make performance tests more repeatable.

Launch and observer tools preserve startup/error evidence, use silent helper
processes, and do not impose a game-session deadline. The recurring command
windows from an early logging implementation were addressed. Public release
tools do not automatically upload logs. An observer failure must not silently
kill the game. The player decides when to quit or use Steam Stop.

## Chell, fingers and arm calibration

The early unwanted avatar presentation was replaced with Portal's native Chell
body and hands. We worked through missing textures, hand visibility when armed,
limited finger curl, incomplete thumb extension, body posture, jumping legs and
portal-preview representation. Finger input now drives the supported controller
curl/pose approximation through a much fuller range. This is controller-driven
finger posing, not a claim of complete optical hand or finger tracking.

Standing T-pose calibration, manual measurements and hand/controller placement
controls were added so the body can be fitted rather than assuming everyone's
proportions match Chell. Static body shape controls include wrist/forearm
thickness and chest/hip scale. Later wrist work distributes roll across native
elbow/helper bones while retaining tracked hand targets. A bounded roll option
prevents accumulated multi-turn twisting.

Contact-55 adds independent left/right upper-arm and forearm length controls,
50–150% in 2% steps. The solve preserves a chosen forearm length while allowing
the upper arm to accommodate reach when that option is enabled; the skin scales
along the segment rather than leaving the surface at the old length. Hand and
finger branches remain rigid. A footer displays solved lengths, and the page
links to hand/controller alignment. Those controls solve different problems:
segment length changes elbow-to-wrist spacing, whereas alignment changes the
controller/palm offset.

The last owner test was very positive about the arms, finger behavior, grip and
two-handed aiming working together. Arm IK still infers elbows and shoulders
from limited tracking. It cannot guarantee anatomically exact self-touch for
every body, pose or controller. Manual calibration may still be necessary.

## The portal gun and why the name fits

The flat-game viewmodel was incomplete when viewed from below in VR. A complete
rear housing and inward lining are now generated from the owned game assets;
overlapping one-way legacy shell triangles are masked. Native colour, scale,
attachments and moving front components are retained. This addressed the
see-through underside and shell intersections without distributing the model.

The most substantial recent change is contact with the gun itself. Touching
different parts of the shell gives an estimated hand pose and surface contact.
A support grip captures the corrected palm position and orientation relative
to the gun instead of letting the hand float or slide along an aiming socket.
The tracked aiming socket stays separate from the visible hand correction.
Release, pull-away, handoff, stow, tracking loss and menus clear the grip state.
Legacy slide/offset behavior remains optional.

Front-shell contact was expanded and then connected to the actual animated
front cover. A hand gripping that part now follows its shot animation in the
first-person view. The owner accepted the resulting arbitrary shell placement,
firm grip, lack of clipping in the tested poses, indicator occlusion and smoother
handling. Two-handed aiming, shoulder stow, hand switching and adjustable grab,
holster and feedback settings are retained. Portal colours stay assigned to the
same physical triggers across gun handoffs.

Gun lights required separate work. Detached/stowed portal-window glow was
addressed; actual per-view attachment poses, scoped depth policy and owned-shell
occlusion checks were added. A local CPU BVH excludes hands and glass and tests
the opaque gun housing. The top indicator can display the last portal's colour;
the crosshair dot also supports that colour behavior. The owner accepted the
recent first-person presentation, including the hand covering the indicator.
The tests do not promise perfect light visibility from every possible angle.

The remaining major presentation limit is the portal-world gun: its current
representation is rigid, so the preview does not reproduce the moving front
cover in the same way as the first-person model. Replacing it safely requires
separate culling, animation, attachment, shader and ownership qualification.
That replacement was not silently bundled with the body or grip fixes.

## World interactions, pushing, carrying and throwing

Optional visual hand contact, flat-palm approximation, haptics, physical hand
and gun pushes, sustained pressure, impact tuning and throwing were introduced
and refined. The distinction between slow pressure and a fast swing matters:
early interactions could nudge objects but did not produce convincing pushes
or turret knockdowns. Force, stiffness, impulse, cooldown and optional sandbox
gain controls make these behaviors adjustable rather than hard-coded to one
person's preference.

When the gun is stowed, the direct palm-carry route can acquire an object,
approximate a supporting hand pose, carry its rotation, and drop or throw based
on release motion. Gun drawing/handoff and prop ownership need separate rules
so a visual grip does not steal an unrelated held object. Rotation lockups and
jitter were reported and addressed with carry guards and adaptive smoothing.
Owner tests confirmed major progress in rotation, release throwing, contact and
general handling; they do not amount to complete rigid-body hand simulation.

One native prop plus the portal gun remains the supported ownership model.
Carrying two independent props is not implemented. Hand/body/gun self-contact
is still a bounded visual approximation, not a complete physical skeleton or
full concave hand collider. Some self-touch and object edges can remain imperfect.
Button-controlled doors were specifically reported as a remaining clipping case;
the proposed recovery is in the untested Contact-56 candidate, not this release.

Portal movement, native linked-portal carry and funneling have been protected
from these interaction changes. Existing bypass/reset rules avoid using stale
contact state around traversal. That is a design constraint and a regression
check, not a guarantee that every portal, edge, velocity, carry or jump combination
has been tested. Save regularly and report the exact situation when something
gets stuck.

## Rendering regressions and stability blockers

Several promising integrations were rejected in the headset. Contact-44's
rendering changes improved muzzle coverage but introduced a red body and worse
overlap; Contact-45 restored the accepted path and recovered body colour and
hand/gun visibility. Brief startup duplicate hands also required explicit
ownership and valid tracking before the normal viewmodel could draw.

Contact-47 combined too much: its body/skin/contact bundle produced a missing
body apart from hair, intermittent sticking and poor grab behavior. Contact-48
restored the earlier accepted runtime and removed the whole experimental skin
tuple. Later grip, shell and wrist fixes were reintroduced as smaller accepted
steps. The failed builds and evidence were retained, but are not release assets.

Offline skin auditing later found concrete issues in the old generated skin,
including hardware strip bone-selector counts and lost third-weight influences.
A corrected exact-neutral prototype passed extensive CPU skinning comparisons,
but it still needed native visibility acceptance. The hardware failure happened
before that test could take place. Consequently, localized breast/glute jiggle
is not a working feature of this release. The menu controls do not change that.
Static body morphs are separate and remain available.

Earlier app crashes, load/reload problems and a captured body-lighting crash
path received fixes. Stability improved in owner play, including a reported
roughly 40-minute session during the project's development. Historical complete
black-screen/power-off events were not conclusively diagnosed: heat, battery,
driver/runtime or another cause cannot be established from the evidence. The
charging fault is likewise undiagnosed. We are not calling this crash-free or
claiming to have fixed every hardware black screen.

## Performance, upscaling and the 90 FPS question

Image clarity improved substantially, but the owner explicitly rejected further
visual-quality reductions to chase speed. The accepted baseline therefore keeps
the selected resolution, MSAA, effects and portal output. Existing spatial
upscaling options are choices, not proof of quality-equivalent speed gains.
No new temporal reconstruction, frame generation or guaranteed 90 FPS mode is
being claimed.

Safe CPU work reduced repeat shell-transform validation inside an owned draw
and repeated model-header validation for ordinary props. Host synthetic batches
showed less CPU work, but those numbers are not Frame FPS gains. Sparse timing,
GPU stages, secondary-view counts and scene bookmarks were added to help locate
the remaining cost without changing the picture.

In the last Contact-55 run, sparse measured GPU-stage sums averaged about 5.83 ms
at zero secondary views, 17.95 ms at four and 23.94 ms at six. These are incomplete
GPU slices, not complete compositor/frame times, and secondary counts also include
reflection/depth work rather than uniquely identifying portals. Nonetheless,
portal-heavy samples already exceed the 13.89 ms budget for 72 FPS and the
11.11 ms budget for 90 FPS. The body solver averaged only about 0.26–0.41 ms CPU.
Further body simplification alone cannot solve that rendering cost.

The next sensible steps were matched SteamVR performance recordings, identifying
which secondary views cost the most, and carefully auditing redundant validation
or synchronization. Removing queue waits blindly risks the old freezes; reusing
one eye's or portal's image risks incorrect stereo/visibility. None was treated
as a free optimization. A future 90 Hz display test is reasonable to investigate
in light scenes, but sustained native 90 FPS near portals is not qualified.

## Standalone launchers and creator tools

An earlier public build depended on the PC's silent launch path and failed when
launched directly from Frame. The emergency standalone repair introduced the
Frame-local relay, launch script and DLL-pinned profile. That repair is retained.
After installation and FrameSetup, the game runs without a PC or Wi-Fi. Updating
only the DLL leaves an old profile hash behind, so upgrades still require both
Install.py and FrameSetup.py, then the printed Steam shortcut settings.

The Windows PCVR GUI can install, inspect a setup, launch with SteamVR, preserve
attempt logs and export a local diagnostic ZIP. It is included as an experimental
path; this release was accepted on standalone Frame, not freshly qualified on
Index or another PC headset. Closing the GUI does not end the game.

Creator controls include optional desktop head/follow/orbit/free cameras and
native demo recording/playback controls. Extra camera views can cost performance.
Native demos capture game/world events, not the injected VR body/finger pose
stream; full embodied VR replay/theatre is unfinished. The dream of an easy,
high-quality trailer capture remains worth pursuing, but is not advertised as
finished replay functionality.

## What remains after the hiatus

The unfinished list is explicit: neutral native skin acceptance and genuinely
localized jiggle; moving-door hand recovery; animated portal-world gun parity;
more complete self-touch; independent dual-prop ownership; safe default-Off
ledge mantling; matched portal rendering optimization and 90 Hz qualification;
and broader PCVR/controller/campaign testing. The mantle reference passed offline
tests, but needs static support identity, grip arbitration before prop pickup,
head/player clearance and portal-corridor checks before it belongs in gameplay.

Contact-55 itself passed its native build, CPU/ASAN arm/grip/render regressions,
439 Python checks and 17 deployment/scope checks before installation. The release
packaging is checked separately for checksums, source identity, installer behavior
and local asset generation. Those checks complement the positive owner playtest;
they cannot replace the missing future Frame sessions.

Existing credits, upstream licenses and notices are preserved. This is a community
mod, not an official Valve release. Thank you again for helping turn a rough VR
launch into something I genuinely enjoyed playing and carrying around. I hope
The Heavy Petting Update gives you a good time while we wait to see what happens
with the headset.
