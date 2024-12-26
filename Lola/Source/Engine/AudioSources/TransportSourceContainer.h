/*
  ==============================================================================

    TransportSourceContainer.h
    Created: 14 Jun 2023 6:12:34pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

#include "Engine/Core/LockFreeContainer.h"

#define MAX_AUDIO_CHANNELS 64
#define MAX_TRANSPORT_SOURCES 512

class AudioResourceContainer;
class AudioTrack;
class AudiumTransportSource;
class AudioResource;

class TransportSourceContainer : public juce::AudioSource
{
public:
    TransportSourceContainer() = default;
    ~TransportSourceContainer() override = default;
    
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void prepareToPlay (int samplesPerBlockExpected,
                        double sampleRate) override;
    
    void releaseResources() override {
        cleanup();
    }

    std::shared_ptr<AudiumTransportSource> createAndAddTransportSource(AudioResource& audioResource,
                                                                       std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);
    bool removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource);
    
    void cleanup();
        
    void startPlaying();
    void stopPlaying();
    bool isPlaying() const;
    
    void setBypass(bool isByPass) { byPass = isByPass; }

    std::shared_ptr<AudiumTransportSource> getTransportSourceAtIndex(int index) const;
    int getTransportSourceIndex(std::shared_ptr<AudiumTransportSource> searchTransportSource) const;
    
    const float getOutputLevel(const int channelNumber) const;
     
    void applyChannelMapping();
    
    void commitTransportSources();
    
private:
    std::atomic<bool> playing = false;
    std::atomic<bool> byPass = false;
    
    audium::LockFreeContainer<AudiumTransportSource, MAX_TRANSPORT_SOURCES> audioTransportSources;
    
    juce::AudioBuffer<float> audioBusBuffer;
    
    std::atomic<float> outputLevel[MAX_AUDIO_CHANNELS];
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportSourceContainer)
};
