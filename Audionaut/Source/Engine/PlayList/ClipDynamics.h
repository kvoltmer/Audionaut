//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <cmath>
#include <vector>
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium {

class PlayListItem;

/**
 * \class ClipDynamics
 * \brief The dynamics of a playlist item: per-channel clip gain and the fades.
 *
 * Owned by a `PlayListItem`. Gain is stored per destination channel (an empty
 * vector means 1.0 everywhere); the fades are stored in clocks and clamped
 * against each other within the owner's region length. Serializes itself into
 * the owner's JSON.
 */
class ClipDynamics
{

public:

    explicit ClipDynamics(PlayListItem& owner);

    /** The fade curve shared by DSP and UI: sqrt of the linear 0..1 progress.
        Mirrors VolumeFade::nextCurveValue() (Engine/AudioSources/VolumeFade.h). */
    static double fadeCurve (double linearProgress) noexcept
    {
        return linearProgress > 0.0 ? std::sqrt (std::min (linearProgress, 1.0)) : 0.0;
    }

    // gain (linear range), per destination channel
    void setGain(int channel, double val, bool continous = false);
    double getGain(int channel) const;
    void onDeleteChannel(int channel);

    // gains only - split/clone don't inherit fades
    void copyGainsFrom(const ClipDynamics& other);

    // value range [0, 1]; returns true if other fade values were pushed
    bool setFadeIn(double val);
    double getFadeIn() const;

    // value range [0, 1]; returns true if other fade values were pushed
    bool setFadeOut(double val);
    double getFadeOut() const;

    // where the fade-in ramp begins, measured from the clip start; positive:
    // inside the clip, audio before it is silent. negative: outside the clip -
    // the fade extends the audible material with source audio from before the
    // region window (silence-padded past the file). value <= 1; returns true
    // if other fade values were pushed
    bool setFadeInStart(double val);
    double getFadeInStart() const;

    // where the fade-out ramp ends, measured from the clip end; positive:
    // inside the clip, audio after it is silent. negative: outside the clip -
    // the fade extends the audible material past the region window
    // (silence-padded past the file). value <= 1; returns true if other fade
    // values were pushed
    bool setFadeOutEnd(double val);
    double getFadeOutEnd() const;

    double getFadeInClocks() const
    {
        return fadeInClocks;
    }

    double getFadeOutClocks() const
    {
        return fadeOutClocks;
    }

    double getFadeInStartClocks() const
    {
        return fadeInStartClocks;
    }

    double getFadeOutEndClocks() const
    {
        return fadeOutEndClocks;
    }

    void writeToJson(json& output) const;
    void readFromJson(json& input);

private:

    double getRegionLengthClocks() const;

    PlayListItem& owner;

    // Gain per destination channel; empty => 1.0 for every channel
    std::vector<double> gains;

    double fadeInClocks = 0.0;
    double fadeOutClocks = 0.0;

    // ramp start/end offsets, anchored to their clip edge like the fades;
    // 0.0 means the fade touches the clip edge, negative means the ramp
    // boundary lies outside the clip (extending the audible material)
    double fadeInStartClocks = 0.0;
    double fadeOutEndClocks = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipDynamics)
};

} // namespace audium
