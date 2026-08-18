//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium
{
/**
 * @class VolumeFade
 * @brief A utility class for applying smooth volume fades to audio signals.
 *
 * This template class provides functionality to apply gain adjustments with
 * optional smoothing (ramping) to avoid abrupt changes in volume. It supports
 * both single-sample and block-based processing.
 *
 * @tparam FloatType The floating-point type used for audio processing (e.g., float or double).
 */
template <typename FloatType>
class VolumeFade
{
public:
    VolumeFade() noexcept = default;
    
    /** Applies a new gain as a linear value. */
    void setGainLinear (FloatType newGain, bool reset) noexcept { reset ? gain.setCurrentAndTargetValue(newGain) : gain.setTargetValue (newGain); }
    
    /** Returns the current gain as a linear value. */
    FloatType getGainLinear() const noexcept                    { return gain.getTargetValue(); }
    
    /** Sets the length of the ramp used for smoothing gain changes. */
    void setRampDurationSeconds (double newDurationSeconds) noexcept
    {
        if (! juce::approximatelyEqual (rampDurationSeconds, newDurationSeconds))
        {
            rampDurationSeconds = newDurationSeconds;
            reset();
        }
    }
    
    /**
     * @brief Sets the number of samples to skip before applying gain changes.
     *
     * This method allows you to specify a number of samples to skip during processing.
     * While skipping, the gain changes will not be applied to the audio signal.
     *
     * @param numSamples The number of samples to skip.
     */
    void setSkipSamples(int numSamples) noexcept
    {
        skipSamples = numSamples;
    }
    
    /** Returns the ramp duration in seconds. */
    double getRampDurationSeconds() const noexcept              { return rampDurationSeconds; }
    
    /** Returns true if the current value is currently being interpolated. */
    bool isSmoothing() const noexcept                           { return gain.isSmoothing(); }
    
    /** Called before processing starts. */
    void prepare (const juce::dsp::ProcessSpec& spec) noexcept
    {
        sampleRate = spec.sampleRate;
        reset();
    }
    
    /** Resets the internal state of the gain */
    void reset() noexcept
    {
        if (sampleRate > 0)
            gain.reset (sampleRate, rampDurationSeconds);
    }
    
    /** Returns the result of processing a single sample. */
    template <typename SampleType>
    SampleType JUCE_VECTOR_CALLTYPE processSample (SampleType s) noexcept
    {
        return s * gain.getNextValue();
    }
    
    /** Processes the input and output buffers supplied in the processing context. */
    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        auto&& inBlock  = context.getInputBlock();
        auto&& outBlock = context.getOutputBlock();
        
        jassert (inBlock.getNumChannels() == outBlock.getNumChannels());
        jassert (inBlock.getNumSamples() == outBlock.getNumSamples());
        
        auto len         = inBlock.getNumSamples();
        auto numChannels = inBlock.getNumChannels();

        if (context.isBypassed)
        {
            gain.skip (static_cast<int> (len));

            if (skipSamples > 0)
                skipSamples -= static_cast<int> (len);

            if (context.usesSeparateInputAndOutputBlocks())
                outBlock.copyFrom (inBlock);

            return;
        }

        // Samples before the scheduled fade start pass through untouched; the
        // fade may begin in the middle of a block.
        const auto pending = static_cast<size_t> (juce::jlimit (0, static_cast<int> (len), skipSamples));
        if (pending > 0)
        {
            skipSamples -= static_cast<int> (pending);

            if (context.usesSeparateInputAndOutputBlocks())
                for (size_t chan = 0; chan < numChannels; ++chan)
                    juce::FloatVectorOperations::copy (outBlock.getChannelPointer (chan),
                                                       inBlock.getChannelPointer (chan),
                                                       static_cast<int> (pending));

            if (pending == len)
                return;
        }

        const auto numToProcess = len - pending;

        if (numChannels == 1)
        {
            auto* src = inBlock.getChannelPointer (0) + pending;
            auto* dst = outBlock.getChannelPointer (0) + pending;

            for (size_t i = 0; i < numToProcess; ++i)
                dst[i] = src[i] * nextCurveValue();
        }
        else
        {
            JUCE_BEGIN_IGNORE_WARNINGS_MSVC (6255 6386)
            auto* gains = static_cast<FloatType*> (alloca (sizeof (FloatType) * numToProcess));

            for (size_t i = 0; i < numToProcess; ++i)
                gains[i] = nextCurveValue();
            JUCE_END_IGNORE_WARNINGS_MSVC

            for (size_t chan = 0; chan < numChannels; ++chan)
                juce::FloatVectorOperations::multiply (outBlock.getChannelPointer (chan) + pending,
                                                 inBlock.getChannelPointer (chan) + pending,
                                                 gains, static_cast<int> (numToProcess));
        }
    }
    
private:
    /** The fade curve: sqrt of the linear ramp. The smoothed value can drift
        slightly below zero through float accumulation over long ramps -
        unclamped, sqrt/pow would turn that into NaN and poison the mix.
        Must stay in sync with ClipDynamics::fadeCurve, which the UI uses to
        draw the same curve. */
    FloatType nextCurveValue() noexcept
    {
        auto g = gain.getNextValue();
        return g > FloatType (0) ? std::sqrt (g) : FloatType (0);
    }

    juce::SmoothedValue<FloatType> gain;
    double sampleRate = 0, rampDurationSeconds = 0;
    int skipSamples = 0;
};

} // namespace audium

