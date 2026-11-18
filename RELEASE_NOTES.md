# v0.1.0-beta.3 - Embodiment-23

The owner-tested contact, gun and customization update. Uses the exact tested
Embodiment-23 DLL, retaining the Audit-21 lighting/crash-path fix.

- Stronger, independently adjustable hand/gun impacts, impulse caps and cooldown.
  Embedded contacts recover a surface query; stationary force traces are skipped.
- Gun wall correction follows a protected head-to-muzzle sweep, with adjustable
  radius, displacement limit and release response. Portal-environment bypass and
  crossing/loading/menu resets protect native movement and funneling behavior.
- Complete rigid rear gun housing generated from owned retail assets, with an
  inner lining; native hands, glass, moving claws and animations are preserved.
- Either controller selects native props, independently of float/palm carry.
  Optional palm snap, offset and approximate finger curl. One native held object
  remains supported; independent dual-object carry is not implemented.
- Fixed physical trigger portal colors across handoffs, saved thumb extension,
  crosshair options, body calibration and cosmetic body customization controls.
- Existing reusable GPU menus and scene rendering retained; redundant paired-hand
  draws are skipped when the current full-body palette provides those hands.

Owner feedback: impacts and wall contact are substantially improved, the gun
looks good, and ordinary scenes run near71FPS in this setup. Looking at a portal
or multiple portals still causes slowdown. These are owner observations, not a
sustained performance guarantee or a GPU benchmark.

Known issues: the owner did not find chest/hip sliders in the headset menu and
wants more pronounced jiggle. Those controls exist in this build's Body shape
page but visibility/discoverability and response remain under investigation.
Hand-driven body response is NOT included. Portal-heavy optimization and those
body improvements are next-build work, not claimed fixed in this release.
Historical abrupt headset power-off cause remains unconfirmed.

Validation:76 host tests, owned native ABI and portal/camera checks, AddressSanitizer
body/leg/contact/palm/portal-skin checks and overlay pixel/queue regressions pass.
CPU checks do not establish headset/GPU stability. Save progress regularly.
Experimental hand contact, pushes, palm carry, throws and jiggle default off;
enable the features you want in World interactions / Arms / Jiggle.

Install.py verifies the mod package, generates gun/hands from your owned Windows
Portal assets, preserves existing settings and creates rollback backups. Keep
your working Frame runtime/shortcut setup. No game assets, drivers, recordings,
private logs or generated retail models are distributed. Existing credits and
license are preserved. No automatic gameplay test deadline or termination.
