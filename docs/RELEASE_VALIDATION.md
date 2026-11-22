# Heavy Petting release validation

The runtime DLL is the exact sealed, installed and subsequently owner-tested
Contact-55 artifact, SHA256
`e53e2f798c61920c4396c6b97190cccd03dd956a9d091dadf044e97607b6a2c9`.
Contact-56 is excluded. No new headset/GPU test or app launch was performed for
this packaging work; the Frame charging failure prevents further acceptance.

- Original55 build/CPU+ASAN/prior arm, grip, render, portal regressions;
  439Python checks and17deployment/scope checks retained from development.
- Reconstructed public source compiled successfully with pinned DXVK plus the
  published patch, MSVC v145/x86 and post-build deployment disabled.0errors;
  existing warnings remain. Rebuilt qualification DLL is not the release DLL.
- Re-ran latest public55arm/roll/skin-gate/menu CPU checks and ASAN:
  60segment cases with6palm-style repeats,3600bounded-roll samples,16skin gates,
  57bounded pages/263options pass.
-36portable public Python tests pass, including7new temporary installer cases:
  dry-run immutability, settings/backups, generated-BVH transaction/rollback,
  existing data preservation, active-session refusal, payload tamper refusal,
  concurrent generated-asset edit preservation.
- Local generator recreated all6tested gun/hand model files and opaque-shell
  BVH with exact installed55hashes. Generation did not modify the owned game.
  None of that private generated output is in the public ZIP/source.
- Public config contains generic body defaults, all263menu keys, optional
  physical featuresOff and previously approved generic grab offsets. No live
  developer profile, recordings, logs, saves, credentials or private device data.
- Existing LICENSE,THIRD_PARTY_NOTICES and all existing credits preserved.
  Runtime/source archive hashes and internal file checksums are supplied.
- Launchers retain user-controlled lifetime: observer waits do not terminate
  Portal, closing PCVR GUI leaves it running, native demo auto-quit disabled.
  Helpers are silent; public tools install no automatic log-upload service.

After publication, an anonymous download/checksum check is recorded in the
release work records. Packaging/CPU/source checks do not prove new headset,
all-controller, campaign-wide, hardware-black-screen or sustained72/90FPS
qualification. Known limits and the hardware hiatus remain explicit.
