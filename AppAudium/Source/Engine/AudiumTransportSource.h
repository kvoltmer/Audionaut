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
    
    void schedulePosition (double newPosition, int startSample)
    {
        if (startSample == 0)
        {
            setPosition(newPosition);
        }
        else
        {
            scheduledSample.store(startSample);
            scheduledPosition = newPosition;
        }
    }
    
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        if (scheduledSample == 0)
        {
            tBase::getNextAudioBlock(info);
        }
        else
        {
            auto startSample = scheduledSample.load();
            scheduledSample.store(0);
            
            // process 1st part
            juce::AudioSourceChannelInfo infoPart1 (info.buffer, 0, startSample);
            tBase::getNextAudioBlock(infoPart1);
            
            setPosition(scheduledPosition.load());
            //std::cout << "scheduledPosition " << scheduledPosition.load() << std::endl;
         
            // workaround. TODO: re-implement juce transportsource
            if (not isPlaying())
                start();
            
            // process 2nd part
            juce::AudioSourceChannelInfo infoPart2 (info.buffer, startSample, info.numSamples - startSample);
            tBase::getNextAudioBlock(infoPart2);
        }
        
        outputLevel = info.buffer->getMagnitude(0, info.startSample, info.numSamples);
    }
    
    float getOutputLevel() const { return outputLevel.load(); }
    
private:
    
    typedef juce::AudioTransportSource tBase;
    
    std::atomic<float> outputLevel;
    
    // the sample position where the position change should happen
    std::atomic<int> scheduledSample = 0;
    // the scheduled position change
    std::atomic<double> scheduledPosition = 0.0;
    
};
