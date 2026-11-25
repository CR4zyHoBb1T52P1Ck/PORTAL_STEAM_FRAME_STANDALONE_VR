#pragma once
// Portal native HUD is authored by the game (including commentary playback
// progress). Keep its real visibility/time data rather than estimating a timer.
// This changes main-eye flags only: it does not hook VGUI or draw portal HUDs.
namespace HudPolicy09 {
constexpr int DrawHud=1<<1;
inline int EyeFlags(int nativeFlags,bool enabled,bool menusVisible=false) {
    // Never force HUD into an engine view that deliberately omitted it.
    return enabled&&!menusVisible?nativeFlags:(nativeFlags&~DrawHud);
}
inline bool Crosshair(bool nativeWants,int aimMode,int crosshairStyle,
    bool interactionAllowed,bool weaponStowed,bool showGun) {
    // Exactly the accepted Portal crosshair gate. Enabling scene HUD must not
    // add a second reticle when the player selected the tracked VR dot/laser.
    const bool originalArtwork=crosshairStyle==4&&interactionAllowed&&!weaponStowed&&showGun;
    return nativeWants&&(aimMode==1||originalArtwork);
}
}
