/*
  ==============================================================================

    AudiumTransportSource.h
    Created: 6 Oct 2023 11:38:55am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class AudioGroup;

class AudiumTransportSource : public juce::AudioTransportSource
{
public:
    AudiumTransportSource() = default;
    ~AudiumTransportSource()
    {
        setSource(nullptr);
    }
    
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        tBase::getNextAudioBlock(info);
        
        outputLevel = info.buffer->getMagnitude(0, info.startSample, info.numSamples);
    }
    
    float getOutputLevel() const { return outputLevel.load(); }
    
private:
    
    typedef juce::AudioTransportSource tBase;
    
    std::atomic<float> outputLevel;
    
    
};
