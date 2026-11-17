# Owned-asset generation

Run with Python 3.11 or newer:

```text
python generate_owned_assets.py "<owned Portal installation>" "<new private output directory>"
```

The tool reads the retail Portal and HL2 VPK directories. It generates both
Chell VR hands with matching UVs and appends lower housing geometry to the
animated portal gun. It preserves the original install, animation data,
attachments and game saves. Only the verified retail model layout is accepted.

The output's `models` directory belongs under the private installation's
`portal/custom/portal1vr` directory. Include the matching VMT overrides from
`L4D2VR/frame-materials`. Install only after the user closes Portal.

The generated models and test fixtures contain derived game assets. A release
should provide these generators and material references and require an owned
Portal install. Keep generated output, retail VPK/MDL/VVD/VTX/VTF data, personal
logs, device credentials and recordings outside the source repository.

The format readers use the documented layouts in Valve's
[studio.h](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/public/studio.h)
and [optimize.h](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/public/optimize.h).
