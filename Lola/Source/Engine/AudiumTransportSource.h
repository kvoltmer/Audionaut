/*
  ==============================================================================

    AudiumTransportSource.h
    Created: 6 Oct 2023 11:38:55am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

#include "Engine/PlayList/SampleTimer.h"
#include "Engine/Playback/audium_AudioTransportSource.h"

#define MAX_AUDIO_FILE_CHANNELS 64

class AudioGroup;

class AudiumTransportSource : public audium::AudioTransportSource
{
public:
    AudiumTransportSource(std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource) :
        audioFormatReaderSource(audioFormatReaderSource)
    {
    }
    
    ~AudiumTransportSource()
    {
        setSource(nullptr);
    }
    
    void schedulePosition (double newPosition, int startSample)
    {
        if (startSample == 0)
        {
            setPosition(newPosition);
            if (!isPlaying())
                start();
        }
        else
        {
            scheduledSample.store(startSample);
            scheduledPosition = newPosition;
        }
    }
    
    void scheduleDuration(double duration, double sr)
    {
        durationTimer.schedule(static_cast<int>(duration * sr));
    }
    
    void stopIt()
    {
        stop();
    }
    
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        if (getBufferingSource() != nullptr &&
            getBufferingSource()->waitForNextAudioBlockReady(info, 2) == false)
        {
            std::cout << "waitForNextAudioBlockReady" << std::endl;
        }
        
        if (scheduledSample == 0)
        {
            tBase::getNextAudioBlock(info);
            
            auto offset = 0;
            if (durationTimer.process(info.numSamples, offset))
            {
                stopIt();
            }
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
        
        for (auto i = 0; i < info.buffer->getNumChannels(); i++)
        {
            outputLevel[i] = info.buffer->getMagnitude(i, info.startSample, info.numSamples);
        }
    }
    
    float getOutputLevel(int channel) const
    {
        if (channel >= 0 &&
            channel < MAX_AUDIO_FILE_CHANNELS)
        {
            return outputLevel[channel].load();
        }
        return 0.f;
    }
    
private:
    
    typedef audium::AudioTransportSource tBase;
    
    std::atomic<float> outputLevel[MAX_AUDIO_FILE_CHANNELS];
    
    // the sample position where the position change should happen
    std::atomic<int> scheduledSample = 0;
    // the scheduled position change
    std::atomic<double> scheduledPosition = 0.0;
    
    SampleTimer durationTimer;
    
    std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource;
    
};
