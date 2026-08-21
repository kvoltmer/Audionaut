//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <JuceHeader.h>

#include "VolumeFade.h"

namespace audium {

/**
 * @class ClipDynamicsProcessor
 * @brief The per-clip dynamics rendering: clip gain plus the two fade ramps.
 *
 * Applied post-resampling at the device rate, in the order
 * gain -> fade-in -> fade-out. Extracted from AudioTransportSource and
 * injected into it, so the dynamics chain can be configured and exercised
 * on its own. All setters are real-time safe; the scheduler calls them from
 * the audio thread.
 */
class ClipDynamicsProcessor
{
public:
    ClipDynamicsProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        clipGain.prepare(spec);
        clipGain.setRampDurationSeconds(0.01);

        clipFadeIn.prepare(spec);
        clipFadeIn.setRampDurationSeconds(0.0);

        clipFadeOut.prepare(spec);
        clipFadeOut.setRampDurationSeconds(0.0);
    }

    /** Applies gain, fade-in and fade-out over the whole context. */
    void process (const juce::dsp::ProcessContextReplacing<float>& context)
    {
        clipGain.setGainLinear(gain.load());

        clipGain.process(context);
        clipFadeIn.process(context);
        clipFadeOut.process(context);
    }

    void setGain (const float newGain) noexcept   { gain = newGain; }
    float getGain() const noexcept                { return gain.load(); }

    /** Snaps the gain smoother to the target - without this a freshly
        configured clip swells in over the gain ramp duration. */
    void resetGain()
    {
        auto rampDuration = clipGain.getRampDurationSeconds();
        clipGain.setRampDurationSeconds(0.0);
        clipGain.setGainLinear(gain.load());
        clipGain.setRampDurationSeconds(rampDuration);
    }

    /** Transparent resets: no fade, no stale skip/silent counters. */
    void clearFadeIn()
    {
        clipFadeIn.setSilentSamples(0);
        clipFadeIn.setRampDurationSeconds(0.0);
        clipFadeIn.setGainLinear(1.0, true);
    }

    void clearFadeOut()
    {
        // no fade-out: keep the processor transparent instead of scheduling a
        // zero-length ramp to gain 0 (which would mute instantly once the
        // skip counter runs out)
        clipFadeOut.setSkipSamples(0);
        clipFadeOut.setRampDurationSeconds(0.0);
        clipFadeOut.setGainLinear(1.0, true);
    }

    /** The curve exponent applied over each ramp (ClipDynamics::fadeCurve). */
    void setFadeInCurve (double curve)
    {
        clipFadeIn.setCurveExponent(static_cast<float>(curve));
    }

    void setFadeOutCurve (double curve)
    {
        clipFadeOut.setCurveExponent(static_cast<float>(curve));
    }

    /** Configures the fade-in as a ramp of rampSeconds starting
        rampStartSeconds after the scheduled position. rampStartSeconds > 0
        emits silence until the ramp starts; <= 0 means |value| of the ramp
        has already elapsed (the ramp is armed mid-way). */
    void setFadeInRamp (double rampSeconds, double rampStartSeconds, bool reset)
    {
        auto rampTime = rampSeconds;
        auto initialGain = 0.0;

        if (rampStartSeconds > 0.0) {
            // silence until the ramp starts; the smoother is frozen meanwhile
            clipFadeIn.setSilentSamples(juce::roundToInt(rampStartSeconds * sampleRate));
        }
        else {
            // the ramp (partly) elapsed before the scheduled position - always
            // clear a stale silent span from a previous schedule
            clipFadeIn.setSilentSamples(0);

            auto elapsed = -rampStartSeconds;
            rampTime = juce::jmax(0.0, rampSeconds - elapsed);
            initialGain = rampSeconds > 0.0 ? juce::jmin(1.0, elapsed / rampSeconds) : 1.0;
        }

        // ordering is load-bearing: snap the initial gain, then the duration,
        // then arm the target (setRampDurationSeconds resets the smoother)
        if (reset)
            clipFadeIn.setGainLinear(initialGain, true);
        clipFadeIn.setRampDurationSeconds(rampTime);
        clipFadeIn.setGainLinear(1.0, false);
    }

    /** Configures the fade-out as a ramp of rampSeconds starting
        rampStartSeconds after the scheduled position; the completed ramp
        holds silence. rampSeconds <= 0 with a positive start is an instant
        mute at the ramp point. */
    void setFadeOutRamp (double rampSeconds, double rampStartSeconds, bool reset)
    {
        auto rampTime = rampSeconds;
        auto initialGain = 1.0;

        if (rampStartSeconds > 0.0) {
            // unity pass-through until the ramp starts
            clipFadeOut.setSkipSamples(juce::roundToInt(rampStartSeconds * sampleRate));
        }
        else {
            // the ramp (partly) elapsed before the scheduled position - always
            // clear a stale skip span from a previous schedule
            clipFadeOut.setSkipSamples(0);

            auto elapsed = -rampStartSeconds;
            rampTime = juce::jmax(0.0, rampSeconds - elapsed);
            initialGain = rampSeconds > 0.0 ? juce::jmax(0.0, rampTime / rampSeconds) : 0.0;
        }

        if (reset)
            clipFadeOut.setGainLinear(initialGain, true);
        clipFadeOut.setRampDurationSeconds(rampTime);
        clipFadeOut.setGainLinear(0.0, false);
    }

private:
    std::atomic<float> gain = 1.0f;
    double sampleRate = 0.0;

    juce::dsp::Gain<float> clipGain;
    audium::VolumeFade<float> clipFadeIn;
    audium::VolumeFade<float> clipFadeOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipDynamicsProcessor)
};

} // namespace audium
