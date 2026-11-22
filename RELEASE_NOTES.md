# The Heavy Petting Update — Contact-55

**Public beta 5 · November 22, 2026 · Last owner-tested standalone Frame build**

## A personal note: standalone Frame development is on hiatus

My Steam Frame has stopped charging. After an afternoon of extensive
troubleshooting, I have contacted Valve support and am waiting to see whether
we can resolve it or arrange an RMA. There is no confirmed diagnosis or return
date. This will be the last standalone Steam Frame update for the foreseeable
future, until I have working hardware and can test responsibly again.

I'm really sorry to everyone following this project and looking forward to
more. I'm sad to stop here, especially after the last tests made the hands,
gun and body feel so much better together. I hope you will still enjoy the
mostly playable version we have managed to create. Thank you for your patience,
enthusiasm and support. I really hope to return when the hardware situation is
sorted out. PCVR and other projects may continue separately, but I cannot promise
a timetable or claim that PC testing replaces standalone Frame acceptance.

The charging fault's cause is unknown. An RMA has not yet been confirmed, and
no claim is being made that the mod caused the fault.

## Why this build

This release contains **the exact Contact-55 DLL from my last completed Frame
test**, the best result I have personally tested so far. It brings the accepted
improvements forward from the previous public Interaction-26 nightly. It is
still a beta, not a crash-free or fully campaign-qualified release.

**Contact-56 and later experiments are excluded.** Moving-door recovery, the
neutral jiggle skin, animated portal-world gun prototype and ledge-mantle
prototype have not received Frame acceptance. The original Chell skin is used.

## Main improvements since the previous public update

- **Firm gun contact and two-handed grip.** Estimated hand pose settles on the
  shell; gripping captures palm position and orientation relative to that part
  of the gun instead of floating/sliding along an aiming socket. Raw tracked
  aim remains separate. Release, pull-away, handoff and stow clear the grip.
- **Animated front-shell grip.** Contact covers the rounded lower/front shell.
  A hand gripping the front follows its shot animation in first person. The
  later owner tests accepted shell placement, firm grip, indicator occlusion
  and smoother handling.
- **Gun lights and last-shot colour.** Detached/stowed portal-preview effects
  were addressed. Per-view attachment/depth policy and locally generated
  opaque-shell occlusion improve light containment. The top indicator and dot
  reticle can show the last portal colour. Physical trigger colours remain
  consistent across gun handoffs; crosshair options remain configurable.
- **Body/hand rendering recovery.** Red-body and hand/gun overlap regressions
  were corrected or rolled back. Startup ownership and valid-tracking guards
  address the earlier duplicate-hand path. Rejected body/skin experiments are
  not bundled with the accepted changes.
- **Arms, wrists and calibration.** Separate left/right upper-arm and forearm
  length controls, solved-length feedback, hand-alignment links, axial skin
  matching, configurable elbow/helper roll and bounded twist prevent accumulated
  winding. The last owner test was very positive about arms, fingers and grip.
- **Contact/carry polish.** Carry guards and adaptive smoothing build on palm
  carry, rotation/release throwing, physical pushes, sustained pressure and
  force menus. Optional contact/sandbox settings remain adjustable. This is
  not a complete Boneworks-style physical hand/body simulation.
- **Conservative CPU optimization and measurement.** Repeat shell transforms
  and ordinary model-header validation were reduced within bounded scopes.
  Sparse timing, secondary-view diagnostics and scene bookmarks help investigate
  portal cost without reducing image quality. No new Frame FPS gain is promised.
- **Launchers and creator tools retained.** Frame-local standalone repair,
  explicit restart support, silent local logging, experimental Windows PCVR
  launcher and native demo/desktop-camera tools remain available.

