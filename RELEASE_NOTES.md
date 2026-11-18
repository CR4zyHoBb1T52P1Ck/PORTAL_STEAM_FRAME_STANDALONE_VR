# v0.1.0-beta.2 — Audit-21 stability update

Fixes the additional first-person body draw using a real model-instance cache
slot. It now bypasses stale cached lighting, addressing the captured load/chamber
transition crash path. Also includes corrected jump legs, loading-plaque guards,
paired portal model matrices, reusable GPU menu textures, grip-held pickup and
release-to-throw for either hand, and safer throw cancellation on tracking loss
or portal crossing. Native portal movement and funneling routines are retained.

The owner reports approximately 40 minutes of play with much better stability.
The run ended in a sudden headset power-off; its cause (including battery,
temperature or a system fault) is unconfirmed. This is still a beta; stability,
all portal/carry combinations and performance vary by device and settings.
Optional contact/hand-held carry/controller throws remain disabled by default.

Validation: 63 host tests, retail ABI checks, portal/camera regression checks and
AddressSanitizer body/leg checks pass. CPU checks do not establish GPU stability.
Upgrade preserves existing configuration and creates backups. No game, driver,
recordings, private logs or generated retail models are distributed. Generate
owned assets with Install.py; keep your existing working Frame runtime setup.

This release contains the tested Audit-21 DLL only. New crosshair, contact and
customization requests are being developed separately.
