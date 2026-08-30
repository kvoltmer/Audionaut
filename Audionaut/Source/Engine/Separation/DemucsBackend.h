//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/Separation/SeparationBackend.h"

namespace audium {

/**
 * The Demucs (htdemucs, 4 stems) backend, running demucs.cpp on the CPU.
 *
 * Needs the ggml weights file - see DemucsModelStore - and takes minutes for
 * a full song: roughly real time per thread on a desktop CPU. Without
 * DEMUCS_ENABLED it compiles to a backend that reports itself unavailable.
 */
class DemucsBackend : public SeparationBackend
{
public:
    /**
     * @param modelFile   The ggml htdemucs 4-source weights.
     * @param numThreads  Segments to process in parallel; physical cores is
     *                    the sensible maximum.
     */
    DemucsBackend (juce::File modelFile, int numThreads);

    juce::String getName() const override { return "demucs"; }

    int getRequiredSampleRate() const override;

    bool isReady (juce::String& reason) const override;

    bool separate (const juce::AudioBuffer<float>& stereoInput,
                   std::vector<juce::AudioBuffer<float>>& stemsOut,
                   const SeparationProgress& progress,
                   juce::String& error) override;

    /// Whether this build carries the demucs.cpp code at all.
    static bool isCompiledIn();

private:
    juce::File modelFile;
    int numThreads;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DemucsBackend)
};

} // namespace audium
