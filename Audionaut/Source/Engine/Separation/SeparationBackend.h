//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <vector>
#include <JuceHeader.h>

#include "Engine/Separation/SeparationTypes.h"

namespace audium {

/**
 * A stem separation engine: stereo mix in, one buffer per Stem out.
 *
 * The backend is pure signal processing. It never touches the project -
 * StemSeparator renders the clip, hands the audio over here and turns the
 * result into tracks - so it is safe to call from any thread, and a fake can
 * stand in for it in tests.
 */
class SeparationBackend
{
public:
    virtual ~SeparationBackend() = default;

    /// Shown in messages and logs.
    virtual juce::String getName() const = 0;

    /// The rate the input must be delivered at; the caller renders to it.
    virtual int getRequiredSampleRate() const = 0;

    /// Whether separate() can run right now. Fills @p reason when not, e.g.
    /// when the model still has to be downloaded.
    virtual bool isReady (juce::String& reason) const = 0;

    /**
     * Separates @p stereoInput into stems.
     *
     * @param stereoInput  Two channels at getRequiredSampleRate().
     * @param stemsOut     Receives numStems stereo buffers of the input's
     *                     length, in Stem order.
     * @param progress     Progress callback; return false to cancel. May be
     *                     empty.
     * @param error        Set on failure; left empty when cancelled.
     * @return             true on success.
     */
    virtual bool separate (const juce::AudioBuffer<float>& stereoInput,
                           std::vector<juce::AudioBuffer<float>>& stemsOut,
                           const SeparationProgress& progress,
                           juce::String& error) = 0;
};

} // namespace audium
