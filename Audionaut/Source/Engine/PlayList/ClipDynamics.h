//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <cmath>
#include <vector>
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

#include "Engine/TimeContext.h"

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

    /** The default curve exponent: x^0.5 = sqrt = an equal-power fade. */
    static constexpr double defaultFadeCurve = 0.5;

    /** The curve exponents' legal range - the bend handle maps the curve's
        midpoint value into this. */
    static constexpr double minFadeCurve = 0.1;
    static constexpr double maxFadeCurve = 4.0;

    /** The fade curve shared by DSP and UI: the linear 0..1 progress raised
        to the fade's curve exponent (0.5 = sqrt = equal power, 1 = linear,
        > 1 = exponential). Mirrors VolumeFade::nextCurveValue()
        (Engine/AudioSources/VolumeFade.h). */
    static double fadeCurve (double linearProgress, double curve = defaultFadeCurve) noexcept
    {
        return linearProgress > 0.0 ? std::pow (std::min (linearProgress, 1.0), curve) : 0.0;
    }

    // gain (linear range), per destination channel
    void setGain(int channel, double val, bool continous = false);
    double getGain(int channel) const;
    void onDeleteChannel(int channel);

    // gains only - clones don't inherit fades; a split piece additionally
    // copies the fade of the edge it keeps via copyFadeInFrom/copyFadeOutFrom
    void copyGainsFrom(const ClipDynamics& other);

    // the head-side fade family (fade-in, its start offset and curve),
    // clamped to this clip's length - for the split piece keeping the
    // original clip's start edge
    void copyFadeInFrom(const ClipDynamics& other);

    // the tail-side fade family, mirrored - for the piece keeping the
    // original end edge
    void copyFadeOutFrom(const ClipDynamics& other);

    // the complete dynamics: gains AND all four fade values - for an export
    // item that must sound exactly like its source item
    void copyFrom(const ClipDynamics& other);

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

    // the fade values as absolute times in the requested context - unlike
    // the argument-less fraction accessors above, which are relative to the
    // region length
    double getFadeIn(audium::TimeContextType context) const;
    double getFadeOut(audium::TimeContextType context) const;
    double getFadeInStart(audium::TimeContextType context) const;
    double getFadeOutEnd(audium::TimeContextType context) const;

    // the curve exponent per fade, clamped to [minFadeCurve, maxFadeCurve];
    // defaultFadeCurve = the equal-power sqrt curve
    void setFadeInCurve(double curve);
    double getFadeInCurve() const { return fadeInCurve; }

    void setFadeOutCurve(double curve);
    double getFadeOutCurve() const { return fadeOutCurve; }

    void writeToJson(json& output) const;
    void readFromJson(json& input);

private:

    double getRegionLengthClocks() const;

    double clocksToContext(double clocks, audium::TimeContextType context) const;

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

    // curve exponents (x^curve over the ramp)
    double fadeInCurve = defaultFadeCurve;
    double fadeOutCurve = defaultFadeCurve;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipDynamics)
};

} // namespace audium
