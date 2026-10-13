//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/Separation/DemucsBackend.h"
#include "Engine/Separation/DemucsConfig.h"

#if DEMUCS_ENABLED
 #include "Engine/Separation/Demucs/DemucsRunner.h"
#endif

namespace audium {

DemucsBackend::DemucsBackend (juce::File modelFile_, int numThreads_) :
    modelFile (std::move (modelFile_)),
    numThreads (juce::jmax (1, numThreads_))
{
}

bool DemucsBackend::isCompiledIn()
{
    return DEMUCS_ENABLED != 0;
}

int DemucsBackend::getRequiredSampleRate() const
{
#if DEMUCS_ENABLED
    return demucs::sampleRate;
#else
    return 44100;
#endif
}

bool DemucsBackend::isReady (juce::String& reason) const
{
    if (! isCompiledIn())
    {
        reason = "this build was made without Demucs stem separation";
        return false;
    }

    if (! modelFile.existsAsFile())
    {
        reason = "the Demucs model is not installed (expected at "
                 + modelFile.getFullPathName() + ")";
        return false;
    }

    return true;
}

bool DemucsBackend::separate (const juce::AudioBuffer<float>& stereoInput,
                              std::vector<juce::AudioBuffer<float>>& stemsOut,
                              const SeparationProgress& progress,
                              juce::String& error)
{
    stemsOut.clear();

    if (! isReady (error))
        return false;

    if (stereoInput.getNumChannels() != 2 || stereoInput.getNumSamples() == 0)
    {
        error = "the separator needs stereo audio";
        return false;
    }

#if DEMUCS_ENABLED
    demucs::ProgressFn forward;

    if (progress != nullptr)
        forward = [&progress] (float fraction, const std::string& message)
        {
            return progress (static_cast<double> (fraction), juce::String (message));
        };

    std::vector<std::vector<float>> stems;
    std::string demucsError;

    const auto ok = demucs::separate (modelFile.getFullPathName().toStdString(),
                                      stereoInput.getReadPointer (0),
                                      stereoInput.getReadPointer (1),
                                      static_cast<size_t> (stereoInput.getNumSamples()),
                                      numThreads,
                                      forward,
                                      stems,
                                      demucsError);

    if (! ok)
    {
        error = juce::String (demucsError);
        return false;
    }

    if (static_cast<int> (stems.size()) != numStems * 2)
    {
        error = "the separator returned an unexpected number of stems";
        return false;
    }

    const auto numSamples = stereoInput.getNumSamples();

    for (auto index = 0; index < numStems; ++index)
    {
        juce::AudioBuffer<float> stem (2, numSamples);

        for (auto channel = 0; channel < 2; ++channel)
        {
            const auto& samples = stems[static_cast<size_t> (index * 2 + channel)];
            jassert (static_cast<int> (samples.size()) == numSamples);
            stem.copyFrom (channel, 0, samples.data(), juce::jmin (numSamples, static_cast<int> (samples.size())));
        }

        stemsOut.push_back (std::move (stem));
    }

    return true;
#else
    juce::ignoreUnused (progress);
    return false;
#endif
}

} // namespace audium
