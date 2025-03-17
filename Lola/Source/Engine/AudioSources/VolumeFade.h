//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <JuceHeader.h>

namespace audium
{

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
        
        if (skipSamples > 0)
        {
            if (skipSamples > 0)
                skipSamples -= len;
            
            if (context.usesSeparateInputAndOutputBlocks())
                outBlock.copyFrom (inBlock);
            
            return;
        }
        
        if (context.isBypassed)
        {
            gain.skip (static_cast<int> (len));
            
            if (skipSamples > 0)
                skipSamples -= len;
            
            if (context.usesSeparateInputAndOutputBlocks())
                outBlock.copyFrom (inBlock);
            
            return;
        }
        
        if (numChannels == 1)
        {
            auto* src = inBlock.getChannelPointer (0);
            auto* dst = outBlock.getChannelPointer (0);
            
            for (size_t i = 0; i < len; ++i)
                dst[i] = src[i] * pow(gain.getNextValue(), 0.5f);
        }
        else
        {
            JUCE_BEGIN_IGNORE_WARNINGS_MSVC (6255 6386)
            auto* gains = static_cast<FloatType*> (alloca (sizeof (FloatType) * len));

            for (size_t i = 0; i < len; ++i)
                gains[i] = pow(gain.getNextValue(), 0.5f);
            JUCE_END_IGNORE_WARNINGS_MSVC

            for (size_t chan = 0; chan < numChannels; ++chan)
                juce::FloatVectorOperations::multiply (outBlock.getChannelPointer (chan),
                                                 inBlock.getChannelPointer (chan),
                                                 gains, static_cast<int> (len));
        }
    }
    
private:
    juce::SmoothedValue<FloatType> gain;
    double sampleRate = 0, rampDurationSeconds = 0;
    int skipSamples = 0;
};

} // namespace audium

