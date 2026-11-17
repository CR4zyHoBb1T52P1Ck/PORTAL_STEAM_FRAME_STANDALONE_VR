#pragma once
namespace VrFocus {
inline bool OwnsInput(bool dashboard,bool inputAvailable,bool pause,bool sceneFocus) {
    return !dashboard&&inputAvailable&&!pause&&sceneFocus;
}
// Returning from Steam must not turn a held dashboard click into a shot or
// menu command. Require a fresh, neutral action sample before resuming input.
struct Gate {
    bool focused=true,neutralRequired=false;
    bool Allowed() const {return focused&&!neutralRequired;}
    void Focus(bool value) {focused=value;if(!value)neutralRequired=true;}
    void Sample(bool fresh,bool held) {if(focused&&fresh&&!held)neutralRequired=false;}
};
enum class Submission {None,Background,Eyes};
inline Submission SelectSubmission(bool canRender,bool eyeFrame,bool backgroundReady) {
    return !canRender?Submission::None:eyeFrame?Submission::Eyes:
        backgroundReady?Submission::Background:Submission::None;
}
}
