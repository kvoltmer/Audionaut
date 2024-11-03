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
#include "Engine/Resource/AudioResource.h"

#define MAX_AUDIO_FILE_CHANNELS 64

class AudioTrack;
class AudioResource;

class AudiumTransportSource : public juce::AudioSource
{
public:
    AudiumTransportSource(AudioResource& audioResource,
                          std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource) :
        audioResource(audioResource),
        audioFormatReaderSource(audioFormatReaderSource)
    {
        //channelRemapping = std::make_unique<juce::ChannelRemappingAudioSource>(this, false);
        
        audioTransportSource = std::make_shared<audium::AudioTransportSource>();
  
        // source
        auto readAheadBufferSize = 48000;
        auto readAheadThread = audioResource.getContainer().getReadAheadThread();
        auto memReader = dynamic_cast<MemoryMappedAudioFormatReader*>(audioFormatReaderSource->getAudioFormatReader());
        if (memReader)
        {
            readAheadBufferSize = 0;
            readAheadThread = nullptr;
        }

        audioTransportSource->setSource (audioFormatReaderSource.get(),
                                         readAheadBufferSize,
                                         readAheadThread,
                                         audioFormatReaderSource->getAudioFormatReader()->sampleRate);
        
    }
    
    ~AudiumTransportSource()
    {
        //setSource(nullptr);
        audioTransportSource->setSource(nullptr);
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        
        audioTransportSource->prepareToPlay(samplesPerBlockExpected, sampleRate);
        
//        // TODO: set the number of channels
//        channelRemapping->setNumberOfChannelsToProduce(8);
//
//        channelRemapping->prepareToPlay(samplesPerBlockExpected, sampleRate);
//
//        channelRemapping->setOutputChannelMapping(0, 0);
//        channelRemapping->setOutputChannelMapping(1, 1);
        
        
    }
    
    void releaseResources() override
    {
        audioTransportSource->releaseResources();
    }

    
    void schedulePosition (double newPosition, int startSample)
    {
        if (startSample == 0)
        {
            audioTransportSource->setPosition(newPosition);
            if (!audioTransportSource->isPlaying())
                audioTransportSource->start();
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
        audioTransportSource->stop();
    }
    
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        if (audioTransportSource->getBufferingSource() != nullptr &&
            audioTransportSource->getBufferingSource()->waitForNextAudioBlockReady(info, 2) == false)
        {
            std::cout << "waitForNextAudioBlockReady" << std::endl;
        }
        
        if (scheduledSample == 0)
        {
            audioTransportSource->getNextAudioBlock(info);
            
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
            audioTransportSource->getNextAudioBlock(infoPart1);
            
            audioTransportSource->setPosition(scheduledPosition.load());
            //std::cout << "scheduledPosition " << scheduledPosition.load() << std::endl;
         
            // workaround. TODO: re-implement juce transportsource
            if (not audioTransportSource->isPlaying())
                audioTransportSource->start();
            
            // process 2nd part
            juce::AudioSourceChannelInfo infoPart2 (info.buffer, startSample, info.numSamples - startSample);
            audioTransportSource->getNextAudioBlock(infoPart2);
        }
        
        // We need the number of channels of the actual file.
        auto numChannels = audioResource.getNumChannels();
        for (auto i = 0; i < numChannels; i++)
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
    
    AudioResource& getAudioResource() const {
        return audioResource;
    }
    
    //juce::ChannelRemappingAudioSource* getChannelRemapping() const { return channelRemapping.get(); }
    
    std::shared_ptr<audium::AudioTransportSource> getAudioTransportSource() const { return audioTransportSource; }
    
private:
    
    //typedef audium::AudioTransportSource tBase;
    
    AudioResource& audioResource;
    
    std::atomic<float> outputLevel[MAX_AUDIO_FILE_CHANNELS];
    
    // the sample position where the position change should happen
    std::atomic<int> scheduledSample = 0;
    // the scheduled position change
    std::atomic<double> scheduledPosition = 0.0;
    
    SampleTimer durationTimer;
    
    std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource;
    
    std::shared_ptr<audium::AudioTransportSource> audioTransportSource;
    
    std::unique_ptr<juce::ChannelRemappingAudioSource> channelRemapping;
    
};
