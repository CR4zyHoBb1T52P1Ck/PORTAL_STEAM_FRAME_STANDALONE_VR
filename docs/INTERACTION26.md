Interaction-26 nightly — headset qualification pending

FRAME
The main test entry becomes Portal VR - Standalone (Interaction-26).
The separate Stable Embodiment-23 / beta.3 entry remains unchanged.
The latest saved stable profile seeds this trial: grip/hand placement, force,
body and comfort tweaks. Previous main-test config is backed up. Saves remain
separate between the two entries. Quit/Steam Stop ends the game when you choose.

INTERACTIONS
Prop grab > Gun stowed / handoff routing: new grabs use direct palm carry when
the gun is stowed. Gun drawn uses the selected carry style. The mode is latched
until release. Hold grip to carry; release gently to drop or move and release
to throw. Bring hands near and squeeze the receiving grip to transfer ownership
without a detach. The previous hand's release must not throw the object. Drawing or handing off
the gun must preserve an already-held prop in the other hand.
The actual native surface is sampled on acquisition, not a fixed centre offset.
Visible hands follow the blocked physics object at walls. Finger pose uses an
approximate contact volume. Across linked portals, custom target/visual binding
defers to native carry. Tracking/loading/menu fences clear contacts and throws.
ONE native held prop remains supported, plus the portal gun; independent
two-prop ownership and full concave-mesh grips are not implemented.

GUN HANDS
Arms and body > Fingers > Gun surface and visibility: surface stop, clearance
and scene-depth toggle. Both hands use conservative housing/underside contact
proxies, not a full moving-part mesh collider. Controllers and gun aiming are
not moved by the finger solver. The GPU bridge changes a compressed viewmodel
depth range only for gun drawing, then restores the prior viewport. Disable
Gun world depth if your runtime shows artifacts. Headset/GPU testing is needed.

FORCES
Physical contact > Sustained pressure: independent hand/gun stiffness and cap.
Slow presses apply pressure rather than being discarded by the impact speed
gate. Multiple gun probes cannot multiply pressure on the same target.
Extra sandbox force ranges: optional 1–10x push/melee and 1–5x throw multipliers.
Finite final caps remain:4000N,1200Ns,40m/s. Your prior settings are preserved;
sandbox multipliers default off. Native player movement/funneling is retained.

CREATOR
Home > Creator, cameras and replay: native demo record/stop, latest take,
pause/resume, speed and native timeline. Stop recording keeps gameplay open.
Playing a take leaves the current level: save before selecting playback.
Native demos replay world/game events, NOT injected VR finger/body animation.
This is a native demo viewer foundation, not full CTR feature parity or a VR
pose-replay editor. Demo playback explicitly disables automatic game quit.
Spectator camera: head mirror, follow, orbit, free. It changes desktop output
only; headset tracking and native player travel stay separate. Move with menu
controls or optional focused-game-window WASD/QE + arrows. Keyboard editing
parks player movement. Selecting a camera enables an extra desktop render;
disable mirror for maximum headset performance. Full stereo fly-camera,
theatre-screen output and VR-pose sidecars remain future work.
Whole-torso jiggle inflation is suppressed; localized softskin asset prototypes
were not installed with this interaction trial. Static body morphs remain.

PCVR
Extract the nightly runtime ZIP; double-click Launch-PCVR.pyw.
Python3 with Tk required. Choose your owned Windows x86 Portal folder, install
the mod, optionally apply PC mirror profile, start SteamVR, then Play.
The GUI checks hashes/runtime presence, preserves per-attempt logs and supports
explicit in-game restart requests. It imposes no game deadline. Closing the
launcher leaves Portal running. Diagnostics export is local for review, with
no automatic upload. PC GPU/headset/controller operation remains untested.

VALIDATION
Build succeeded. CPU and AddressSanitizer checks pass:52menus fit, handoff and
release state,27rotated surface anchors, rate-independent pressure, actual Chell
finger bone lengths, first-contact blocking, independent camera transforms.
61Python launcher/logging/update/restart tests pass. Offscreen menu rasters were
inspected. These checks do not establish in-headset stability. No test game or
renderer was launched. Stable emergency beta.4 remains available separately.