Read the **very detailed progress and blocker breakdown**, including the stereo
startup, model completeness, rendering regressions, skin audit, arm fit, physics,
standalone launch repair, performance investigation and unfinished roadmap:
[Heavy Petting: progress and hiatus](https://github.com/CR4zyHoBb1T52P1Ck/PORTAL_STEAM_FRAME_STANDALONE_VR/blob/main/docs/HEAVY_PETTING_PROGRESS.md).
It is also included in the runtime ZIP.

## Install or update — both steps matter

Download **Portal-Frame-VR-v0.1.0-beta.5-contact55.zip**. The `-source.zip` and
automatic GitHub source downloads do not contain the runtime payload. Requires
your owned **Windows x86 Steam Portal** files and a working Frame SteamVR/ARM64
Proton-FEX setup; runtime installation/game transfer are prerequisites.

1. Close Portal yourself. Extract the runtime ZIP outside the game folder.
2. Run `Install.py --portal-dir "your Windows Portal folder"` with Python3.11+.
   `--dry-run` validates without changing the game. It verifies checksums,
   preserves settings/saves, backs up changed files and generates gun/hand
   models plus gun-light occlusion data locally from your own retail VPKs.
3. If installed on Windows, transfer the patched Windows game and extracted
   release folder to Frame. On Frame, run `python3 FrameSetup.py --portal-dir
   "/actual/path/to/Portal"`. Retain an existing setup location with its
   `--setup-dir` argument.
4. Apply the printed Steam shortcut executable, working directory and launch
   options; mark it as VR. **Re-run FrameSetup after upgrading beta.4/the
   nightly:** it pins the new DLL hash. A DLL-only copy is incomplete.

[Full setup/upgrade guide](https://github.com/CR4zyHoBb1T52P1Ck/PORTAL_STEAM_FRAME_STANDALONE_VR/blob/main/docs/GETTING_STARTED.md).
After setup, play needs no PC or Wi-Fi. Both thumbsticks open the mod menu.
Contact, carry, throw and sandbox options remain opt-in; existing users keep
their preferences. Generic body defaults are supplied, not my personal fit.

**Windows PCVR/SteamVR:** `Launch-PCVR.pyw` opens the optional Python3/Tk GUI.
Select Portal, install/update, start SteamVR when ready, then Play. It is an
experimental path, not freshly headset-qualified on Index or another PCVR
device. Closing the launcher leaves the game running.

## Known limits

- **Portal-heavy performance:** the last run recorded2016×2064/eye,4×MSAA and
  native rendering. Sparse measured GPU slices averaged5.83ms at zero secondary
  views versus17.95/23.94ms at4/6views. These are incomplete GPU slices, not full
  frame timings; counts include reflection/depth work too. Sustained72FPS
  everywhere and90FPS remain unqualified. No quality cut is being advertised.
- **Moving doors:** button-controlled doors can still allow hand clipping.
  Contact-56's proposed recovery is deliberately excluded as untested.
- **Portal-world gun:** its front cover remains rigid in the portal preview,
  unlike the animated first-person grip. Light/contact edge cases may remain.
- **Physics/embodiment:** one native prop plus the gun, not two independent
  props. Finger/contact/self-touch poses are approximations. Elbows/shoulders
  are inferred and may need calibration. Existing portal/funneling safeguards
  remain, but all edge, jump, momentum and carry combinations are not qualified.
- **Jiggle/climbing:** localized jiggle is not a working/qualified release
  feature. No experimental Chell skin is installed. Static body morphs remain;
  ledge mantling/climbing is not implemented in gameplay.
- **Stability:** historical crashes/full-headset black screens have not all
  been explained. Improved owner play does not establish a permanent hardware
  fix. Save regularly. The new charging fault is likewise undiagnosed.
- **Creator tools:** additional camera views can cost performance. Native demos
  do not record injected VR body/finger poses; full embodied replay/theatre is
  unfinished.

## Verification, logging and support

Before owner testing, Contact-55 passed its build, CPU/ASAN regressions,439Python
checks and17deployment/scope checks. Release packaging has separate installer,
generation, source identity, checksum and privacy checks; see the included
`docs/RELEASE_VALIDATION.md`. The exact tested DLL SHA256 is:

```text
e53e2f798c61920c4396c6b97190cccd03dd956a9d091dadf044e97607b6a2c9
```

Logs stay local and silent; no automatic upload or recurring background task
is installed. No game-session deadline/watchdog kill is imposed. Quit in-game
or use Steam Stop whenever you choose. Older releases stay available; preserve
settings/backups and keep DLLs paired with their matching launch profiles.

Bug reports can still be recorded, but standalone fixes or a support schedule
cannot be promised during the hiatus. All existing credits, licenses and upstream
notices are preserved. Thank you again, and enjoy The Heavy Petting Update. — Dev
